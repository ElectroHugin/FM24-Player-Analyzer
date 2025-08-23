# app.py

import streamlit as st
import pandas as pd
import re
from io import StringIO
import plotly.graph_objects as go
from data_parser import load_data, parse_and_update_data, get_filtered_players, get_players_by_role, get_player_role_matrix
from sqlite_db import (update_player_roles, get_user_club, set_user_club, update_dwrs_ratings, get_all_players, 
                     get_dwrs_history, get_second_team_club, set_second_team_club, update_player_club, set_primary_role, 
                     update_player_apt, get_favorite_tactics, set_favorite_tactics, 
                     update_player_transfer_status, update_player_loan_status)
from constants import (CSS_STYLES, SORTABLE_COLUMNS, FILTER_OPTIONS,
                     get_player_roles, get_valid_roles, get_position_to_role_mapping, 
                     get_tactic_roles, get_tactic_layouts, FIELD_PLAYER_APT_OPTIONS, GK_APT_OPTIONS, 
                     get_role_specific_weights, GLOBAL_STAT_CATEGORIES, GK_STAT_CATEGORIES)
from config_handler import (get_weight, set_weight, get_role_multiplier, set_role_multiplier, 
                          get_db_name, set_db_name, get_apt_weight, set_apt_weight, 
                          get_age_threshold, set_age_threshold)
from squad_logic import calculate_squad_and_surplus
from ui_components import display_tactic_grid
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


def sidebar(df):
    with st.sidebar:
        st.header("Navigation")
        page_options = ["All Players", "Assign Roles", "Role Analysis", "Player-Role Matrix", "Best Position Calculator", "Transfer & Loan Management", "Player Comparison", "DWRS Progress", "Edit Player Data", "Create New Role", "Settings"]
        page = st.radio("Go to", page_options)
        uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
        #df = load_data()
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

def main_page(uploaded_file, df):
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
    #df = load_data()
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

def assign_roles_page(df):
    st.title("Assign Roles to Players")
    # --- START: Initialize session state for filters ---
    if 'ar_filter_option' not in st.session_state:
        st.session_state.ar_filter_option = "All Players"
    if 'ar_club_filter' not in st.session_state:
        st.session_state.ar_club_filter = "All"
    if 'ar_pos_filter' not in st.session_state:
        st.session_state.ar_pos_filter = "All"
    if 'ar_sort_column' not in st.session_state:
        st.session_state.ar_sort_column = "Name"
    if 'ar_sort_order' not in st.session_state:
        st.session_state.ar_sort_order = "Ascending"
    if 'ar_search' not in st.session_state:
        st.session_state.ar_search = ""
    # --- END: Initialize session state ---
    #df = pd.DataFrame(get_all_players())
    if df.empty:
        st.info("No players available. Please upload player data.")
        return
    
    st.subheader("Filter & Sort")
    c1, c2, c3 = st.columns(3)

    # The 'key' argument automatically links the widget's state to st.session_state
    filter_option = c1.selectbox("Filter by", options=FILTER_OPTIONS, key='ar_filter_option')
    club_filter = c2.selectbox("Filter by Club", options=["All"] + sorted(df['Club'].unique()), key='ar_club_filter')
    pos_filter = c3.selectbox("Filter by Position", options=["All"] + sorted(df['Position'].unique()), key='ar_pos_filter')
    sort_column = c1.selectbox("Sort by", options=SORTABLE_COLUMNS, key='ar_sort_column')
    sort_order = c2.selectbox("Sort Order", options=["Ascending", "Descending"], key='ar_sort_order')
    search = st.text_input("Search by Name", key='ar_search')

    # Now, use the values from session_state to perform the filtering
    filtered_df = get_filtered_players(
        st.session_state.ar_filter_option, 
        st.session_state.ar_club_filter, 
        st.session_state.ar_pos_filter, 
        st.session_state.ar_sort_column, 
        (st.session_state.ar_sort_order == "Ascending"), 
        get_user_club()
    )

    if st.session_state.ar_search: 
        filtered_df = filtered_df[filtered_df['Name'].str.contains(st.session_state.ar_search, case=False, na=False)]
    
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


