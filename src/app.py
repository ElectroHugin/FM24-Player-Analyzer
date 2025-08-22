# app.py

import streamlit as st
import pandas as pd
import re
from io import StringIO
import csv
import urllib.parse
from data_parser import load_data, parse_and_update_data, get_filtered_players, get_players_by_role, get_player_role_matrix
from sqlite_db import (update_player_roles, get_user_club, set_user_club, update_dwrs_ratings, get_all_players, 
                     get_dwrs_history, get_second_team_club, set_second_team_club, update_player_club, set_primary_role, 
                     update_player_apt, get_favorite_tactics, set_favorite_tactics, 
                     update_player_transfer_status, update_player_loan_status)
from constants import (CSS_STYLES, SORTABLE_COLUMNS, FILTER_OPTIONS, PLAYER_ROLE_MATRIX_COLUMNS,
                     get_player_roles, get_valid_roles, get_position_to_role_mapping, 
                     get_tactic_roles, get_tactic_layouts, FIELD_PLAYER_APT_OPTIONS, GK_APT_OPTIONS)
from config_handler import (get_weight, set_weight, get_role_multiplier, set_role_multiplier, 
                          get_db_name, set_db_name, get_apt_weight, set_apt_weight)
from definitions_handler import get_definitions, save_definitions

st.set_page_config(page_title="FM 2024 Player Dashboard", layout="wide")
st.markdown(CSS_STYLES, unsafe_allow_html=True)

def clear_all_caches():
    st.cache_data.clear()

def get_last_name(full_name):
    """Extracts the last name from a full name string."""
    if isinstance(full_name, str) and full_name:
        return full_name.split(' ')[-1]
    return ""

def get_role_display_map():
    player_roles = get_player_roles()
    return {role: name for category in player_roles.values() for role, name in category.items()}

def format_role_display(role_abbr):
    return get_role_display_map().get(role_abbr, role_abbr)

def format_role_display_with_all(role_abbr):
    return "All Roles" if role_abbr == "All Roles" else get_role_display_map().get(role_abbr, role_abbr)

def _calculate_squad_and_surplus(my_club_players, positions, master_role_ratings):
    """
    A single, reliable function to calculate the Starting XI, B-Team, Depth, and Surplus players.
    This version includes special logic for goalkeepers and is corrected to prevent TypeErrors.
    """
    def select_team(team_positions, available_players):
        team = {}
        players_team = set()
        while len(team) < len(team_positions):
            best_candidate_for_each_pos = {}
            empty_positions = [p for p in team_positions if p not in team]
            for pos in empty_positions:
                role = team_positions[pos]
                best_candidate = None
                max_score = -1
                for player in available_players:
                    if player['Unique ID'] in players_team: continue
                    primary_role = player.get('primary_role')
                    if primary_role and primary_role != role: continue
                    rating = master_role_ratings.get(role, {}).get(player['Unique ID'], 0)
                    if rating > 0:
                        apt = player.get("Agreed Playing Time", "None") or "None"
                        apt_weight = get_apt_weight(apt)
                        selection_score = rating * apt_weight
                        if selection_score > max_score:
                            max_score = selection_score
                            best_candidate = { "player_id": player['Unique ID'], "player_name": player['Name'], "player_apt": apt, "rating": rating, "selection_score": selection_score, "position": pos }
                if best_candidate: best_candidate_for_each_pos[pos] = best_candidate
            if not best_candidate_for_each_pos: break 
            weakest_link_pos = min(best_candidate_for_each_pos, key=lambda p: best_candidate_for_each_pos[p]['selection_score'])
            winner = best_candidate_for_each_pos[weakest_link_pos]
            team[weakest_link_pos] = { "name": winner['player_name'], "rating": f"{int(winner['rating'])}%", "apt": winner['player_apt'] }
            players_team.add(winner['player_id'])
            available_players = [p for p in available_players if p['Unique ID'] != winner['player_id']]
        return team, available_players

    # --- Main Logic Execution ---
    starting_xi, remaining_players = select_team(positions, my_club_players)
    b_team, depth_pool = select_team(positions, remaining_players)
    
    default_player = {"name": "-", "rating": "0%", "apt": ""}
    for pos in positions:
        if pos not in starting_xi: starting_xi[pos] = default_player.copy()
        if pos not in b_team: b_team[pos] = default_player.copy()

    players_xi_or_b_team = {p['Unique ID'] for p in my_club_players if p not in depth_pool}
    
    best_depth_options = {}
    depth_player_ids = set()
    if depth_pool:
        for pos, role in positions.items():
            if pos == 'GK':
                gks_depth = [p for p in depth_pool if 'GK' in p.get('Position', '')]
                sorted_gks = sorted(gks_depth, key=lambda p: master_role_ratings.get(role, {}).get(p['Unique ID'], -1), reverse=True)
                top_depth_gks = sorted_gks[:2] 
                if top_depth_gks:
                    best_depth_options[pos] = []
                    for gk in top_depth_gks:
                        rating = master_role_ratings.get(role, {}).get(gk['Unique ID'], 0)
                        if rating > 0:
                            best_depth_options[pos].append({ "name": gk['Name'], "rating": f"{int(rating)}%", "apt": gk.get("Agreed Playing Time", "") or "" })
                            depth_player_ids.add(gk['Unique ID'])
            else:
                outfielders_depth = [p for p in depth_pool if 'GK' not in p.get('Position', '')]
                if outfielders_depth:
                    best_candidate = max(outfielders_depth, key=lambda p: master_role_ratings.get(role, {}).get(p['Unique ID'], -1), default=None)
                    if best_candidate:
                        rating = master_role_ratings.get(role, {}).get(best_candidate['Unique ID'], 0)
                        if rating > 0:
                            best_depth_options[pos] = [{ "name": best_candidate['Name'], "rating": f"{int(rating)}%", "apt": best_candidate.get("Agreed Playing Time", "") or "" }]
                            depth_player_ids.add(best_candidate['Unique ID'])

    players_with_a_role_ids = players_xi_or_b_team.union(depth_player_ids)
    surplus_players = [p for p in my_club_players if p['Unique ID'] not in players_with_a_role_ids]

    youth_surplus = []
    senior_surplus = []
    for player in surplus_players:
        age_str = player.get('Age')
        age = int(age_str) if age_str and age_str.isdigit() else 99
        is_gk = 'GK' in player.get('Position', '')
        if is_gk:
            if age <= 25: youth_surplus.append(player)
            else: senior_surplus.append(player)
        else:
            if age <= 20: youth_surplus.append(player)
            else: senior_surplus.append(player)

    youth_surplus.sort(key=lambda p: get_last_name(p['Name']))
    senior_surplus.sort(key=lambda p: get_last_name(p['Name']))
    
    return {
        "starting_xi": starting_xi, "b_team": b_team, "best_depth_options": best_depth_options,
        "surplus_players": surplus_players, "youth_surplus": youth_surplus, "senior_surplus": senior_surplus
    }

