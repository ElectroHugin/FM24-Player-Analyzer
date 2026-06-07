# data_parser.py

import pandas as pd
from bs4 import BeautifulSoup
from constants import (attribute_mapping, get_valid_roles, ROLE_ANALYSIS_COLUMNS, 
                     PLAYER_ROLE_MATRIX_COLUMNS, )
from sqlite_db import (init_db, get_all_players, get_latest_dwrs_ratings, 
                       bulk_upsert_players, create_database_backup, merge_player_records)

import streamlit as st

REQUIRED_COLUMN = "UID"

def parse_html_table(file):
    try:
        # Support both file-like objects (Streamlit uploads) and raw bytes/strings
        raw = file.read()
        if isinstance(raw, bytes):
            content = raw.decode('utf-8', errors='replace')
        else:
            content = raw

        soup = BeautifulSoup(content, 'html.parser')
        table = soup.find('table')
        if not table:
            print("parse_html_table: No <table> element found in file.")
            return None

        header_row = table.find('tr')
        if not header_row:
            print("parse_html_table: No header row found in table.")
            return None

        headers = [th.text.strip() for th in header_row.find_all('th')]
        if not headers:
            print("parse_html_table: No <th> elements found in header row.")
            return None
        if len(headers) < 10:
            print(f"parse_html_table: Too few columns ({len(headers)}), expected at least 10.")
            return None
        if REQUIRED_COLUMN not in headers:
            print(f"parse_html_table: Required column '{REQUIRED_COLUMN}' not found. Found: {headers[:10]}")
            return None

        # --- Deduplicate column headers here, before building the DataFrame ---
        # This prevents issues downstream when pandas silently picks the wrong
        # column when two columns share the same name (e.g. FM exports with a
        # duplicate 'Name' column from certain custom views).
        seen = {}
        deduped_headers = []
        for h in headers:
            if h in seen:
                seen[h] += 1
                deduped_headers.append(f"{h}_dup{seen[h]}")
            else:
                seen[h] = 0
                deduped_headers.append(h)

        rows = [[td.text.strip() for td in tr.find_all('td')] for tr in table.find_all('tr')[1:]]
        valid_rows = [row for row in rows if len(row) == len(headers)]
        if not valid_rows:
            print(f"parse_html_table: No valid rows found. Total rows: {len(rows)}, expected row length: {len(headers)}.")
            return None

        df = pd.DataFrame(valid_rows, columns=deduped_headers)

        # Drop any columns that were renamed as duplicates (e.g. Name_dup1)
        dup_cols = [c for c in df.columns if '_dup' in c]
        if dup_cols:
            print(f"parse_html_table: Dropping duplicate columns: {dup_cols}")
            df = df.drop(columns=dup_cols)

        return df

    except Exception as e:
        print(f"parse_html_table: Unexpected error: {e}")
        return None

def load_data():
    init_db()
    players = get_all_players()
    return pd.DataFrame(players) if players else None