def best_position_calculator_page(players):
    st.title("Best Position Calculator")
    st.info("This tool uses a 'weakest link first' algorithm. Instead of picking the best player for each position one by one, it evaluates all open positions simultaneously and fills the one where the best available player provides the smallest upgrade. This creates a more balanced and often stronger overall team.")

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
    #my_club_players = [p for p in get_all_players() if p['Club'] == user_club]
    my_club_players = [p for p in players if p['Club'] == user_club]
    
    with st.spinner("Calculating best teams and surplus players..."):
        master_role_ratings = _get_master_role_ratings(user_club)
        squad_data = calculate_squad_and_surplus(my_club_players, positions, master_role_ratings)

    display_tactic_grid(squad_data["starting_xi"], "Starting XI", positions, layout)
    st.divider()
    display_tactic_grid(squad_data["b_team"], "B Team", positions, layout)

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

    outfielder_age_limit = get_age_threshold('outfielder')
    goalkeeper_age_limit = get_age_threshold('goalkeeper')

    if not squad_data["surplus_players"]:
        st.info("No surplus players found for this tactic. Your squad is perfectly balanced!")
    else:
        senior_surplus = squad_data["senior_surplus"]
        if senior_surplus:
            
            # --- Corrected Title ---
            st.markdown(f"**For Sale / Release (Outfielders {outfielder_age_limit}+ , GKs {goalkeeper_age_limit}+):**")
            df_senior_data = {
                'Name': [p['Name'] for p in senior_surplus], 'Age': [p.get('Age', 'N/A') for p in senior_surplus],
                'On Transfer List': ["✅" if p.get('transfer_status', 0) else "❌" for p in senior_surplus],
                'On Loan List': ["✅" if p.get('loan_status', 0) else "❌" for p in senior_surplus]
            }
            st.dataframe(pd.DataFrame(df_senior_data), hide_index=True, use_container_width=True)
        
        youth_surplus = squad_data["youth_surplus"]
        if youth_surplus:
            # --- Corrected Title ---
            st.markdown(f"**For Loan (Outfielders U{outfielder_age_limit}, GKs U{goalkeeper_age_limit}):**")
            df_youth_data = {
                'Name': [p['Name'] for p in youth_surplus], 'Age': [p.get('Age', 'N/A') for p in youth_surplus],
                'On Transfer List': ["✅" if p.get('transfer_status', 0) else "❌" for p in youth_surplus],
                'On Loan List': ["✅" if p.get('loan_status', 0) else "❌" for p in youth_surplus]
            }
            st.dataframe(pd.DataFrame(df_youth_data), hide_index=True, use_container_width=True)

  