def sidebar():
    with st.sidebar:
        st.header("Navigation")
        page_options = ["All Players", "Assign Roles", "Role Analysis", "Player-Role Matrix", "Best Position Calculator", "Transfer & Loan Management", "Player Comparison", "DWRS Progress", "Edit Player Data", "Create New Role", "Settings"]
        page = st.radio("Go to", page_options)
        uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
        df = load_data()
        club_options = ["Select a club"] + sorted(df['Club'].unique()) if df is not None else ["Select a club"]
        current_club = get_user_club() or "Select a club"
                # --- ADD THIS BUTTON ---
        if st.button("🚨 Clear Cache & Rerun"):
            clear_all_caches()
            st.rerun()
        # --- END OF BUTTON CODE ---
        # --- NEW ROBUST LOGIC ---
        # Check if the current club from settings is actually in the options from the (potentially stale) cache.
        # If not, default the index to 0 ("Select a club") to prevent a crash.
        club_index = 0
        if current_club in club_options:
            club_index = club_options.index(current_club)
        
        selected_club = st.selectbox("Your Club", options=club_options, index=club_index)
        # --- END ROBUST LOGIC ---

        if selected_club != current_club and selected_club != "Select a club":
            set_user_club(selected_club)
            st.rerun()
        current_second = get_second_team_club() or "Select a club"
        selected_second = st.selectbox("Your Second Team", options=club_options, index=club_options.index(current_second))
        if selected_second != current_second and selected_second != "Select a club":
            set_second_team_club(selected_second)
            st.rerun()
        return page, uploaded_file

def main_page(uploaded_file):
    st.title("Player Dashboard")
    if uploaded_file:
        with st.spinner("Processing file..."):
            df = parse_and_update_data(uploaded_file)
        if df is None:
            st.error("Invalid HTML file: Must contain a table with a 'UID' column.")
            return
        clear_all_caches()
        update_dwrs_ratings(df, get_valid_roles())
        st.success("Data updated successfully!")
        st.rerun()
    df = load_data()
    if df is not None:
        st.subheader("All Players")
        st.dataframe(df, use_container_width=True, hide_index=True)
    else:
        st.info("Please upload an HTML file to display player data.")

def parse_position_string(pos_str):
    if not isinstance(pos_str, str): return set()
    final_pos = set()
    for part in [p.strip() for p in pos_str.split(',')]:
        match = re.match(r'([A-Z/]+) *(?:\(([RLC]+)\))?$', part.strip())
        if match:
            bases, sides = match.groups()
            for base in bases.split('/'):
                if sides:
                    for side in list(sides): final_pos.add(f"{base} ({side})")
                else:
                    final_pos.add(f"{base} (C)" if base == "ST" else base)
    return final_pos

def assign_roles_page():
    st.title("Assign Roles to Players")
    df = pd.DataFrame(get_all_players())
    if df.empty:
        st.info("No players available. Please upload player data.")
        return
    st.subheader("Filter & Sort")
    c1, c2, c3 = st.columns(3)
    filter_option = c1.selectbox("Filter by", options=FILTER_OPTIONS)
    club_filter = c2.selectbox("Filter by Club", options=["All"] + sorted(df['Club'].unique()))
    pos_filter = c3.selectbox("Filter by Position", options=["All"] + sorted(df['Position'].unique()))
    sort_column = c1.selectbox("Sort by", options=SORTABLE_COLUMNS)
    sort_order = c2.selectbox("Sort Order", options=["Ascending", "Descending"])
    filtered_df = get_filtered_players(filter_option, club_filter, pos_filter, sort_column, (sort_order == "Ascending"), get_user_club())
    search = st.text_input("Search by Name")
    if search: filtered_df = filtered_df[filtered_df['Name'].str.contains(search, case=False, na=False)]
    
    def handle_role_update(role_changes):
        if role_changes:
            update_player_roles(role_changes)
            affected = df[df['Unique ID'].isin(role_changes.keys())]
            update_dwrs_ratings(affected, get_valid_roles())
            clear_all_caches()
            st.success(f"Successfully updated roles for {len(role_changes)} players!")
            st.rerun()
        else: st.info("No changes to apply.")

    st.subheader("Automatic Role Assignment")
    c1, c2 = st.columns(2)
    if c1.button("Auto-Assign to Unassigned Players"):
        unassigned = df[df['Assigned Roles'].apply(lambda x: not x)]
        changes = {p['Unique ID']: sorted(list(set(r for pos in parse_position_string(p['Position']) for r in get_position_to_role_mapping().get(pos, [])))) for _, p in unassigned.iterrows()}
        handle_role_update({k: v for k, v in changes.items() if v})
    if c2.button("⚠️ Auto-Assign to ALL Players"):
        changes = {p['Unique ID']: sorted(list(set(r for pos in parse_position_string(p['Position']) for r in get_position_to_role_mapping().get(pos, [])))) for _, p in df.iterrows()}
        handle_role_update({k: v for k, v in changes.items() if set(v) != set(df[df['Unique ID'] == k]['Assigned Roles'].iloc[0])})
    
    st.subheader("Assign/Edit Roles Individually")
    st.dataframe(filtered_df[['Name', 'Position', 'Club', 'Assigned Roles']], use_container_width=True, hide_index=True)
    changes = {}
    for _, row in filtered_df.iterrows():
        new = st.multiselect(f"Roles for {row['Name']} ({row['Position']})", options=get_valid_roles(), default=row['Assigned Roles'], key=f"roles_{row['Unique ID']}", format_func=format_role_display)
        #new = st.multiselect(f"Roles for {row['Name']}", options=get_valid_roles(), default=row['Assigned Roles'], key=f"roles_{row['Unique ID']}", format_func=format_role_display)
        if new != row['Assigned Roles']: changes[row['Unique ID']] = new
    if st.button("Save All Individual Changes"): handle_role_update(changes)

