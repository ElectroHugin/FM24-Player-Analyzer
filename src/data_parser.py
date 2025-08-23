# data_parser.py

import pandas as pd
from bs4 import BeautifulSoup
from constants import (attribute_mapping, get_valid_roles, ROLE_ANALYSIS_COLUMNS, 
                     PLAYER_ROLE_MATRIX_COLUMNS, WEIGHT_DEFAULTS, GK_WEIGHT_DEFAULTS)
from sqlite_db import init_db, update_player, get_all_players
from analytics import calculate_dwrs
from config_handler import get_weight
import streamlit as st

REQUIRED_COLUMN = "UID"

def parse_html_table(file):
    try:
        content = file.read().decode('utf-8')
        soup = BeautifulSoup(content, 'html.parser')
        table = soup.find('table')
        if not table: return None
        # Find the first row of the table specifically, and get headers from it.
        # This is more robust than searching the whole table for <th> tags.
        header_row = table.find('tr')
        if not header_row: return None
        headers = [th.text.strip() for th in header_row.find_all('th')]
        if not headers or REQUIRED_COLUMN not in headers or len(headers) < 10: return None
        
        rows = [[td.text.strip() for td in tr.find_all('td')] for tr in table.find_all('tr')[1:]]
        valid_rows = [row for row in rows if len(row) == len(headers)]
        if not valid_rows: return None
        
        return pd.DataFrame(valid_rows, columns=headers)
    except Exception as e:
        print(f"Error reading HTML file: {e}")
        return None

def load_data():
    init_db()
    players = get_all_players()
    return pd.DataFrame(players) if players else None

def parse_and_update_data(file):
    html_df = parse_html_table(file)
    if html_df is None: return None
    init_db()
# --- NEW ROBUST LOGIC ---
    # Create a set of all the column names our application officially recognizes from constants.py.
    # This is our "master list" of which columns are safe to import.
    known_columns = set(attribute_mapping.values())

    for _, row in html_df.iterrows():
        player_data = row.to_dict()

        # Step 1: Map the columns from the HTML file using the abbreviations.
        # This will produce keys like "Pace", "Acceleration", but also "Days Old", "Ability", etc.
        mapped_player_data = {attribute_mapping.get(k, k): v for k, v in player_data.items()}

        # Step 2: Filter the data. We create a new dictionary that ONLY includes
        # the key-value pairs where the key is in our "master list" (known_columns).
        # This safely discards "Days Old", "Potential", and any other unknown columns.
        final_player_data = {k: v for k, v in mapped_player_data.items() if k in known_columns}

        # Step 3: We only try to save the player to the database if a valid Unique ID exists.
        # This prevents errors from corrupt rows in the file.
        if 'Unique ID' in final_player_data and final_player_data['Unique ID']:
            update_player(final_player_data)

    # --- END NEW LOGIC ---

    return load_data()

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
    players = get_all_players()
    empty_df = pd.DataFrame(columns=ROLE_ANALYSIS_COLUMNS)
    if not players: return empty_df, empty_df, empty_df

    players_with_role = [p for p in players if role in p.get('Assigned Roles', [])]
    if not players_with_role: return empty_df, empty_df, empty_df
    
    df = pd.DataFrame(players_with_role)

    # --- START REFACTOR ---

    # Define weights once, outside the helper function, for efficiency
    weights = {cat: get_weight(cat.lower().replace(" ", "_"), default) for cat, default in WEIGHT_DEFAULTS.items()}
    gk_weights = {cat: get_weight("gk_" + cat.lower().replace(" ", "_"), default) for cat, default in GK_WEIGHT_DEFAULTS.items()}
    all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]
    weights_to_use = gk_weights if role in all_gk_roles else weights

    # Helper function to process a single row (player)
    def calculate_dwrs_for_row(player):
        absolute, normalized_str = calculate_dwrs(player.to_dict(), role, weights_to_use)
        normalized_val = float(normalized_str.rstrip('%'))
        return pd.Series([absolute, normalized_val], index=['DWRS Rating (Absolute)', 'DWRS Rating (Normalized)'])

    # Apply the helper function to each row. This returns a new DataFrame with the calculated columns.
    dwrs_ratings_df = df.apply(calculate_dwrs_for_row, axis=1)

    # Join the new ratings back to the original DataFrame
    df = df.join(dwrs_ratings_df)

    # --- END REFACTOR ---

    my_club_df = df[df['Club'] == user_club]
    second_team_df = df[df['Club'] == second_team_club] if second_team_club else pd.DataFrame()
    exclude_clubs = [user_club, second_team_club] if second_team_club else [user_club]
    scouted_df = df[~df['Club'].isin(exclude_clubs)]

    final_dfs = []
    for temp_df in [my_club_df, second_team_df, scouted_df]:
        if not temp_df.empty:
            # Sort by the numeric rating first
            temp_df = temp_df.sort_values(by='DWRS Rating (Normalized)', ascending=False)
            # Then format it as a string for display
            temp_df['DWRS Rating (Normalized)'] = temp_df['DWRS Rating (Normalized)'].apply(lambda x: f"{int(x)}%")
            # Ensure the final columns are in the correct order
            final_dfs.append(temp_df.reindex(columns=ROLE_ANALYSIS_COLUMNS))
        else:
            final_dfs.append(empty_df)
            
    return final_dfs

@st.cache_data
def get_player_role_matrix(user_club, second_team_club=None):
    players = get_all_players()
    if not players: return pd.DataFrame()
    df_players = pd.DataFrame(players)
    if df_players.empty: return pd.DataFrame()

    weights = {cat: get_weight(cat.lower().replace(" ", "_"), default) for cat, default in WEIGHT_DEFAULTS.items()}
    gk_weights = {cat: get_weight("gk_" + cat.lower().replace(" ", "_"), default) for cat, default in GK_WEIGHT_DEFAULTS.items()}
    all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]

    # --- START REFACTOR ---

    # 1. Define a helper function that processes one row (a player)
    def calculate_player_roles(player):
        row = {col: player.get(col, '') for col in PLAYER_ROLE_MATRIX_COLUMNS}
        assigned_roles = player.get('Assigned Roles', [])
        
        for role in get_valid_roles():
            if role in assigned_roles:
                weights_to_use = gk_weights if role in all_gk_roles else weights
                #_, normalized_str = calculate_dwrs(player, role, weights_to_use)
                _, normalized_str = calculate_dwrs(player.to_dict(), role, weights_to_use)
                try:
                    row[role] = int(normalized_str.rstrip('%'))
                except (ValueError, AttributeError):
                    row[role] = None
            else:
                row[role] = None
        return pd.Series(row)

    # 2. Apply this function to every row of the DataFrame
    matrix_df = df_players.apply(calculate_player_roles, axis=1)

    # --- END REFACTOR ---
    
    return matrix_df