def transfer_loan_management_page(players):
    st.title("Transfer & Loan Management")
    st.info("Manage surplus players based on your selected tactic. Use the save buttons to commit your changes.")

    # --- Helper functions for color-coding ---
    def color_attribute(value_str):
        try:
            value = int(value_str)
            if value >= 13: color = '#85f585' # Green
            elif value >= 10: color = '#f5f585' # Yellow
            else: color = '#f58585' # Red
            return f"<span style='color: {color}; font-weight: bold;'>{value}</span>"
        except (ValueError, TypeError):
            return "N/A"

    def color_age(age_str):
        try:
            age = int(age_str)
            color = '#f58585' if age <= 17 else 'white'
            return f"<span style='color: {color};'>{age}</span>"
        except (ValueError, TypeError):
            return "N/A"
        
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
    #my_club_players = [p for p in get_all_players() if p['Club'] == user_club]
    my_club_players = [p for p in players if p['Club'] == user_club]
    
    master_ratings = {}
    for role in get_valid_roles():
        ratings_df, _, _ = get_players_by_role(role, user_club)
        if not ratings_df.empty:
            ratings_df['DWRS'] = pd.to_numeric(ratings_df['DWRS Rating (Normalized)'].str.rstrip('%'))
            master_ratings[role] = ratings_df.set_index('Unique ID')['DWRS'].to_dict()
            
    squad_data = calculate_squad_and_surplus(my_club_players, positions, master_ratings)

    # --- Calculate Best DWRS AND Best Role for each surplus player ---
    tactic_roles = set(get_tactic_roles().get(tactic, {}).values())

    surplus_players_with_best_dwrs = []
    for player in squad_data["surplus_players"]:
        best_dwrs = 0
        best_role_abbr = ''
        assigned_roles = player.get('Assigned Roles', [])
        for role in assigned_roles:
            # NEW: Check if the player's role is relevant to the current tactic
            if role in tactic_roles:
                rating = master_ratings.get(role, {}).get(player['Unique ID'], 0)
                if rating > best_dwrs:
                    best_dwrs = rating
                    best_role_abbr = role
        
        player['Best DWRS'] = f"{int(best_dwrs)}%"
        player['Best Role Abbr'] = best_role_abbr
        surplus_players_with_best_dwrs.append(player)
    
    senior_surplus = [p for p in surplus_players_with_best_dwrs if p in squad_data["senior_surplus"]]
    youth_surplus = [p for p in surplus_players_with_best_dwrs if p in squad_data["youth_surplus"]]
    senior_surplus.sort(key=lambda p: get_last_name(p['Name']))
    youth_surplus.sort(key=lambda p: get_last_name(p['Name']))
    
    # --- COMPLETELY REFACTORED UI FOR MANAGEMENT ---
    def display_management_table(player_list, title, is_youth=False):
        st.subheader(title)
        if not player_list:
            st.info(f"No players in this category for the '{tactic}' tactic.")
            return

        # Define column widths and headers
        if is_youth:
            cols = [2.5, 0.5, 1.5, 0.5, 0.5, 0.8, 0.8, 2, 1]
            headers = ["Name", "Age", "Best DWRS (Role)", "Det", "Wor", "Transfer", "Loan", "New Club", "Action"]
        else: # Senior players
            cols = [3, 0.5, 1.5, 0.8, 0.8, 2, 1]
            headers = ["Name", "Age", "Best DWRS (Role)", "Transfer", "Loan", "New Club", "Action"]

        # Render Header
        header_cols = st.columns(cols)
        for i, header in enumerate(headers):
            header_cols[i].markdown(f"**{header}**")
        st.markdown("---")

        # Render Player Rows
        for player in player_list:
            uid = player['Unique ID']
            row_cols = st.columns(cols)
            
            # --- Set widgets for this player's row ---
            # Populate text input first to ensure its state is available
            if f"club_input_{uid}" not in st.session_state: st.session_state[f"club_input_{uid}"] = ""
            
            # Combine DWRS and Role for display
            best_role_abbr = player.get('Best Role Abbr', '')
            best_role_display = f"({best_role_abbr})" if best_role_abbr else ""
            dwrs_display = f"{player.get('Best DWRS', 'N/A')} {best_role_display}"
            
            if is_youth:
                row_cols[0].write(player['Name'])
                row_cols[1].markdown(color_age(player.get('Age')), unsafe_allow_html=True)
                row_cols[2].write(dwrs_display)
                row_cols[3].markdown(color_attribute(player.get('Determination')), unsafe_allow_html=True)
                row_cols[4].markdown(color_attribute(player.get('Work Rate')), unsafe_allow_html=True)
                row_cols[5].checkbox(label=f"transfer_{uid}", label_visibility="collapsed", value=bool(player.get('transfer_status', 0)), key=f"transfer_{uid}")
                row_cols[6].checkbox(label=f"loan_{uid}", label_visibility="collapsed", value=bool(player.get('loan_status', 0)), key=f"loan_{uid}")
                row_cols[7].text_input("New Club", key=f"club_input_{uid}", label_visibility="collapsed", placeholder="New club...")
                
                # Individual Save Button
                if row_cols[8].button("Save", key=f"save_{uid}"):
                    # Read values from session state and update DB
                    transfer_status = st.session_state[f"transfer_{uid}"]
                    loan_status = st.session_state[f"loan_{uid}"]
                    new_club = st.session_state[f"club_input_{uid}"].strip()
                    update_player_transfer_status(uid, transfer_status)
                    update_player_loan_status(uid, loan_status)
                    if new_club:
                        update_player_club(uid, new_club)
                    st.toast(f"Saved changes for {player['Name']}!", icon="✔️")

            else: # Senior players
                row_cols[0].write(player['Name'])
                row_cols[1].write(player.get('Age', 'N/A'))
                row_cols[2].write(dwrs_display)
                row_cols[3].checkbox(label=f"transfer_{uid}", label_visibility="collapsed", value=bool(player.get('transfer_status', 0)), key=f"transfer_{uid}")
                row_cols[4].checkbox(label=f"loan_{uid}", label_visibility="collapsed", value=bool(player.get('loan_status', 0)), key=f"loan_{uid}")
                row_cols[5].text_input("New Club", key=f"club_input_{uid}", label_visibility="collapsed", placeholder="New club...")
                
                # Individual Save Button
                if row_cols[6].button("Save", key=f"save_{uid}"):
                    transfer_status = st.session_state[f"transfer_{uid}"]
                    loan_status = st.session_state[f"loan_{uid}"]
                    new_club = st.session_state[f"club_input_{uid}"].strip()
                    update_player_transfer_status(uid, transfer_status)
                    update_player_loan_status(uid, loan_status)
                    if new_club:
                        update_player_club(uid, new_club)
                    st.toast(f"Saved changes for {player['Name']}!", icon="✔️")
        
        st.markdown("---")
        # "Save All" button for this specific table
        if st.button(f"Save All Changes for this List", key=f"save_all_{title.replace(' ', '_')}", type="primary"):
            with st.spinner(f"Saving all players in '{title}'..."):
                for p in player_list:
                    p_uid = p['Unique ID']
                    transfer_status = st.session_state[f"transfer_{p_uid}"]
                    loan_status = st.session_state[f"loan_{p_uid}"]
                    new_club = st.session_state[f"club_input_{p_uid}"].strip()
                    update_player_transfer_status(p_uid, transfer_status)
                    update_player_loan_status(p_uid, loan_status)
                    if new_club:
                        update_player_club(p_uid, new_club)
            st.success(f"All changes for players in '{title}' have been saved!")
            st.rerun()


    outfielder_age = get_age_threshold('outfielder')
    gk_age = get_age_threshold('goalkeeper')
    
    # --- Display tables in a single column, youth first ---
    display_management_table(youth_surplus, f"For Loan (Outfielders U{outfielder_age}, GKs U{gk_age})", is_youth=True)
    st.divider()
    display_management_table(senior_surplus, f"For Sale / Release (Outfielders {outfielder_age}+, GKs {gk_age}+)")