def role_analysis_page():
    st.title("Role Analysis")
    user_club, second_club = get_user_club(), get_second_team_club()
    if not user_club:
        st.warning("Please select your club in the sidebar.")
        return
    role = st.selectbox("Select Role", options=get_valid_roles(), format_func=format_role_display)
    my_df, second_df, scout_df = get_players_by_role(role, user_club, second_club)
    st.subheader(f"Players from {user_club}")
    st.dataframe(my_df, use_container_width=True, hide_index=True)
    if second_club:
        st.subheader(f"Players from {second_club}")
        st.dataframe(second_df, use_container_width=True, hide_index=True)
    st.subheader("Scouted Players")
    st.dataframe(scout_df, use_container_width=True, hide_index=True)

def player_role_matrix_page():
    st.title("Player-Role Matrix")
    st.write("View DWRS ratings for players in assigned roles. Select a tactic to see relevant roles, and toggle extra details.")
    
    user_club = get_user_club()
    second_team_club = get_second_team_club()
    if not user_club:
        st.warning("Please select your club in the sidebar to use this page.")
        return
    
    st.subheader("Display Options")
    col1, col2, col3, col4 = st.columns(4)
    with col1:
        fav_tactic1, fav_tactic2 = get_favorite_tactics()
        all_tactics = sorted(list(get_tactic_roles().keys()))
        
        # Create the sorted list
        sorted_tactics = []
        if fav_tactic1 and fav_tactic1 in all_tactics:
            sorted_tactics.append(fav_tactic1)
        if fav_tactic2 and fav_tactic2 in all_tactics and fav_tactic2 != fav_tactic1:
            sorted_tactics.append(fav_tactic2)
        
        # Add remaining tactics
        for tactic in all_tactics:
            if tactic not in sorted_tactics:
                sorted_tactics.append(tactic)
        
        tactic_options = ["All Roles"] + sorted_tactics
        
        # The index will be 1 if a favorite is set, otherwise 0
        default_index = 1 if fav_tactic1 else 0
        
        selected_tactic = st.selectbox("Select Tactic", options=tactic_options, index=default_index)
    with col2:
        show_extra_details = st.checkbox("Show Extra Details")
    with col3:
        show_second_team = st.checkbox("Show Second Team separately", value=False)
    with col4:
        hide_retired = st.checkbox("Hide 'Retired' Players", value=True)
    
    selected_roles = get_valid_roles() if selected_tactic == "All Roles" else sorted(list(set(get_tactic_roles()[selected_tactic].values())))
    full_matrix = get_player_role_matrix(user_club, second_team_club)
    
    if full_matrix.empty:
        st.info("No player data to generate matrix.")
        return

    if hide_retired:
        full_matrix = full_matrix[full_matrix['Club'].str.lower() != 'retired']

    my_club_matrix = full_matrix[full_matrix['Club'] == user_club].copy()
    second_team_matrix = full_matrix[full_matrix['Club'] == second_team_club].copy() if second_team_club else pd.DataFrame()
    exclude_clubs = [user_club]
    if second_team_club: exclude_clubs.append(second_team_club)
    scouted_matrix = full_matrix[~full_matrix['Club'].isin(exclude_clubs)].copy()

    def prepare_and_display_df(df, title, key_suffix, display_cols):
        st.subheader(title)
        
        # Sort the DataFrame by last name before displaying
        if not df.empty:
            df['LastName'] = df['Name'].apply(get_last_name)
            df = df.sort_values(by=['LastName', 'Name']).drop(columns=['LastName'])

        search_term = st.text_input(f"Search by Name in {title}", key=f"search_{key_suffix}")
        if search_term:
            df = df[df['Name'].str.contains(search_term, case=False, na=False)]

        if df.empty:
            st.write("No players found for this category or matching the filter.")
            return

        existing_cols = [col for col in display_cols if col in df.columns]
        df_display = df[existing_cols]

        # --- NEW: Define column formatting ---
        # Get all role columns that exist in this dataframe
        role_cols_df = [role for role in get_valid_roles() if role in df_display.columns]
        
        # Create a configuration dictionary to format role columns as percentages
        column_config = {
            role: st.column_config.NumberColumn(
                label=role, # Use the full role name in the header
                format="%d%%",                  # Display the number with a '%' sign
            )
            for role in role_cols_df
        }
        # --- END NEW ---

        st.dataframe(
            df_display,
            use_container_width=True,
            hide_index=True,
            column_config=column_config # Pass the new configuration here
        )
        
        csv_buffer = StringIO()
        df_display.to_csv(csv_buffer, index=False)
        st.download_button(label=f"Download {title} Matrix as CSV", data=csv_buffer.getvalue(), file_name=f"{title.lower().replace(' ', '_')}_matrix.csv", mime="text/csv")

    my_club_base_cols = ["Name", "Age", "Position", "Wage"]
    if show_extra_details:
        my_club_base_cols.extend(["Left Foot", "Right Foot", "Height", "Transfer Value"])
    my_club_display_cols = my_club_base_cols + selected_roles

    scouted_base_cols = ["Name", "Age", "Position", "Club", "Transfer Value", "Wage"]
    if show_extra_details:
        scouted_base_cols.extend(["Left Foot", "Right Foot", "Height"])
    scouted_display_cols = scouted_base_cols + selected_roles

    if not show_second_team and second_team_club and not second_team_matrix.empty:
        combined_club_matrix = pd.concat([my_club_matrix, second_team_matrix]).sort_values(by='Name')
        prepare_and_display_df(combined_club_matrix, f"Players from {user_club} & Second Team", "combined_club", my_club_display_cols)
    else:
        prepare_and_display_df(my_club_matrix, f"Players from {user_club}", "my_club", my_club_display_cols)
        if second_team_club and show_second_team:
            prepare_and_display_df(second_team_matrix, f"Players from {second_team_club} (Second Team)", "second_team", my_club_display_cols)

    prepare_and_display_df(scouted_matrix, "Scouted Players", "scouted", scouted_display_cols)

    