def parse_and_update_data(file):
    
    create_database_backup()

    html_df = parse_html_table(file)
    if html_df is None:
        st.error(
            "❌ **Could not parse the HTML file.** Possible causes:\n"
            "- The file has no `<table>` or the table has no `UID` column.\n"
            "- The file is corrupt or not a valid Football Manager HTML export.\n"
            "- Check the terminal/logs for a detailed error message from `parse_html_table`."
        )
        return None, []

    # Warn the user if the FM view exported unexpected extra columns.
    # (Deduplication already happened inside parse_html_table.)
    known_fm_columns = set(attribute_mapping.keys()) | {'UID'}
    extra_cols = [c for c in html_df.columns if c not in known_fm_columns and '_dup' not in c]
    if extra_cols:
        st.warning(
            f"⚠️ **Unknown columns in export** (will be ignored): `{', '.join(extra_cols)}`\n\n"
            "These columns are not part of the expected FM view. "
            "Consider removing them from your FM view to keep exports clean."
        )

    try:
        html_df['UID'] = html_df['UID'].astype(str).str.strip()
        html_df['Name'] = html_df['Name'].astype(str).str.strip()
    except KeyError as e:
        st.error(
            f"❌ **Missing required column: `{e}`**\n\n"
            "The HTML file must contain both a `UID` and a `Name` column. "
            "Please check your Football Manager export view."
        )
        return None, []

    # --- START OF THE DEFINITIVE UNIFICATION ENGINE ---
    
    # 2. Fetch existing player data to build lookup maps
    existing_players = get_all_players()
    # Map for quick lookup: { 'NumericID': 'PlayerName' }
    numeric_id_to_name = {p['Unique ID']: p['Name'] for p in existing_players if not p['Unique ID'].startswith('r-')}
    # Map for quick lookup: { 'PlayerName': 'r-ID' } -> The canonical mapping
    name_to_r_id = {p['Name']: p['Unique ID'] for p in existing_players if p['Unique ID'].startswith('r-')}
    
    # 3. Iterate through incoming data to find and fix inconsistencies BEFORE saving
    for index, row in html_df.iterrows():
        incoming_id = row['UID']
        incoming_name = row['Name']
        
        # SCENARIO 1: "Mistaken Identity"
        # The game exported a numeric ID for a player who we know is a newgen.
        if not incoming_id.startswith('r-') and incoming_name in name_to_r_id:
            correct_id = name_to_r_id[incoming_name]
            # Silently correct the ID in the DataFrame before it ever touches the database.
            html_df.loc[index, 'UID'] = correct_id
            
        # SCENARIO 2: "Data Corruption"
        # The game exported the correct r-ID, but our database has a corrupted numeric entry.
        if incoming_id.startswith('r-'):
            numeric_part = incoming_id[2:]
            # Check if a player exists with the numeric part of the ID AND the same name.
            if numeric_part in numeric_id_to_name and numeric_id_to_name[numeric_part] == incoming_name:
                # We found a corrupted record. Use our migration tool to fix it permanently.
                merge_player_records(bad_id=numeric_part, good_id=incoming_id)
                # After merging, the bad numeric ID is gone from our lookup, preventing future errors.
                del numeric_id_to_name[numeric_part]

    # 4. Get the final list of UIDs after all corrections and unifications.
    affected_ids = list(html_df['UID'].unique())
    
    # --- END OF THE UNIFICATION ENGINE ---

    # 5. Prepare and save the now-clean data (no changes from here on)
    html_df.rename(columns=attribute_mapping, inplace=True)
    known_columns = set(attribute_mapping.values())
    cols_to_keep = [col for col in html_df.columns if col in known_columns or col == 'Unique ID']
    df_filtered = html_df[cols_to_keep]
    APP_MANAGED_COLUMNS = ["Assigned Roles", "primary_role", "natural_positions", "transfer_status", "loan_status"]
    cols_from_html = [col for col in df_filtered.columns if col not in APP_MANAGED_COLUMNS]
    df_final = df_filtered[cols_from_html]
    players_to_upsert = df_final.to_dict('records')

    if players_to_upsert:
        bulk_upsert_players(players_to_upsert)

    return load_data(), affected_ids

def get_filtered_players(filter_option="Unassigned Players", club_filter="All", position_filter="All", sort_column="Name", sort_ascending=True, user_club=None):
    players = get_all_players()
    if not players: return pd.DataFrame()
    df = pd.DataFrame(players)
    if filter_option == "Unassigned Players":
        df = df[df['Assigned Roles'].apply(lambda x: not x)]
    elif filter_option == "Players Not From My Club" and user_club:
        df = df[df['Club'] != user_club]
    elif filter_option == "Unassigned Players Not From My Club" and user_club:
        df = df[(df['Club'] != user_club) & (df['Assigned Roles'].apply(lambda x: not x))]
    if club_filter != "All": df = df[df['Club'] == club_filter]
    if position_filter != "All": df = df[df['Position'] == position_filter]

    # Check if the user wants to sort by player name
    if sort_column == "Name":
        # Helper function to extract the last name
        def get_last_name(full_name):
            if isinstance(full_name, str) and full_name:
                return full_name.split(' ')[-1]
            return ""
        
        # Create a temporary column for the last name, sort by it, then drop it
        df['LastName'] = df['Name'].apply(get_last_name)
        return df.sort_values(by=['LastName', 'Name'], ascending=sort_ascending).drop(columns=['LastName'])
    else:
        # For all other columns, sort normally
        return df.sort_values(by=sort_column, ascending=sort_ascending)