def player_comparison_page(players):
    st.title("Player Comparison")
    
    df = pd.DataFrame(players)
    if df.empty:
        st.info("No players available.")
        return

    # --- 1. ADVANCED FILTERING SECTION ---
    st.subheader("Filter Player Selection")
    user_club = get_user_club()
    f_col1, f_col2, f_col3 = st.columns(3)
    
    with f_col1:
        fav_tactic1, _ = get_favorite_tactics()
        all_tactics = ["All Roles"] + sorted(list(get_tactic_roles().keys()))
        tactic_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
        selected_tactic = st.selectbox("Filter by Tactic", options=all_tactics, index=tactic_index)

    with f_col2:
        if selected_tactic == "All Roles":
            role_options = get_valid_roles()
        else:
            role_options = sorted(list(set(get_tactic_roles()[selected_tactic].values())))
        selected_role = st.selectbox("Filter by Role", options=role_options, format_func=format_role_display)

    with f_col3:
        club_filter = st.selectbox("Filter by Club", options=["My Club", "All Players"])

    player_pool = df.copy()
    if club_filter == "My Club":
        player_pool = player_pool[player_pool['Club'] == user_club]
    
    player_pool = player_pool[player_pool['Assigned Roles'].apply(lambda roles: selected_role in roles if isinstance(roles, list) else False)]
    
    # --- START: BUG FIX FOR DUPLICATE NAMES ---

    # Create a mapping from Unique ID to a descriptive, unique display name
    player_map = {
        p['Unique ID']: f"{p['Name']} ({p['Club']})"
        for _, p in player_pool.iterrows()
    }
    
    if not player_map:
        st.warning(f"No players found with the role '{format_role_display(selected_role)}' in the selected club filter.")
        return
        
    # The multiselect options are now the Unique IDs, but it displays the descriptive names
    selected_ids = st.multiselect(
        f"Select players to compare (up to 5 for optimal viewing)",
        options=list(player_map.keys()),
        format_func=lambda uid: player_map[uid],
        help="Only players matching the filters above are shown."
    )

    if selected_ids:
        # Filter the main DataFrame using the list of selected Unique IDs
        comparison_df = df[df['Unique ID'].isin(selected_ids)].copy()
        
        # --- END: BUG FIX ---
        
        all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]
        is_gk_role = selected_role in all_gk_roles

        role_weights = get_role_specific_weights().get(selected_role, {"key": [], "preferable": []})
        key_attrs = role_weights["key"]
        pref_attrs = role_weights["preferable"]

        if is_gk_role:
            gameplay_attrs = { 'Shot Stopping': ['Reflexes', 'One vs One', 'Handling', 'Agility'], 'Aerial Control': ['Aerial Reach', 'Command of Area', 'Jumping Reach'], 'Distribution': ['Kicking', 'Throwing', 'Passing', 'Vision'], 'Sweeping': ['Rushing Out (Tendency)', 'Acceleration', 'Pace'], 'Mental': ['Composure', 'Concentration', 'Decisions', 'Anticipation']}
            meta_categories = { "Top Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "Top Importance"], "High Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "High Importance"], "Medium Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "Medium Importance"], "Key Role Attributes": key_attrs, "Preferable Role Attributes": pref_attrs}
            meta_chart_title = "GK Meta-Attribute Profile"
        else:
            gameplay_attrs = { 'Pace': ['Acceleration', 'Pace'], 'Shooting': ['Finishing', 'Long Shots'], 'Passing': ['Passing', 'Crossing', 'Vision'], 'Dribbling': ['Dribbling', 'First Touch', 'Flair'], 'Defending': ['Tackling', 'Marking', 'Positioning'], 'Physical': ['Strength', 'Stamina', 'Balance'], 'Mental': ['Work Rate', 'Determination', 'Teamwork', 'Decisions']}
            meta_categories = { "Extremely Important": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Extremely Important"], "Important": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Important"], "Good": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Good"], "Key Role Attributes": key_attrs, "Preferable Role Attributes": pref_attrs}
            meta_chart_title = "Outfield Meta-Attribute Profile"

        with st.expander("What do these charts show?"):
            st.info(f"The charts below visualize player attributes grouped into different categories. The Meta-Attribute Profile is based on the **{format_role_display(selected_role)}** role.")
            info_col1, info_col2 = st.columns(2)
            with info_col1:
                st.markdown("**Chart 1: Gameplay Areas**")
                md_string = "".join([f"- **{cat}**: `{', '.join(attrs)}`\n" for cat, attrs in gameplay_attrs.items()])
                st.markdown(md_string)
            with info_col2:
                st.markdown(f"**Chart 2: {meta_chart_title}**")
                meta_string = "".join([f"- **{cat}**: `{', '.join(attrs) or 'None'}`\n" for cat, attrs in meta_categories.items()])
                st.markdown(meta_string)
        
        def parse_attribute_value(raw_value):
            if isinstance(raw_value, str) and '-' in raw_value:
                try: return sum(map(float, raw_value.split('-'))) / 2
                except (ValueError, TypeError): return 0.0
            else:
                try: return float(raw_value)
                except (ValueError, TypeError): return 0.0

        chart_col1, chart_col2 = st.columns(2)
        with chart_col1:
            st.subheader("Gameplay Areas")
            fig1 = go.Figure()
            # Loop through the selected IDs to build the chart
            for uid in selected_ids:
                player_data = comparison_df[comparison_df['Unique ID'] == uid].iloc[0]
                category_values = [sum(parse_attribute_value(player_data.get(attr, 0)) for attr in attrs) / len(attrs) if attrs else 0 for attrs in gameplay_attrs.values()]
                fig1.add_trace(go.Scatterpolar(r=category_values, theta=list(gameplay_attrs.keys()), fill='toself', name=player_map[uid]))
            fig1.update_layout(polar=dict(radialaxis=dict(visible=True, range=[0, 20], tickfont=dict(color='white'), gridcolor='rgba(255, 255, 255, 0.4)'), angularaxis=dict(tickfont=dict(size=12, color='white'), direction="clockwise"), bgcolor='rgba(46, 46, 46, 0.8)'), showlegend=False, paper_bgcolor='rgba(0,0,0,0)', plot_bgcolor='rgba(0,0,0,0)', margin=dict(l=40, r=40, t=40, b=40))
            st.plotly_chart(fig1, use_container_width=True)

        with chart_col2:
            st.subheader(meta_chart_title)
            fig2 = go.Figure()
            # Loop through the selected IDs to build the chart
            for uid in selected_ids:
                player_data = comparison_df[comparison_df['Unique ID'] == uid].iloc[0]
                category_values = [sum(parse_attribute_value(player_data.get(attr, 0)) for attr in attrs) / len(attrs) if attrs else 0 for attrs in meta_categories.values()]
                fig2.add_trace(go.Scatterpolar(r=category_values, theta=list(meta_categories.keys()), fill='toself', name=player_map[uid]))
            fig2.update_layout(polar=dict(radialaxis=dict(visible=True, range=[0, 20], tickfont=dict(color='white'), gridcolor='rgba(255, 255, 255, 0.4)'), angularaxis=dict(tickfont=dict(size=12, color='white'), direction="clockwise"), bgcolor='rgba(46, 46, 46, 0.8)'), legend=dict(font=dict(color='white')), paper_bgcolor='rgba(0,0,0,0)', plot_bgcolor='rgba(0,0,0,0)', margin=dict(l=40, r=40, t=40, b=40))
            st.plotly_chart(fig2, use_container_width=True)

        st.divider()
        st.subheader("Detailed Attribute Comparison")
        if 'Assigned Roles' in comparison_df.columns:
            comparison_df['Assigned Roles'] = comparison_df['Assigned Roles'].apply(lambda roles: ', '.join(roles) if isinstance(roles, list) else roles)
        
        # Create a unique display name for the table index
        comparison_df['Display Name'] = comparison_df['Unique ID'].map(player_map)
        
        # Set the unique 'Display Name' as the index before transposing
        st.dataframe(comparison_df.set_index('Display Name').astype(str).T, use_container_width=True)