def best_position_calculator_page():
    st.title("Best Position Calculator")
    st.write("This tool uses a 'weakest link first' algorithm to build the most balanced team based on a selected tactic.")

    @st.cache_data
    def _get_master_role_ratings(user_club):
        master_ratings = {}
        all_club_players_df = pd.DataFrame([p for p in get_all_players() if p['Club'] == user_club])
        if all_club_players_df.empty: return {}
        for role in get_valid_roles():
            ratings_df, _, _ = get_players_by_role(role, user_club)
            if not ratings_df.empty:
                ratings_df['DWRS'] = pd.to_numeric(ratings_df['DWRS Rating (Normalized)'].str.rstrip('%'))
                master_ratings[role] = ratings_df.set_index('Unique ID')['DWRS'].to_dict()
        return master_ratings

    user_club = get_user_club()
    if not user_club:
        st.warning("Please select your club in the sidebar.")
        return

    fav_tactic1, fav_tactic2 = get_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    sorted_tactics = []
    if fav_tactic1 and fav_tactic1 in all_tactics: sorted_tactics.append(fav_tactic1)
    if fav_tactic2 and fav_tactic2 in all_tactics and fav_tactic2 != fav_tactic1: sorted_tactics.append(fav_tactic2)
    for tactic in all_tactics:
        if tactic not in sorted_tactics: sorted_tactics.append(tactic)
    tactic = st.selectbox("Select Tactic", options=sorted_tactics, index=0)
    
    positions, layout = get_tactic_roles()[tactic], get_tactic_layouts()[tactic]
    my_club_players = [p for p in get_all_players() if p['Club'] == user_club]
    
    with st.spinner("Calculating best teams and surplus players..."):
        master_role_ratings = _get_master_role_ratings(user_club)
        squad_data = _calculate_squad_and_surplus(my_club_players, positions, master_role_ratings)

    def display_layout(team, title):
        st.subheader(title)
        default_player = {"name": "-", "rating": "0%", "apt": ""}
        gk_pos_key = 'GK'
        gk_role = positions.get(gk_pos_key, "GK")
        player_info = team.get(gk_pos_key, default_player)
        apt_html = f"<br><small><i>{player_info['apt']}</i></small>" if player_info['apt'] else ""
        gk_display = f"<div style='text-align: center;'><b>{gk_pos_key}</b> ({gk_role})<br>{player_info['name']}<br><i>{player_info['rating']}</i>{apt_html}</div>"
        for row in reversed(layout):
            cols = st.columns(len(row))
            for i, pos_key in enumerate(row):
                player_info = team.get(pos_key, default_player)
                role = positions.get(pos_key, "")
                apt_html = f"<br><small><i>{player_info['apt']}</i></small>" if player_info['apt'] else ""
                cols[i].markdown(f"<div style='text-align: center; border: 1px solid #444; border-radius: 5px; padding: 10px; height: 100%;'><b>{pos_key}</b> ({role})<br>{player_info['name']}<br><i>{player_info['rating']}</i>{apt_html}</div>", unsafe_allow_html=True)
            st.write("")
        st.markdown(gk_display, unsafe_allow_html=True)

    display_layout(squad_data["starting_xi"], "Starting XI")
    st.divider()
    display_layout(squad_data["b_team"], "B Team")
    st.divider()

    st.subheader("Additional Depth")
    best_depth_options = squad_data["best_depth_options"]
    if best_depth_options:
        for pos in sorted(best_depth_options.keys()):
            players = best_depth_options.get(pos, [])
            if players:
                player_strs = [f"{p['name']} ({p['rating']}){' - ' + p['apt'] if p['apt'] else ''}" for p in players]
                display_str = ', '.join(player_strs)
                st.markdown(f"**{pos} ({positions[pos]})**: {display_str}")
    else:
        st.info("No other players were suitable as depth options for this tactic.")

    st.divider()
    st.subheader(f"Surplus Players for '{tactic}' tactic")
    st.success("✔️ To manage these players, go to the **Transfer & Loan Management** page in the sidebar!")

    if not squad_data["surplus_players"]:
        st.info("No surplus players found for this tactic. Your squad is perfectly balanced!")
    else:
        senior_surplus = squad_data["senior_surplus"]
        if senior_surplus:
            # --- Corrected Title ---
            st.markdown("**For Sale / Release (Outfielders 21+, GKs 26+):**")
            df_senior_data = {
                'Name': [p['Name'] for p in senior_surplus], 'Age': [p.get('Age', 'N/A') for p in senior_surplus],
                'On Transfer List': ["✅" if p.get('transfer_status', 0) else "❌" for p in senior_surplus],
                'On Loan List': ["✅" if p.get('loan_status', 0) else "❌" for p in senior_surplus]
            }
            st.dataframe(pd.DataFrame(df_senior_data), hide_index=True, use_container_width=True)
        
        youth_surplus = squad_data["youth_surplus"]
        if youth_surplus:
            # --- Corrected Title ---
            st.markdown("**For Loan (Outfielders U20, GKs U25):**")
            df_youth_data = {
                'Name': [p['Name'] for p in youth_surplus], 'Age': [p.get('Age', 'N/A') for p in youth_surplus],
                'On Transfer List': ["✅" if p.get('transfer_status', 0) else "❌" for p in youth_surplus],
                'On Loan List': ["✅" if p.get('loan_status', 0) else "❌" for p in youth_surplus]
            }
            st.dataframe(pd.DataFrame(df_youth_data), hide_index=True, use_container_width=True)

  