@st.cache_data
def get_players_by_role(role, user_club, second_team_club=None):
    # Import the new, fast data loader
    from sqlite_db import get_latest_dwrs_ratings

    players = get_all_players()
    empty_df = pd.DataFrame(columns=ROLE_ANALYSIS_COLUMNS)
    if not players: return empty_df, empty_df, empty_df

    # 1. Get ALL pre-calculated ratings in one go. This is cached and fast.
    all_ratings = get_latest_dwrs_ratings()
    ratings_for_role = all_ratings.get(role, {}) # Get ratings just for the role we want

    # 2. Filter players who can play the role and have a rating for it.
    players_with_role = []
    for p in players:
        if role in p.get('Assigned Roles', []) and p['Unique ID'] in ratings_for_role:
            player_data = p.copy()

            absolute_val, normalized_str = ratings_for_role[p['Unique ID']]
            
            # Add both columns to the player data
            player_data['DWRS Rating (Absolute)'] = absolute_val
            player_data['DWRS Rating (Normalized)'] = normalized_str
            
            # We still need a numeric version for sorting
            try:
                player_data['DWRS_Sort_Value'] = int(float(normalized_str.rstrip('%')))
            except (ValueError, TypeError):
                player_data['DWRS_Sort_Value'] = 0 # Default if data is corrupted
            players_with_role.append(player_data)

    if not players_with_role: return empty_df, empty_df, empty_df
    
    df = pd.DataFrame(players_with_role)

    # 3. Split the DataFrame by club
    my_club_df = df[df['Club'] == user_club]
    second_team_df = df[df['Club'] == second_team_club] if second_team_club else pd.DataFrame()
    exclude_clubs = [user_club, second_team_club] if second_team_club else [user_club]
    scouted_df = df[~df['Club'].isin(exclude_clubs)]

    final_dfs = []
    for temp_df in [my_club_df, second_team_df, scouted_df]:
        if not temp_df.empty:
            # Sort by the numeric rating first, then format the display column
            temp_df = temp_df.sort_values(by='DWRS_Sort_Value', ascending=False)
            final_dfs.append(temp_df.reindex(columns=ROLE_ANALYSIS_COLUMNS))
        else:
            final_dfs.append(empty_df)
            
    return final_dfs

@st.cache_data
def get_player_role_matrix(user_club=None, second_team_club=None):
    # This now uses the same fast, reliable data source as get_players_by_role.
    
    players = get_all_players()
    if not players:
        return pd.DataFrame()

    # 1. Get ALL pre-calculated ratings in one go. This is cached and fast.
    all_ratings = get_latest_dwrs_ratings()
    
    matrix_data = []
    for player in players:
        # Start with the basic player info
        row = {col: player.get(col, '') for col in PLAYER_ROLE_MATRIX_COLUMNS}
        
        # Now, fill in the ratings for each role by looking them up
        for role in get_valid_roles():
            # Safely get the rating tuple (abs, norm) for the player and role
            rating_tuple = all_ratings.get(role, {}).get(player['Unique ID'])
            
            if rating_tuple:
                try:
                    # We only need the normalized string from the tuple
                    _absolute_val, normalized_str = rating_tuple
                    # Convert the percentage string to a number for display
                    row[role] = int(float(normalized_str.rstrip('%')))
                except (ValueError, AttributeError, TypeError):
                    row[role] = None # Handle potential bad data
            else:
                row[role] = None # Player doesn't have a rating for this role
        
        matrix_data.append(row)

    return pd.DataFrame(matrix_data)