def dwrs_progress_page(players):
    st.title("DWRS Player Development")
    st.info("Analyze player development trends. Choose an analysis mode to compare positional averages, specific players in a role, or an individual player's progress across their roles.")

    user_club = get_user_club()
    all_players = [p for p in players if p['Club'] == user_club]
    if not all_players:
        st.warning("No players found for your club. Please select your club in the sidebar.")
        return

    st.subheader("1. Choose Analysis Mode")
    analysis_mode = st.selectbox(
        "How would you like to analyze development?",
        ["Squad Overview (by Position)", "Player vs. Player (in a specific role)", "Individual Player (deep dive)"],
        label_visibility="collapsed"
    )
    
    st.subheader("2. Select Your Filters")

    # --- PRONG 1: SQUAD OVERVIEW (Unchanged) ---
    if analysis_mode == "Squad Overview (by Position)":
        pos_categories = {
            'Goalkeepers': ['GK'], 'Defenders': ['DL', 'DC', 'DR', 'WBL', 'WBR'],
            'Midfielders': ['ML', 'MC', 'MR', 'DML', 'DMC', 'DMR'], 'Attackers': ['ST', 'AML', 'AMR', 'AMC']
        }
        selected_cats = st.multiselect("Select positional groups to display", options=pos_categories.keys(), default=list(pos_categories.keys()))
        
        if not selected_cats:
            st.warning("Please select at least one positional group.")
            return

        with st.spinner("Aggregating squad development data..."):
            all_history_dfs = []
            for cat_name in selected_cats:
                cat_player_ids = {p['Unique ID'] for p in all_players if any(p['Position'].startswith(prefix) for prefix in pos_categories[cat_name])}
                if not cat_player_ids: continue
                history_df = get_dwrs_history(list(cat_player_ids))
                if history_df.empty: continue
                history_df['dwrs_normalized'] = pd.to_numeric(history_df['dwrs_normalized'].str.rstrip('%'))
                best_role_per_snapshot = history_df.loc[history_df.groupby(['unique_id', 'snapshot'])['dwrs_normalized'].idxmax()]
                avg_progress = best_role_per_snapshot.groupby('snapshot')['dwrs_normalized'].mean().rename(f"Average - {cat_name}")
                all_history_dfs.append(avg_progress)

        if all_history_dfs:
            chart_data = pd.concat(all_history_dfs, axis=1).interpolate(method='linear', limit_direction='forward', axis=0)
            st.subheader("Average DWRS Progression by Position")
            st.line_chart(chart_data)
        else:
            st.info("No historical data found for the selected positional groups.")

    # --- PRONG 2: PLAYER VS. PLAYER (REFACTORED) ---
    elif analysis_mode == "Player vs. Player (in a specific role)":
        c1, c2 = st.columns(2)
        with c1:
            fav_tactic1, _ = get_favorite_tactics()
            all_tactics = ["All Roles"] + sorted(list(get_tactic_roles().keys()))
            tactic_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
            selected_tactic = st.selectbox("Filter by Tactic", options=all_tactics, index=tactic_index)
        with c2:
            if selected_tactic == "All Roles":
                role_options = get_valid_roles()
            else:
                role_options = sorted(list(set(get_tactic_roles()[selected_tactic].values())))
            selected_role = st.selectbox("Filter by Role", options=role_options, format_func=format_role_display)

        # Filter the player pool based on the selected role
        player_pool = [p for p in all_players if selected_role in p.get('Assigned Roles', [])]
        
        # Create a map of Unique ID -> Display Name to handle duplicate names
        player_map = {p['Unique ID']: f"{p['Name']} ({p['Age']})" for p in player_pool}

        if not player_map:
            st.warning(f"No players in your club have the role '{format_role_display(selected_role)}' assigned.")
            return

        selected_ids = st.multiselect(
            "Select players to compare",
            options=list(player_map.keys()),
            format_func=lambda uid: player_map[uid]
        )
        
        if selected_ids and selected_role:
            history = get_dwrs_history(selected_ids, selected_role)
            
            if not history.empty:
                history['dwrs_normalized'] = pd.to_numeric(history['dwrs_normalized'].str.rstrip('%'))
                # Use the unique display name from player_map for the chart legend
                history['DisplayName'] = history['unique_id'].map(player_map)
                
                pivot = history.pivot_table(index='snapshot', columns='DisplayName', values='dwrs_normalized', aggfunc='mean')
                pivot = pivot.interpolate(method='linear', limit_direction='forward', axis=0)

                st.subheader(f"Development as {format_role_display(selected_role)}")
                st.line_chart(pivot)
            else:
                st.info(f"No historical data found for the selected players in the '{format_role_display(selected_role)}' role.")
        else:
            st.info("Select 2 or more players from the list to see a comparison.")

    # --- PRONG 3: INDIVIDUAL DEEP DIVE (Unchanged) ---
    elif analysis_mode == "Individual Player (deep dive)":
        c1, c2 = st.columns(2)
        with c1:
            player_names = sorted([p['Name'] for p in all_players], key=get_last_name)
            selected_name = st.selectbox("Select a player", options=player_names)
        
        player_obj = next((p for p in all_players if p['Name'] == selected_name), None)
        
        with c2:
            if player_obj:
                role_options = sorted(player_obj.get('Assigned Roles', []), key=format_role_display)
                if role_options:
                    selected_roles = st.multiselect("Select roles to display", options=role_options, default=role_options[:3], format_func=format_role_display)
                else:
                    st.warning("This player has no assigned roles to analyze.")
                    selected_roles = []
            else:
                selected_roles = []

        if player_obj and selected_roles:
            player_id_to_chart = [player_obj['Unique ID']]
            history_dfs = []
            for role in selected_roles:
                history = get_dwrs_history(player_id_to_chart, role)
                if not history.empty:
                    history['dwrs_normalized'] = pd.to_numeric(history['dwrs_normalized'].str.rstrip('%'))
                    history = history.rename(columns={'dwrs_normalized': format_role_display(role)})
                    history_dfs.append(history.set_index('snapshot')[format_role_display(role)])

            if history_dfs:
                chart_data = pd.concat(history_dfs, axis=1).interpolate(method='linear', limit_direction='forward', axis=0)
                st.subheader(f"Development for {selected_name}")
                st.line_chart(chart_data)
            else:
                st.info("No historical data found for the selected roles.")
        else:
            st.info("Select a player and at least one of their roles to see their progress.")