def transfer_loan_management_page():
    st.title("Transfer & Loan Management")
    st.info("This page lists players who are surplus to requirements based on your selected tactic. You can manage their transfer/loan status here.")

    user_club = get_user_club()
    if not user_club:
        st.warning("Please select your club in the sidebar to use this feature.")
        return

    fav_tactic1, _ = get_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    try:
        default_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
    except ValueError:
        default_index = 0
    tactic = st.selectbox("Select Tactic to Analyze Surplus Players", options=all_tactics, index=default_index)

    positions = get_tactic_roles()[tactic]
    my_club_players = [p for p in get_all_players() if p['Club'] == user_club]
    
    # We still need the master ratings for the calculation
    master_ratings = {} # <-- The variable is defined here as master_ratings
    for role in get_valid_roles():
        ratings_df, _, _ = get_players_by_role(role, user_club)
        if not ratings_df.empty:
            ratings_df['DWRS'] = pd.to_numeric(ratings_df['DWRS Rating (Normalized)'].str.rstrip('%'))
            master_ratings[role] = ratings_df.set_index('Unique ID')['DWRS'].to_dict()
            
    # --- THE FIX IS ON THIS LINE ---
    # Call the helper function with the correct variable name: master_ratings
    squad_data = _calculate_squad_and_surplus(my_club_players, positions, master_ratings)
    
    # Extract the lists we need from the returned dictionary
    senior_surplus = squad_data["senior_surplus"]
    youth_surplus = squad_data["youth_surplus"]
    surplus_players = squad_data["surplus_players"]
    
    # --- UI FOR MANAGEMENT ---
    
    def display_management_table(player_list, title):
        st.subheader(title)
        
        if not player_list:
            st.info(f"No players in this category for the '{tactic}' tactic.")
            return

        df_data = {
            "Name": [p['Name'] for p in player_list],
            "Age": [p.get('Age', 'N/A') for p in player_list],
            'On Transfer List': ["✅" if p.get('transfer_status', 0) else "❌" for p in player_list],
            'On Loan List': ["✅" if p.get('loan_status', 0) else "❌" for p in player_list]
        }
        st.dataframe(pd.DataFrame(df_data), hide_index=True, use_container_width=True)
        st.markdown("---")

        for player in player_list:
            uid = player['Unique ID']
            c1, c2, c3, c4 = st.columns([2, 1, 1, 2])
            with c1:
                st.write(player['Name'])
            with c2:
                st.checkbox("Transfer List", value=bool(player.get('transfer_status', 0)), key=f"transfer_{uid}", 
                            on_change=update_player_transfer_status, args=(uid, not bool(player.get('transfer_status', 0))))
            with c3:
                st.checkbox("Loan List", value=bool(player.get('loan_status', 0)), key=f"loan_{uid}",
                            on_change=update_player_loan_status, args=(uid, not bool(player.get('loan_status', 0))))
            with c4:
                if f"club_input_{uid}" not in st.session_state:
                    st.session_state[f"club_input_{uid}"] = ""
                st.text_input("New Club", key=f"club_input_{uid}", label_visibility="collapsed", placeholder="e.g., Retired, Man Utd")

    col1, col2 = st.columns(2)
    with col1:
        display_management_table(senior_surplus, "For Sale / Release (Outfielders 21+, GKs 26+)")
    with col2:
        display_management_table(youth_surplus, "For Loan (Outfielders U20, GKs U25)")

    if surplus_players and st.button("Save All Club Changes", type="primary"):
        with st.spinner("Saving club updates..."):
            changes_made = 0
            for player in surplus_players:
                uid = player['Unique ID']
                new_club = st.session_state.get(f"club_input_{uid}", "").strip()
                if new_club:
                    update_player_club(uid, new_club)
                    changes_made += 1
            st.success(f"Successfully processed {changes_made} club changes!")
            clear_all_caches()
            st.rerun()

def player_comparison_page():
    st.title("Player Comparison")
    df = pd.DataFrame(get_all_players())
    if df.empty:
        st.info("No players available.")
        return
    player_names = sorted(df['Name'].unique(), key=get_last_name)
    names = st.multiselect("Select players to compare", options=player_names)
    if names:
        #st.dataframe(df[df['Name'].isin(names)].set_index('Name').T, use_container_width=True)
        comparison_df = df[df['Name'].isin(names)].copy()
        
        # Convert the 'Assigned Roles' list to a readable string to prevent the ArrowTypeError
        if 'Assigned Roles' in comparison_df.columns:
            comparison_df['Assigned Roles'] = comparison_df['Assigned Roles'].apply(
                lambda roles: ', '.join(roles) if isinstance(roles, list) else roles
            )

        # Set index, transpose, and display the corrected DataFrame
        st.dataframe(comparison_df.set_index('Name').T, use_container_width=True)

def dwrs_progress_page():
    st.title("DWRS Progress Over Time")

    all_players = get_all_players()
    if not all_players:
        st.info("No players available.")
        return

    # --- NEW HIERARCHICAL FILTERING UI ---
    st.subheader("Display Options")
    
    # -- Filter 1: Select Tactic (with favorites) --
    fav_tactic1, _ = get_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    
    try:
        # Set default index to the favorite tactic if it exists
        default_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
    except ValueError:
        default_index = 0
        
    selected_tactic = st.selectbox("Select a Tactic to Analyze", options=all_tactics, index=default_index)
    
    # -- Filter 2: Select Display Mode --
    display_mode = st.selectbox(
        "Display progress by:",
        ["Player Position Category", "Specific Role", "Individual Players"]
    )
    
    # -- Filter 3: Conditional Selector & U20 Checkbox --
    c1, c2 = st.columns([3, 1])
    
    player_ids_to_chart = []
    role_to_chart = None
    
    positions_tactic = get_tactic_roles().get(selected_tactic, {})
    
    with c1:
        if display_mode == "Player Position Category":
            # Define position categories based on common tactical shapes
            pos_categories = {
                'Attackers': ['ST', 'AML', 'AMR', 'AMC'],
                'Midfielders': ['ML', 'MC', 'MR', 'DML', 'DMC', 'DMR'],
                'Defenders': ['DL', 'DC', 'DR', 'WBL', 'WBR']
            }
            selected_cat = st.selectbox("Select Position Category", options=pos_categories.keys())
            
            # Find all players who are assigned to any role in the tactic that fits the category
            players_category = set()
            for pos_code, role in positions_tactic.items():
                # Check the start of the position code (e.g., 'STL' starts with 'ST')
                if any(pos_code.startswith(cat_prefix) for cat_prefix in pos_categories[selected_cat]):
                    # Get all players assigned this role
                    role_players_df, _, _ = get_players_by_role(role, get_user_club())
                    if not role_players_df.empty:
                        for pid in role_players_df['Unique ID']:
                            players_category.add(pid)
            player_ids_to_chart = list(players_category)
            role_to_chart = "All Roles" # Show all relevant roles for these players

        elif display_mode == "Specific Role":
            # Get unique roles from the selected tactic and display them nicely
            roles_tactic = sorted(list(set(positions_tactic.values())), key=format_role_display)
            selected_role = st.selectbox("Select Specific Role", options=roles_tactic, format_func=format_role_display)
            
            # Find all players assigned this role
            role_players_df, _, _ = get_players_by_role(selected_role, get_user_club())
            if not role_players_df.empty:
                player_ids_to_chart = role_players_df['Unique ID'].tolist()
            role_to_chart = selected_role

        else: # Individual Players
            player_names = sorted([p['Name'] for p in all_players], key=get_last_name)
            selected_names = st.multiselect("Select Players", options=player_names)
            player_ids_to_chart = [p['Unique ID'] for p in all_players if p['Name'] in selected_names]
            role_to_chart = "All Roles"

    with c2:
        st.write("") # Spacer
        st.write("") # Spacer
        u20_only = st.checkbox("Youth Prospects Only (U20)")

    # --- DATA PROCESSING AND CHARTING ---
    if not player_ids_to_chart:
        st.info("Select players or a category to display the chart.")
        return

    # Filter for U20 players if the checkbox is ticked
    if u20_only:
        u20_ids = {p['Unique ID'] for p in all_players if p.get('Age') and int(p['Age']) <= 20}
        player_ids_to_chart = [pid for pid in player_ids_to_chart if pid in u20_ids]

    if not player_ids_to_chart:
        st.warning("No players match the selected criteria (e.g., no U20 players found).")
        return

    history = get_dwrs_history(player_ids_to_chart, role_to_chart if role_to_chart != "All Roles" else None)

    if not history.empty:
        st.subheader("DWRS Progress by Data Snapshot")
        history['dwrs_normalized'] = pd.to_numeric(history['dwrs_normalized'].str.rstrip('%'))
        
        # Merge with player names
        player_name_map = {p['Unique ID']: p['Name'] for p in all_players}
        history['Name'] = history['unique_id'].map(player_name_map)
        
        # Create a unique label for each line on the chart
        if role_to_chart == "All Roles":
            history['label'] = history['Name'] + ' - ' + history['role'].apply(format_role_display)
        else:
            history['label'] = history['Name']

        # --- THE NEW PIVOT ---
        # The X-axis (index) is now the 'snapshot' number.
        pivot = history.pivot_table(index='snapshot', columns='label', values='dwrs_normalized', aggfunc='mean')
        
        # Fill missing values to create continuous lines where possible
        pivot = pivot.interpolate(method='linear', limit_direction='forward', axis=0)

        st.line_chart(pivot)
        
        with st.expander("Show Raw Data"):
            st.dataframe(history[['snapshot', 'Name', 'role', 'dwrs_normalized', 'timestamp']].sort_values(by=['Name', 'role', 'snapshot']), use_container_width=True, hide_index=True)
    else:
        st.info("No historical data available for the selected players/roles.")