def edit_player_data_page(players):
    st.title("Edit Player Data")
    user_club = get_user_club()
    if not user_club:
        st.warning("Please select your club from the sidebar to use this feature.")
        return

    #all_players = get_all_players()
    all_players = players

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
            current_apt = player.get('Agreed Playing Time')
            
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
    with st.expander("⭐ Favorite Tactic Selection", expanded=True):
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

    with st.expander("📄 Agreed Playing Time (APT) Weights"):
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

    with st.expander("⚖️ DWRS Weights & Multipliers"):
        st.info(
            "These settings form the core of the DWRS calculation. "
            "'Global Weights' define the general importance of a meta category. "
            "'Role Multipliers' boost the value of an attribute if it is 'Key' or 'Preferable' for a specific role, "
            "making the calculation role-sensitive."
        )
        st.subheader("Global Stat Weights")
        new_weights = {cat: st.number_input(f"{cat} Weight", 0.0, 10.0, get_weight(cat.lower().replace(" ", "_"), val), 0.1) for cat, val in { "Extremely Important": 8.0, "Important": 4.0, "Good": 2.0, "Decent": 1.0, "Almost Irrelevant": 0.2 }.items()}
        st.subheader("Goalkeeper Stat Weights")
        new_gk_weights = {cat: st.number_input(f"{cat} Weight", 0.0, 10.0, get_weight("gk_" + cat.lower().replace(" ", "_"), val), 0.1, key=f"gk_{cat}") for cat, val in { "Top Importance": 10.0, "High Importance": 8.0, "Medium Importance": 6.0, "Key": 4.0, "Preferable": 2.0, "Other": 0.5 }.items()}
        st.subheader("Role Multipliers")
        key_mult = st.number_input("Key Attributes Multiplier", 1.0, 20.0, get_role_multiplier('key'), 0.1)
        pref_mult = st.number_input("Preferable Attributes Multiplier", 1.0, 20.0, get_role_multiplier('preferable'), 0.1)

    with st.expander("👶 Surplus Player Age Thresholds"):
        st.info(
            "Define the maximum age for a player to be considered 'youth'. Players at or below (<=) this age will be "
            "suggested for loan, while older players will be suggested for sale/release. This affects the "
            "'Best Position Calculator' and 'Transfer & Loan Management' pages."
        )

        col1, col2 = st.columns(2)
        with col1:
            new_outfielder_age = st.number_input(
                "Outfielder Youth Age", 
                min_value=15, 
                max_value=30, 
                value=get_age_threshold('outfielder'), 
                step=1
            )
        with col2:
            new_goalkeeper_age = st.number_input(
                "Goalkeeper Youth Age", 
                min_value=15, 
                max_value=35, 
                value=get_age_threshold('goalkeeper'), 
                step=1
            )

    with st.expander("⚙️ Database Settings"):
        db_name = st.text_input("Database Name (no .db)", value=get_db_name())

    # This button remains outside the expanders
    if st.button("Save All Settings"):
        set_favorite_tactics(new_fav_tactic1, new_fav_tactic2)
        
        # --- NEW: Save the APT weights ---
        for apt, val in new_apt_weights.items():
            set_apt_weight(apt, val)

        for cat, val in new_weights.items(): set_weight(cat.lower().replace(" ", "_"), val)
        for cat, val in new_gk_weights.items(): set_weight("gk_" + cat.lower().replace(" ", "_"), val)
        set_role_multiplier('key', key_mult)
        set_role_multiplier('preferable', pref_mult)
        set_age_threshold('outfielder', new_outfielder_age)
        set_age_threshold('goalkeeper', new_goalkeeper_age)
        set_db_name(db_name)
        clear_all_caches()
        df = load_data()
        if df is not None: update_dwrs_ratings(df, get_valid_roles())
        st.success("Settings updated successfully!")
        st.rerun()

# --- FIXED: Main function with clear if/elif structure ---
def main():
     # --- START REFACTOR ---
    df = load_data() # This hits the @st.cache_data function once
    players = get_all_players()
    page, uploaded_file = sidebar(df) # Pass df to sidebar
    
    # Use st.query_params to allow linking to specific pages
    query_params = st.query_params
    if "page" in query_params:
        page = query_params["page"][0]
    
    if page == "All Players":
        main_page(uploaded_file, df)
    elif page == "Assign Roles":
        assign_roles_page(df)
    elif page == "Role Analysis":
        role_analysis_page()
    elif page == "Player-Role Matrix":
        player_role_matrix_page()
    elif page == "Best Position Calculator":
        best_position_calculator_page(players)
    elif page == "Transfer & Loan Management":
        transfer_loan_management_page(players)
    elif page == "Player Comparison":
        player_comparison_page(players)
    elif page == "DWRS Progress":
        dwrs_progress_page(players)
    elif page == "Edit Player Data":
        edit_player_data_page(players)
    elif page == "Create New Role":
        create_new_role_page()
    elif page == "Settings":
        settings_page()
    else:
        main_page(uploaded_file) # Default page

if __name__ == "__main__":
    main()