def edit_player_data_page():
    st.title("Edit Player Data")
    user_club = get_user_club()
    if not user_club:
        st.warning("Please select your club from the sidebar to use this feature.")
        return

    all_players = get_all_players()
    if not all_players:
        st.info("No players loaded.")
        return

    player_to_edit = None

    # --- Dropdown for user's club players ---
    st.subheader("Select Player from Your Club")
    st.caption("Players are marked with 🎯 for a missing Primary Role and 📄 for missing Agreed Playing Time.")
    
    my_club_players = sorted([p for p in all_players if p['Club'] == user_club], key=lambda p: get_last_name(p['Name']))
    
    player_options_map = {}
    dropdown_options = ["--- Select a Player ---"]

    for player in my_club_players:
        markers = []
        # Check for Primary Role
        if not bool(player.get('primary_role')):
            markers.append('🎯')
        # Check for Agreed Playing Time
        if not bool(player.get('Agreed Playing Time')):
            markers.append('📄')
        
        # Join the markers with a space if there are multiple
        marker = f" {' '.join(markers)}" if markers else ""
        display_name = f"{player['Name']}{marker}"

        
        dropdown_options.append(display_name)
        player_options_map[display_name] = player['Unique ID']

    selected_dropdown_option = st.selectbox("My Club Players", options=dropdown_options, index=0)

    if selected_dropdown_option != "--- Select a Player ---":
        player_id = player_options_map[selected_dropdown_option]
        player_to_edit = next((p for p in all_players if p['Unique ID'] == player_id), None)

    st.divider()

    # --- Existing Search Feature (as a fallback) ---
    st.subheader("Or, Search All Players")
    search = st.text_input("Search for a player by name")
    
    if search and not player_to_edit:
        results = [p for p in all_players if search.lower() in p['Name'].lower()]
        if results:
            search_options_map = {f"{p['Name']} ({p['Club']})": p for p in results}
            selected_search_option = st.selectbox("Select a player from search results", options=list(search_options_map.keys()))
            if selected_search_option:
                player_to_edit = search_options_map[selected_search_option]
        else:
            st.warning("No players found with that name.")
    
    # --- Main Edit Form ---
    if player_to_edit:
        player = player_to_edit
        st.write(f"### Editing: {player['Name']}")
        
        st.subheader("Update Club")
        new_club = st.text_input("Club Name", value=player['Club'], key=f"club_{player['Unique ID']}")
        if st.button("Save Club Change"):
            update_player_club(player['Unique ID'], new_club)
            clear_all_caches()
            st.success(f"Updated club to '{new_club}'.")
            st.rerun()

        if player['Club'] == user_club:
            st.subheader("Set Agreed Playing Time")
            is_gk = "GK" in player.get('Position', '')
            apt_options = GK_APT_OPTIONS if is_gk else FIELD_PLAYER_APT_OPTIONS
            current_apt = player.get('agreed_playing_time')
            
            try:
                apt_index = apt_options.index(current_apt)
            except (ValueError, TypeError):
                apt_index = 0
            
            new_apt = st.selectbox("Agreed Playing Time", options=apt_options, index=apt_index, key=f"apt_{player['Unique ID']}")

            if st.button("Save Playing Time"):
                value_to_save = None if new_apt == "None" else new_apt
                update_player_apt(player['Unique ID'], value_to_save)
                clear_all_caches()
                st.success(f"Set Agreed Playing Time to '{new_apt}'.")
                st.rerun()

            st.divider()

            st.subheader("Set Primary Role")
            st.info("This role will be prioritized in the 'Best Position Calculator'.")
            role_options = ["None"] + sorted(player.get('Assigned Roles', []))
            current_role = player.get('primary_role')
            
            try:
                role_index = role_options.index(current_role)
            except (ValueError, TypeError):
                role_index = 0

            new_role = st.selectbox("Primary Role", role_options, index=role_index, format_func=lambda x: "None" if x == "None" else format_role_display(x), key=f"role_{player['Unique ID']}")
            
            if st.button("Save Primary Role"):
                set_primary_role(player['Unique ID'], new_role if new_role != "None" else None)
                clear_all_caches()
                st.success(f"Set primary role to {new_role}.")
                st.rerun()

def create_new_role_page():
    st.title("Create a New Player Role")
    st.info("Define a new field player role. The short name is generated automatically as you type. After creation, the app will reload with the new role available everywhere.")

    # Attribute lists as you defined them
    TECHNICAL_ATTRS = ["Corners", "Crossing", "Dribbling", "Finishing", "First Touch", "Free Kick Taking", "Heading", "Long Shots", "Long Throws", "Marking", "Passing", "Penalty Taking", "Tackling", "Technique"]
    MENTAL_ATTRS = ["Aggression", "Anticipation", "Bravery", "Composure", "Concentration", "Decisions", "Determination", "Flair", "Leadership", "Off the Ball", "Positioning", "Teamwork", "Vision", "Work Rate"]
    PHYSICAL_ATTRS = ["Acceleration", "Agility", "Balance", "Jumping Reach", "Natural Fitness", "Pace", "Stamina", "Strength"]
    ALL_ATTRIBUTES = TECHNICAL_ATTRS + MENTAL_ATTRS + PHYSICAL_ATTRS

    # --- PART 1: Inputs that need live feedback (moved outside the form) ---
    st.subheader("1. Basic Role Information")
    
    c1, c2, c3 = st.columns(3)
    with c1:
        role_name = st.text_input("Full Role Name (e.g., 'Advanced Playmaker')", key="new_role_name", help="The descriptive name of the role.")
    with c2:
        role_cat = st.selectbox("Role Category", ["Defense", "Midfield", "Attack"], key="new_role_cat")
    with c3:
        role_duty = st.selectbox("Role Duty", ["Defend", "Support", "Attack", "Automatic", "Cover", "Stopper"], key="new_role_duty")

    # Auto-generate and display short name dynamically
    if role_name:
        short_name_suggestion = "".join([word[0] for word in role_name.split()]).upper()
        duty_suffix = role_duty[0] if role_duty not in ["Cover", "Stopper"] else role_duty[:2]
        final_short_name = f"{short_name_suggestion}-{duty_suffix}"
        st.write(f"**Generated Short Name:** `{final_short_name}` (This will be the unique ID)")
    else:
        final_short_name = ""
        st.write("**Generated Short Name:** `...`")


    with st.form("new_role_form"):
        st.subheader("2. Position Mapping")
        definitions = get_definitions()
        possible_positions = sorted([p for p in definitions['position_to_role_mapping'].keys() if p != "GK"])
        assigned_positions = st.multiselect("Assign this role to positions:", options=possible_positions, help="Select one or more positions where this role can be used.", key="new_role_positions")

        st.subheader("3. Key and Preferable Attributes")
        st.warning("A 'Key' attribute gets the highest multiplier. A 'Preferable' attribute gets a medium multiplier. If both are checked, 'Key' will be prioritized.")

        tc, mc, pc = st.columns(3)
        with tc:
            st.markdown("#### Technical")
            for attr in TECHNICAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        with mc:
            st.markdown("#### Mental")
            for attr in MENTAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        with pc:
            st.markdown("#### Physical")
            for attr in PHYSICAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        
        submitted = st.form_submit_button("Create New Role", type="primary")

    if submitted:
        with st.spinner("Validating and saving new role..."):
            if not role_name or not assigned_positions:
                st.error("Validation Failed: Please provide a Full Role Name and at least one Position Mapping.")
                return
            
            if final_short_name in definitions['role_specific_weights']:
                st.error(f"Validation Failed: The short name '{final_short_name}' already exists. Please choose a different role name or duty.")
                return

            key_attrs, pref_attrs = [], []
            for attr in ALL_ATTRIBUTES:
                if st.session_state.get(f"key_{attr}"):
                    key_attrs.append(attr)
                elif st.session_state.get(f"pref_{attr}"):
                    pref_attrs.append(attr)

            full_role_display_name = f"{role_name} ({role_duty})"
            
            definitions['player_roles'][role_cat][final_short_name] = full_role_display_name
            definitions['role_specific_weights'][final_short_name] = {"key": key_attrs, "preferable": pref_attrs}
            for pos in assigned_positions:
                if pos in definitions['position_to_role_mapping']:
                    definitions['position_to_role_mapping'][pos].append(final_short_name)
                    definitions['position_to_role_mapping'][pos].sort()

            success, message = save_definitions(definitions)

            if success:
                st.success(f"Role '{full_role_display_name}' created successfully! Reloading application...")
                
                # --- FIX: Clear all session state keys to reset the form completely ---
                keys_to_delete = [k for k in st.session_state.keys() if k.startswith('new_role_') or k.startswith('key_') or k.startswith('pref_')]
                for key in keys_to_delete:
                    del st.session_state[key]
                
                clear_all_caches()
                st.rerun()
            else:
                st.error(f"Failed to save role: {message}")

def settings_page():
    st.title("Settings")

    st.subheader("Favorite Tactic Selection")
    st.info("The selected tactics will appear at the top of the list on the analysis pages.")
    
    # Get all available tactics and add a "None" option
    all_tactics = ["None"] + sorted(list(get_tactic_roles().keys()))
    
    # Get currently saved favorite tactics
    fav_tactic1, fav_tactic2 = get_favorite_tactics()
    
    # Set the index for the dropdowns, defaulting to "None" (index 0)
    index1 = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
    index2 = all_tactics.index(fav_tactic2) if fav_tactic2 in all_tactics else 0

    c1, c2 = st.columns(2)
    with c1:
        new_fav_tactic1 = st.selectbox("Primary Favorite Tactic", options=all_tactics, index=index1)
    with c2:
        new_fav_tactic2 = st.selectbox("Secondary Favorite Tactic", options=all_tactics, index=index2)
    
    st.divider()

    # --- NEW: Add Agreed Playing Time Weights Section ---
    st.subheader("Agreed Playing Time (APT) Weights")
    st.info("Adjust the multiplier for a player's selection score based on their promised playing time. A higher value makes them more likely to be selected.")
    
    all_apt_options = sorted(list(set(FIELD_PLAYER_APT_OPTIONS + GK_APT_OPTIONS) - {'None'}))
    new_apt_weights = {}
    
    cols = st.columns(3)
    col_idx = 0
    for apt in all_apt_options:
        with cols[col_idx % 3]:
            # Use the new get_apt_weight function to get the current value
            current_weight = get_apt_weight(apt)
            new_apt_weights[apt] = st.number_input(f"Weight for '{apt}'", 0.0, 5.0, current_weight, 0.05, key=f"apt_{apt}")
        col_idx += 1
    
    st.divider()

    st.subheader("Global Stat Weights")
    new_weights = {cat: st.number_input(f"{cat} Weight", 0.0, 10.0, get_weight(cat.lower().replace(" ", "_"), val), 0.1) for cat, val in { "Extremely Important": 8.0, "Important": 4.0, "Good": 2.0, "Decent": 1.0, "Almost Irrelevant": 0.2 }.items()}
    st.subheader("Goalkeeper Stat Weights")
    new_gk_weights = {cat: st.number_input(f"{cat} Weight", 0.0, 10.0, get_weight("gk_" + cat.lower().replace(" ", "_"), val), 0.1, key=f"gk_{cat}") for cat, val in { "Top Importance": 10.0, "High Importance": 8.0, "Medium Importance": 6.0, "Key": 4.0, "Preferable": 2.0, "Other": 0.5 }.items()}
    st.subheader("Role Multipliers")
    key_mult = st.number_input("Key Attributes Multiplier", 1.0, 20.0, get_role_multiplier('key'), 0.1)
    pref_mult = st.number_input("Preferable Attributes Multiplier", 1.0, 20.0, get_role_multiplier('preferable'), 0.1)
    st.subheader("Database Settings")
    db_name = st.text_input("Database Name (no .db)", value=get_db_name())
    
    if st.button("Save All Settings"):
        set_favorite_tactics(new_fav_tactic1, new_fav_tactic2)
        
        # --- NEW: Save the APT weights ---
        for apt, val in new_apt_weights.items():
            set_apt_weight(apt, val)

        for cat, val in new_weights.items(): set_weight(cat.lower().replace(" ", "_"), val)
        for cat, val in new_gk_weights.items(): set_weight("gk_" + cat.lower().replace(" ", "_"), val)
        set_role_multiplier('key', key_mult)
        set_role_multiplier('preferable', pref_mult)
        set_db_name(db_name)
        clear_all_caches()
        df = load_data()
        if df is not None: update_dwrs_ratings(df, get_valid_roles())
        st.success("Settings updated successfully!")
        st.rerun()

# --- FIXED: Main function with clear if/elif structure ---
def main():
    page, uploaded_file = sidebar()
    
    # Use st.query_params to allow linking to specific pages
    query_params = st.query_params
    if "page" in query_params:
        page = query_params["page"][0]
    
    if page == "All Players":
        main_page(uploaded_file)
    elif page == "Assign Roles":
        assign_roles_page()
    elif page == "Role Analysis":
        role_analysis_page()
    elif page == "Player-Role Matrix":
        player_role_matrix_page()
    elif page == "Best Position Calculator":
        best_position_calculator_page()
    elif page == "Transfer & Loan Management":
        transfer_loan_management_page()
    elif page == "Player Comparison":
        player_comparison_page()
    elif page == "DWRS Progress":
        dwrs_progress_page()
    elif page == "Edit Player Data":
        edit_player_data_page()
    elif page == "Create New Role":
        create_new_role_page()
    elif page == "Settings":
        settings_page()
    else:
        main_page(uploaded_file) # Default page

if __name__ == "__main__":
    main()