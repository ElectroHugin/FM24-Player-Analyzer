# app.py

import streamlit as st
from streamlit_option_menu import option_menu
import pandas as pd
import os
import re
from io import StringIO
import plotly.graph_objects as go

# --- Cleaned up Imports ---
from data_parser import load_data, parse_and_update_data, get_filtered_players, get_players_by_role, get_player_role_matrix
from sqlite_db import (update_player_roles, get_user_club, set_user_club, update_dwrs_ratings, get_all_players, 
                     get_dwrs_history, get_second_team_club, set_second_team_club, update_player_club, set_primary_role, 
                     update_player_apt, get_favorite_tactics, set_favorite_tactics, 
                     update_player_transfer_status, update_player_loan_status, update_player_natural_positions)
from constants import (SORTABLE_COLUMNS, FILTER_OPTIONS, get_valid_roles, get_position_to_role_mapping, 
                     get_tactic_roles, get_tactic_layouts, FIELD_PLAYER_APT_OPTIONS, GK_APT_OPTIONS, 
                     get_role_specific_weights, GLOBAL_STAT_CATEGORIES, GK_STAT_CATEGORIES, MASTER_POSITION_MAP)
from config_handler import (get_weight, set_weight, get_role_multiplier, set_role_multiplier, 
                          get_db_name, set_db_name, get_apt_weight, set_apt_weight, 
                          get_age_threshold, set_age_threshold, get_theme_settings, save_theme_settings,
                          get_selection_bonus, set_selection_bonus)
from squad_logic import calculate_squad_and_surplus, calculate_development_squads
from ui_components import display_tactic_grid
from definitions_handler import get_definitions, save_definitions, PROJECT_ROOT
from utils import (get_last_name, format_role_display, get_role_display_map, get_available_databases, 
                   calculate_contrast_ratio, hex_to_rgb, parse_position_string)
from theme_handler import set_theme_toml

# --- Page Config ---
st.set_page_config(page_title="FM 2024 Player Dashboard", layout="wide")

def clear_all_caches():
    st.cache_data.clear()

def sidebar(df):
    with st.sidebar:
        # --- NEW: Theme Switch Logic ---
        theme_settings = get_theme_settings()
        current_mode = theme_settings.get('current_mode', 'night')

        # --- NEW: Centered Logo & Fallback Text ---
        # We use columns to center the logo and header.
        col1, col2, col3 = st.columns([1, 2, 1]) # [Spacer, Content, Spacer]
        with col2:
            logo_path = os.path.join(PROJECT_ROOT, 'config', 'assets', 'logo.png')
            if os.path.exists(logo_path):
                st.image(logo_path) # st.image in a column is auto-centered
            else:
                # The header is also placed in the central column
                st.header("FM Dashboard")

        # --- Get colors for the nav bar ---
        # --- UPDATED: Uses the new theme_handler function ---
        primary_color = theme_settings.get(f"{current_mode}_primary_color")
        secondary_color = theme_settings.get(f"{current_mode}_text_color")
        rgb = hex_to_rgb(primary_color)
        hover_color = f"rgba({rgb[0]}, {rgb[1]}, {rgb[2]}, 0.15)"

        # Convert hex primary color to an RGBA with low opacity for the hover effect
        rgb = hex_to_rgb(primary_color)
        hover_color = f"rgba({rgb[0]}, {rgb[1]}, {rgb[2]}, 0.15)"
        
        # This is the improved option_menu navigation
        page = option_menu(
            menu_title="Navigation",
            options=["Dashboard", "Assign Roles", "Role Analysis", "Squad Matrix", "Best XI", "Transfers", "Comparison", "Development", "Edit Player", "New Role", "New Tactic", "Settings"],
            icons=["house", "person-plus", "search", "table", "trophy", "arrow-left-right", "people", "graph-up", "pencil-square", "person-badge", "clipboard-plus", "gear"],
            menu_icon="cast",
            default_index=0,
            styles={
                "container": {"padding": "5px !important", "background-color": "transparent"},
                "icon": {"color": secondary_color, "font-size": "20px"},
                "nav-link": {"font-size": "16px", "text-align": "left", "margin":"0px", "--hover-color": hover_color},
                "nav-link-selected": {"background-color": primary_color},
            }
        )
        page_mapping = { "Dashboard": "All Players", "Assign Roles": "Assign Roles", "Role Analysis": "Role Analysis", "Squad Matrix": "Player-Role Matrix", "Best XI": "Best Position Calculator", "Transfers": "Transfer & Loan Management", "Comparison": "Player Comparison", "Development": "DWRS Progress", "Edit Player": "Edit Player Data", "New Role": "Create New Role", "New Tactic": "Create New Tactic", "Settings": "Settings" }
        actual_page = page_mapping.get(page, "All Players")

        st.divider()
        uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
        club_options = ["Select a club"] + sorted(df['Club'].unique()) if df is not None else ["Select a club"]
        current_club = get_user_club() or "Select a club"
        club_index = 0
        if current_club in club_options:
            club_index = club_options.index(current_club)
        selected_club = st.selectbox("Your Club", options=club_options, index=club_index)

        if selected_club != current_club and selected_club != "Select a club":
            set_user_club(selected_club)
            st.rerun()
        
        current_second = get_second_team_club() or "Select a club"
        selected_second = st.selectbox("Your Second Team", options=club_options, index=club_options.index(current_second) if current_second in club_options else 0)

        if selected_second != current_second and selected_second != "Select a club":
            set_second_team_club(selected_second)
            st.rerun()

        # The toggle's state reflects the current mode
        is_day_mode = st.toggle("☀️ Day Mode", value=(current_mode == 'day'))
        
        # Check if the toggle was flipped by the user
        new_mode = 'day' if is_day_mode else 'night'
        if new_mode != current_mode:
            theme_settings['current_mode'] = new_mode
            save_theme_settings(theme_settings)
            
            # Apply the new theme immediately
            if new_mode == 'day':
                set_theme_toml(
                    theme_settings['day_primary_color'],
                    theme_settings['day_text_color'],
                    theme_settings['day_background_color'],
                    theme_settings['day_secondary_background_color']
                )
            else: # night mode
                set_theme_toml(
                    theme_settings['night_primary_color'],
                    theme_settings['night_text_color'],
                    theme_settings['night_background_color'],
                    theme_settings['night_secondary_background_color']
                )
            st.rerun()
            
        return actual_page, uploaded_file

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

    # --- 1. SETUP & INITIAL DATA FETCH (Done once for both tabs) ---
    user_club = get_user_club()
    second_team_club = get_second_team_club()

    if not user_club:
        st.warning("Please select your club in the sidebar.")
        return

    # Create player pools
    my_club_players = [p for p in players if p.get('Club') == user_club]
    second_team_players = [p for p in players if p.get('Club') == second_team_club] if second_team_club else []

    @st.cache_data
    def _get_master_role_ratings(club_list):
        master_ratings = {}
        # Get all players from the relevant clubs to calculate ratings
        all_relevant_players = [p for p in get_all_players() if p.get('Club') in club_list]
        if not all_relevant_players: return {}
        
        for role in get_valid_roles():
            # Pass all relevant clubs to the function
            ratings_df, _, _ = get_players_by_role(role, club_list[0], club_list[1] if len(club_list) > 1 else None)
            if not ratings_df.empty:
                ratings_df['DWRS'] = pd.to_numeric(ratings_df['DWRS Rating (Normalized)'].str.rstrip('%'))
                # Ensure we only consider players from the specified clubs
                valid_ratings = ratings_df[ratings_df['Club'].isin(club_list)]
                master_ratings[role] = valid_ratings.set_index('Unique ID')['DWRS'].to_dict()
        return master_ratings
    
    def create_detailed_surplus_df(player_list, master_role_ratings):
        if not player_list:
            return pd.DataFrame()

        data = []
        for player in player_list:
            best_dwrs = 0
            best_role_abbr = ''
            # Find the best DWRS rating among the player's assigned roles
            assigned_roles = player.get('Assigned Roles', [])
            if assigned_roles:
                for role in assigned_roles:
                    rating = master_role_ratings.get(role, {}).get(player['Unique ID'], 0)
                    if rating > best_dwrs:
                        best_dwrs = rating
                        best_role_abbr = role
            
            player_data = {
                "Name": player['Name'],
                "Age": player.get('Age', 'N/A'),
                "Position": player.get('Position', 'N/A'),
                "Best Role": format_role_display(best_role_abbr) if best_role_abbr else "N/A",
                "Best DWRS": int(best_dwrs),
                "Det": player.get('Determination', 'N/A'),
                "Wor": player.get('Work Rate', 'N/A'),
                "Transfer": "✅" if player.get('transfer_status', 0) else "❌",
                "Loan": "✅" if player.get('loan_status', 0) else "❌",
            }
            data.append(player_data)
        
        return pd.DataFrame(data)

    # Tactic selection
    fav_tactic1, fav_tactic2 = get_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    sorted_tactics = []
    if fav_tactic1 and fav_tactic1 in all_tactics: sorted_tactics.append(fav_tactic1)
    if fav_tactic2 and fav_tactic2 in all_tactics and fav_tactic2 != fav_tactic1: sorted_tactics.append(fav_tactic2)
    for tactic in all_tactics:
        if tactic not in sorted_tactics: sorted_tactics.append(tactic)
    tactic = st.selectbox("Select Tactic", options=sorted_tactics, index=0)
    
    positions, layout = get_tactic_roles()[tactic], get_tactic_layouts()[tactic]
    theme_settings = get_theme_settings()
    current_mode = theme_settings.get('current_mode', 'night')

    # --- 2. PERFORM ALL CALCULATIONS (Done once, before displaying UI) ---
    with st.spinner("Calculating all team structures..."):
        # We need ratings for both your club and the second team club for all calculations
        clubs_to_rate = [user_club]
        if second_team_club:
            clubs_to_rate.append(second_team_club)
        
        master_role_ratings = _get_master_role_ratings(clubs_to_rate)

        # First, calculate the main squad as before
        first_team_squad_data = calculate_squad_and_surplus(my_club_players, positions, master_role_ratings)

        # Then, use those results to calculate the development squads
        dev_squad_data = calculate_development_squads(
            second_team_players, 
            first_team_squad_data["depth_pool"], 
            positions,           
            master_role_ratings  
        )

    # --- 3. CREATE UI ---
    
    # Define the column configuration ONCE to be reused everywhere
    column_config = {
        "Best DWRS": st.column_config.NumberColumn(format="%d%%"),
        "Det": st.column_config.TextColumn(help="Determination"),
        "Wor": st.column_config.TextColumn(help="Work Rate"),
    }

    tab1, tab2 = st.tabs(["First Team Squad", "Youth & Second Team"])

    # --- All content for the first tab goes inside this "with" block ---
    with tab1:
        st.header("First Team Analysis")
        with st.expander("How does it work?"):
            st.info("This tool uses a 'weakest link first' algorithm. Instead of picking the best player for each position one by one, it evaluates all open positions simultaneously and fills the one where the best available player provides the smallest upgrade. This creates a more balanced and often stronger overall team.")

        xi_col, b_team_col = st.columns(2)
        with xi_col:
            display_tactic_grid(first_team_squad_data["starting_xi"], "Starting XI", positions, layout, mode=current_mode)
        with b_team_col:
            display_tactic_grid(first_team_squad_data["b_team"], "B Team", positions, layout, mode=current_mode)
        
        st.divider()

        st.subheader("Additional Depth")
        best_depth_options = first_team_squad_data["best_depth_options"]
        if best_depth_options:
            for pos in sorted(best_depth_options.keys()):
                players_list = best_depth_options.get(pos, [])
                if players_list:
                    player_strs = [f"{p['name']} ({p['rating']}){' - ' + p['apt'] if p['apt'] else ''}" for p in players_list]
                    display_str = ', '.join(player_strs)
                    st.markdown(f"**{pos} ({positions[pos]})**: {display_str}")
        else:
            st.info("No other players were suitable as depth options for this tactic.")

    # --- All content for the second tab goes inside this "with" block ---
    with tab2:
        with st.expander("Development Squad Analysis"):
            st.info("This section displays the optimal Youth XI from your young players and the best XI for your Second Team based on the selected tactic.")

        second_col, youth_col = st.columns(2)

        with second_col:
            second_team_title = f"Second Team XI ({second_team_club or 'From Main Club Surplus'})"
            display_tactic_grid(dev_squad_data["second_team_xi"], second_team_title, positions, layout, mode=current_mode)
        
        with youth_col:
            display_tactic_grid(dev_squad_data["youth_xi"], "Youth XI", positions, layout, mode=current_mode)

        st.divider()
        st.subheader("Development & Transfer Candidates")
        st.success("✔️ To manage these players, go to the **Transfer & Loan Management** page in the sidebar!")

        loan_candidates_df = create_detailed_surplus_df(dev_squad_data.get("loan_candidates", []), master_role_ratings)
        if not loan_candidates_df.empty:
            st.markdown(f"**Promising Youth for Loan (Work Rate + Determination >= 20):**")
            st.dataframe(loan_candidates_df, hide_index=True, use_container_width=True, column_config=column_config)
        else:
            st.info("No promising youth players identified for loan.")

        sell_candidates_df = create_detailed_surplus_df(dev_squad_data.get("sell_candidates", []), master_role_ratings)
        if not sell_candidates_df.empty:
            st.markdown(f"**Surplus Players for Sale / Release:**")
            st.dataframe(sell_candidates_df, hide_index=True, use_container_width=True, column_config=column_config)
        else:
            st.info("No surplus players identified for sale.")

  
def transfer_loan_management_page(players):
    st.title("Transfer & Loan Management")
    st.info("Manage the definitive list of surplus players based on your selected tactic. These are the players who did not make the First, B, Second, or Youth teams.")

    # --- Helper functions for color-coding ---
    def color_attribute(value_str):
        try:
            value = int(value_str)
            if value >= 13: color = '#85f585'
            elif value >= 10: color = '#f5f585'
            else: color = '#f58585'
            return f"<span style='color: {color}; font-weight: bold;'>{value}</span>"
        except (ValueError, TypeError): return "N/A"

    def color_age(age_str):
        try:
            age = int(age_str)
            color = '#f58585' if age <= 17 else 'white'
            return f"<span style='color: {color};'>{age}</span>"
        except (ValueError, TypeError): return "N/A"
        
    # --- 1. SETUP & TACTIC SELECTION ---
    user_club = get_user_club()
    second_team_club = get_second_team_club()
    if not user_club:
        st.warning("Please select your club in the sidebar to use this feature.")
        return

    fav_tactic1, _ = get_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    try:
        default_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
    except ValueError: default_index = 0
    tactic = st.selectbox("Select Tactic to Analyze Surplus Players", options=all_tactics, index=default_index)
    positions = get_tactic_roles()[tactic]

    # --- 2. PERFORM THE DEFINITIVE SURPLUS CALCULATION ---
    with st.spinner("Calculating definitive surplus lists..."):
        my_club_players = [p for p in players if p.get('Club') == user_club]
        second_team_players = [p for p in players if p.get('Club') == second_team_club] if second_team_club else []

        clubs_to_rate = [user_club]
        if second_team_club: clubs_to_rate.append(second_team_club)
        
        master_ratings = {}
        for role in get_valid_roles():
            ratings_df, _, _ = get_players_by_role(role, user_club, second_team_club)
            if not ratings_df.empty:
                ratings_df['DWRS'] = pd.to_numeric(ratings_df['DWRS Rating (Normalized)'].str.rstrip('%'))
                master_ratings[role] = ratings_df.set_index('Unique ID')['DWRS'].to_dict()

        first_team_squad_data = calculate_squad_and_surplus(my_club_players, positions, master_ratings)
        dev_squad_data = calculate_development_squads(
            second_team_players,
            first_team_squad_data["depth_pool"],
            positions,
            master_ratings
        )
        
        loan_candidates = dev_squad_data.get("loan_candidates", [])
        sell_candidates = dev_squad_data.get("sell_candidates", [])

        for player in (loan_candidates + sell_candidates):
            best_dwrs, best_role_abbr = 0, ''
            for role in player.get('Assigned Roles', []):
                rating = master_ratings.get(role, {}).get(player['Unique ID'], 0)
                if rating > best_dwrs:
                    best_dwrs, best_role_abbr = rating, role
            player['Best DWRS'] = f"{int(best_dwrs)}%"
            player['Best Role Abbr'] = best_role_abbr
    
    # --- 3. DISPLAY MANAGEMENT UI ---
    def display_management_table(player_list, title, is_youth=False):
        st.subheader(title)
        if not player_list:
            st.info(f"No players in this category for the '{tactic}' tactic.")
            return

        cols = [2.5, 0.5, 1.5, 0.5, 0.5, 0.8, 0.8, 2, 1] if is_youth else [3, 0.5, 1.5, 0.8, 0.8, 2, 1]
        headers = ["Name", "Age", "Best DWRS (Role)", "Det", "Wor", "Transfer", "Loan", "New Club", "Action"] if is_youth else ["Name", "Age", "Best DWRS (Role)", "Transfer", "Loan", "New Club", "Action"]

        header_cols = st.columns(cols)
        for i, header in enumerate(headers):
            header_cols[i].markdown(f"**{header}**")
        st.markdown("---")

        for player in player_list:
            uid = player['Unique ID']
            row_cols = st.columns(cols)
            if f"club_input_{uid}" not in st.session_state: st.session_state[f"club_input_{uid}"] = ""
            
            best_role_display = f"({player.get('Best Role Abbr', '')})" if player.get('Best Role Abbr') else ""
            dwrs_display = f"{player.get('Best DWRS', 'N/A')} {best_role_display}"
            
            if is_youth:
                row_cols[0].write(player['Name'])
                row_cols[1].markdown(color_age(player.get('Age')), unsafe_allow_html=True)
                row_cols[2].write(dwrs_display)
                row_cols[3].markdown(color_attribute(player.get('Determination')), unsafe_allow_html=True)
                row_cols[4].markdown(color_attribute(player.get('Work Rate')), unsafe_allow_html=True)
                # --- FIX START ---
                row_cols[5].checkbox("Transfer Status", value=bool(player.get('transfer_status', 0)), key=f"transfer_{uid}", label_visibility="collapsed")
                row_cols[6].checkbox("Loan Status", value=bool(player.get('loan_status', 0)), key=f"loan_{uid}", label_visibility="collapsed")
                row_cols[7].text_input("New Club Name", key=f"club_input_{uid}", label_visibility="collapsed", placeholder="New club...")
                # --- FIX END ---
                if row_cols[8].button("Save", key=f"save_{uid}"):
                    update_player_transfer_status(uid, st.session_state[f"transfer_{uid}"])
                    update_player_loan_status(uid, st.session_state[f"loan_{uid}"])
                    if st.session_state[f"club_input_{uid}"].strip():
                        update_player_club(uid, st.session_state[f"club_input_{uid}"].strip())
                    st.toast(f"Saved changes for {player['Name']}!", icon="✔️")
            else: # Senior players
                row_cols[0].write(player['Name'])
                row_cols[1].write(player.get('Age', 'N/A'))
                row_cols[2].write(dwrs_display)
                # --- FIX START ---
                row_cols[3].checkbox("Transfer Status", value=bool(player.get('transfer_status', 0)), key=f"transfer_{uid}", label_visibility="collapsed")
                row_cols[4].checkbox("Loan Status", value=bool(player.get('loan_status', 0)), key=f"loan_{uid}", label_visibility="collapsed")
                row_cols[5].text_input("New Club Name", key=f"club_input_{uid}", label_visibility="collapsed", placeholder="New club...")
                # --- FIX END ---
                if row_cols[6].button("Save", key=f"save_{uid}"):
                    update_player_transfer_status(uid, st.session_state[f"transfer_{uid}"])
                    update_player_loan_status(uid, st.session_state[f"loan_{uid}"])
                    if st.session_state[f"club_input_{uid}"].strip():
                        update_player_club(uid, st.session_state[f"club_input_{uid}"].strip())
                    st.toast(f"Saved changes for {player['Name']}!", icon="✔️")
        
        st.markdown("---")
        if st.button(f"Save All Changes for this List", key=f"save_all_{title.replace(' ', '_')}", type="primary"):
            with st.spinner(f"Saving all players in '{title}'..."):
                for p in player_list:
                    p_uid = p['Unique ID']
                    update_player_transfer_status(p_uid, st.session_state[f"transfer_{p_uid}"])
                    update_player_loan_status(p_uid, st.session_state[f"loan_{p_uid}"])
                    if st.session_state[f"club_input_{p_uid}"].strip():
                        update_player_club(p_uid, st.session_state[f"club_input_{p_uid}"].strip())
            st.success(f"All changes for players in '{title}' have been saved!")
            st.rerun()

    outfielder_age = get_age_threshold('outfielder')
    gk_age = get_age_threshold('goalkeeper')
    
    display_management_table(loan_candidates, f"For Loan (Promising Youth)", is_youth=True)
    st.divider()
    display_management_table(sell_candidates, f"For Sale / Release")
def player_comparison_page(players):
    st.title("Player Comparison")

    # --- START: THEME-AWARE SETUP ---
    theme_settings = get_theme_settings()
    current_mode = theme_settings.get('current_mode', 'night')

    # Define chart colors and palettes based on the current mode
    if current_mode == 'day':
        # Day Mode: Dark text, light backgrounds, saturated trace colors
        chart_bg_color = 'rgba(230, 230, 230, 0.5)' # Light gray background
        font_color = theme_settings.get('day_text_color', '#31333F')
        grid_color = 'rgba(0, 0, 0, 0.2)'
        # Palette designed for high contrast on light backgrounds
        primary_color = theme_settings.get('day_primary_color', '#0055a4')
        trace_palette = [primary_color, '#D32F2F', '#7B1FA2', '#0288D1', '#FFA000']
    else:
        # Night Mode: Light text, dark backgrounds, bright trace colors
        chart_bg_color = 'rgba(46, 46, 46, 0.8)' # Original dark gray
        font_color = theme_settings.get('night_text_color', '#FFFFFF')
        grid_color = 'rgba(255, 255, 255, 0.4)'
        # Palette designed for high contrast on dark backgrounds
        primary_color = theme_settings.get('night_primary_color', '#0055a4')
        trace_palette = [primary_color, '#F50057', '#00E5FF', '#FFDE03', '#76FF03']
    # --- END: THEME-AWARE SETUP ---
    
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
        
        
        all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]
        is_gk_role = selected_role in all_gk_roles

        role_weights = get_role_specific_weights().get(selected_role, {"key": [], "preferable": []})
        key_attrs = role_weights["key"]
        pref_attrs = role_weights["preferable"]

        if is_gk_role:
            gameplay_attrs = { 'Shot Stopping': ['Reflexes', 'One vs One', 'Handling', 'Agility'], 'Aerial Control': ['Aerial Reach', 'Command of Area', 'Jumping Reach'], 'Distribution': ['Kicking', 'Throwing', 'Passing', 'Vision'], 'Sweeping': ['Rushing Out (Tendency)', 'Acceleration', 'Pace'], 'Mental': ['Composure', 'Concentration', 'Decisions', 'Anticipation']}
            meta_categories = { "Top Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "Top Importance"], "High Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "High Importance"], "Medium Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "Medium Importance"], "Key": key_attrs, "Preferable": pref_attrs}
            meta_chart_title = "GK Meta-Attribute Profile"
        else:
            gameplay_attrs = { 'Pace': ['Acceleration', 'Pace'], 'Shooting': ['Finishing', 'Long Shots'], 'Passing': ['Passing', 'Crossing', 'Vision'], 'Dribbling': ['Dribbling', 'First Touch', 'Flair'], 'Defending': ['Tackling', 'Marking', 'Positioning'], 'Physical': ['Strength', 'Stamina', 'Balance'], 'Mental': ['Work Rate', 'Determination', 'Teamwork', 'Decisions']}
            meta_categories = { "Extremely Important": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Extremely Important"], "Important": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Important"], "Good": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Good"], "Key": key_attrs, "Preferable": pref_attrs}
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

            # --- FIX: Define the theta values for the loop ONCE ---
            gameplay_theta = list(gameplay_attrs.keys())

            # --- UPDATED: Loop to build chart with dynamic colors ---
            for i, uid in enumerate(selected_ids):
                player_data = comparison_df[comparison_df['Unique ID'] == uid].iloc[0]
                category_values = [sum(parse_attribute_value(player_data.get(attr, 0)) for attr in attrs) / len(attrs) if attrs else 0 for attrs in gameplay_attrs.values()]
                
                
                # --- FIX: Append the first value to the end to CLOSE the shape ---
                if category_values:
                    category_values.append(category_values[0])

                # Assign a color from the palette, looping if necessary
                color = trace_palette[i % len(trace_palette)]
                
                fig1.add_trace(go.Scatterpolar(
                    r=category_values, 
                    # --- FIX: Use a closed list of labels that matches the data ---
                    theta=gameplay_theta + [gameplay_theta[0]], 
                    fill='toself', 
                    name=player_map[uid],
                    line=dict(color=color),
                    fillcolor=f"rgba({','.join(str(c) for c in hex_to_rgb(color))}, 0.2)"
                ))
            
            # --- UPDATED: Dynamic layout styling ---
            fig1.update_layout(
                polar=dict(
                    radialaxis=dict(visible=True, range=[0, 20], tickfont=dict(color=font_color), gridcolor=grid_color),
                    angularaxis=dict(tickfont=dict(size=12, color=font_color), direction="clockwise"),
                    bgcolor=chart_bg_color
                ),
                showlegend=False, 
                paper_bgcolor='rgba(0,0,0,0)', 
                plot_bgcolor='rgba(0,0,0,0)', 
                margin=dict(l=40, r=40, t=40, b=40)
            )
            st.plotly_chart(fig1, use_container_width=True)

        with chart_col2:
            st.subheader(meta_chart_title)
            fig2 = go.Figure()
            # --- FIX: Define the theta values for the loop ONCE ---
            meta_theta = list(meta_categories.keys())

            # --- UPDATED: Loop to build chart with dynamic colors ---
            for i, uid in enumerate(selected_ids):
                player_data = comparison_df[comparison_df['Unique ID'] == uid].iloc[0]
                category_values = [sum(parse_attribute_value(player_data.get(attr, 0)) for attr in attrs) / len(attrs) if attrs else 0 for attrs in meta_categories.values()]

                # --- FIX: Append the first value to the end to CLOSE the shape ---
                if category_values:
                    category_values.append(category_values[0])

                color = trace_palette[i % len(trace_palette)]

                fig2.add_trace(go.Scatterpolar(
                    r=category_values, 
                    # --- FIX: Use a closed list of labels that matches the data ---
                    theta=meta_theta + [meta_theta[0]], 
                    fill='toself', 
                    name=player_map[uid],
                    line=dict(color=color),
                    fillcolor=f"rgba({','.join(str(c) for c in hex_to_rgb(color))}, 0.2)"
                ))

            # --- UPDATED: Dynamic layout styling ---
            fig2.update_layout(
                polar=dict(
                    radialaxis=dict(visible=True, range=[0, 20], tickfont=dict(color=font_color), gridcolor=grid_color),
                    angularaxis=dict(tickfont=dict(size=12, color=font_color), direction="clockwise"),
                    bgcolor=chart_bg_color
                ),
                legend=dict(font=dict(color=font_color)), 
                paper_bgcolor='rgba(0,0,0,0)', 
                plot_bgcolor='rgba(0,0,0,0)', 
                margin=dict(l=40, r=40, t=40, b=40)
            )
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
    st.info("Analyze player development trends. Choose an analysis mode to compare squad averages by role, specific players, or an individual player's progress.")

    user_club = get_user_club()
    all_players = [p for p in players if p['Club'] == user_club]
    if not all_players:
        st.warning("No players found for your club. Please select your club in the sidebar.")
        return

    st.subheader("1. Choose Analysis Mode")
    analysis_mode = st.selectbox(
        "How would you like to analyze development?",
        ["Squad Overview (by Role)", "Player vs. Player (in a specific role)", "Individual Player (deep dive)"],
        label_visibility="collapsed"
    )
    
    st.subheader("2. Select Your Filters")

    # --- PRONG 1: SQUAD OVERVIEW (COMPLETELY REBUILT) ---
    if analysis_mode == "Squad Overview (by Role)":
        fav_tactic1, _ = get_favorite_tactics()
        all_tactics = ["All Roles"] + sorted(list(get_tactic_roles().keys()))
        tactic_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
        selected_tactic = st.selectbox(
            "Select a Tactic to Analyze its Roles",
            options=all_tactics,
            index=tactic_index
        )

        if selected_tactic == "All Roles":
            # If 'All Roles', the options are all valid roles in the game
            tactic_roles = get_valid_roles()
        else:
            # Otherwise, get the unique roles for the selected tactic
            tactic_roles = sorted(list(set(get_tactic_roles()[selected_tactic].values())))
        
        selected_roles = st.multiselect(
            "Select roles to display on the chart",
            options=tactic_roles,
            default=tactic_roles, # Default to showing all roles from the tactic
            format_func=format_role_display
        )

        if not selected_roles:
            st.warning("Please select at least one role to display.")
            return

        with st.spinner("Aggregating squad development data by role..."):
            all_history_dfs = []
            for role in selected_roles:
                # 1. Find all players in your club who have this role assigned
                player_ids_for_role = {p['Unique ID'] for p in all_players if role in p.get('Assigned Roles', [])}
                
                if not player_ids_for_role:
                    continue # Skip this role if no players can play it

                # 2. Get the historical DWRS data for these players IN THIS SPECIFIC ROLE
                history_df = get_dwrs_history(list(player_ids_for_role), role)
                
                if history_df.empty:
                    continue

                # 3. Calculate the squad's average DWRS for this role at each snapshot
                history_df['dwrs_normalized'] = pd.to_numeric(history_df['dwrs_normalized'].str.rstrip('%'))
                avg_progress = history_df.groupby('snapshot')['dwrs_normalized'].mean()
                
                # 4. Rename the series for a clean chart legend
                avg_progress = avg_progress.rename(format_role_display(role))
                all_history_dfs.append(avg_progress)

        if all_history_dfs:
            chart_data = pd.concat(all_history_dfs, axis=1).interpolate(method='linear', limit_direction='forward', axis=0)
            st.subheader(f"Average Squad DWRS Progression for Roles in '{selected_tactic}'")
            st.line_chart(chart_data)
        else:
            st.info("No historical data found for any players in the selected roles.")

    # --- PRONG 2: PLAYER VS. PLAYER (Unchanged) ---
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

        player_pool = [p for p in all_players if selected_role in p.get('Assigned Roles', [])]
        player_map = {p['Unique ID']: f"{p['Name']} ({p['Age']})" for p in player_pool}

        if not player_map:
            st.warning(f"No players in your club have the role '{format_role_display(selected_role)}' assigned.")
            return

        selected_ids = st.multiselect("Select players to compare", options=list(player_map.keys()), format_func=lambda uid: player_map[uid])
        
        if selected_ids and selected_role:
            history = get_dwrs_history(selected_ids, selected_role)
            if not history.empty:
                history['dwrs_normalized'] = pd.to_numeric(history['dwrs_normalized'].str.rstrip('%'))
                history['DisplayName'] = history['unique_id'].map(player_map)
                pivot = history.pivot_table(index='snapshot', columns='DisplayName', values='dwrs_normalized', aggfunc='mean').interpolate(method='linear', limit_direction='forward', axis=0)
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

    all_players = players
    if not all_players:
        st.info("No players loaded.")
        return

    player_to_edit = None

    # --- Player Selection Section (remains full-width at the top) ---
    c1, c2 = st.columns([1, 1]) # Use columns to neatly separate the two selection methods
    with c1:
        st.subheader("Select Player from Your Club")
        st.caption("Marked with: 🎯 Primary Role, 📄 APT, 📍 Natural Pos.")
        
        my_club_players = sorted([p for p in all_players if p['Club'] == user_club], key=lambda p: get_last_name(p['Name']))
        
        player_options_map = {}
        dropdown_options = ["--- Select a Player ---"]
        for player in my_club_players:
            markers = []
            if not bool(player.get('primary_role')): markers.append('🎯')
            if not bool(player.get('Agreed Playing Time')): markers.append('📄')
            if not bool(player.get('natural_positions')): markers.append('📍')
            marker = f" {' '.join(markers)}" if markers else ""
            display_name = f"{player['Name']}{marker}"
            dropdown_options.append(display_name)
            player_options_map[display_name] = player['Unique ID']

        selected_dropdown_option = st.selectbox("My Club Players", options=dropdown_options, index=0, label_visibility="collapsed")
        if selected_dropdown_option != "--- Select a Player ---":
            player_id = player_options_map[selected_dropdown_option]
            player_to_edit = next((p for p in all_players if p['Unique ID'] == player_id), None)

    with c2:
        st.subheader("Or, Search All Players")
        st.caption("")
        
        search = st.text_input("Search for a player by name", label_visibility="collapsed")
        if search and not player_to_edit:
            results = [p for p in all_players if search.lower() in p['Name'].lower()]
            if results:
                search_options_map = {f"{p['Name']} ({p['Club']})": p for p in results}
                selected_search_option = st.selectbox("Select a player from search results", options=list(search_options_map.keys()))
                if selected_search_option:
                    player_to_edit = search_options_map[selected_search_option]
            else:
                st.warning("No players found with that name.")
    
    st.divider()

    # --- Main player editing form organized into columns ---
    if player_to_edit:
        player = player_to_edit
        st.write(f"### Editing: {player['Name']}")

        # Column 1: Club Info & Tactical Roles
        col1, col2 = st.columns(2)

        with col1:
            st.subheader("Administrative Info")
            
            # --- Update Club ---
            new_club = st.text_input("Club", value=player['Club'], key=f"club_{player['Unique ID']}")
            if st.button("Save Club Change"):
                update_player_club(player['Unique ID'], new_club)
                clear_all_caches()
                st.success(f"Updated club to '{new_club}'.")
                st.rerun()

            # --- Agreed Playing Time (only for user's club) ---
            if player['Club'] == user_club:
                is_gk = "GK" in player.get('Position', '')
                apt_options = GK_APT_OPTIONS if is_gk else FIELD_PLAYER_APT_OPTIONS
                current_apt = player.get('Agreed Playing Time')
                apt_index = apt_options.index(current_apt) if current_apt in apt_options else 0
                new_apt = st.selectbox("Agreed Playing Time", options=apt_options, index=apt_index, key=f"apt_{player['Unique ID']}")
                if st.button("Save Playing Time"):
                    value_to_save = None if new_apt == "None" else new_apt
                    update_player_apt(player['Unique ID'], value_to_save)
                    clear_all_caches()
                    st.success(f"Set Agreed Playing Time to '{new_apt}'.")
                    st.rerun()

        with col2:
            # --- This section only appears for the user's club players ---
            if player['Club'] == user_club:
                st.subheader("Tactical Profile")

                # --- Set Natural Positions ---
                player_position_string = player.get('Position', '')
                player_specific_positions = sorted(list(parse_position_string(player_position_string)))
                current_natural_positions = player.get('natural_positions', [])
                
                if player_specific_positions:
                    new_natural_positions = st.multiselect(
                        "Natural Positions", 
                        options=player_specific_positions,
                        default=current_natural_positions,
                        key=f"nat_pos_{player['Unique ID']}",
                        help="Select the positions this player is most effective in."
                    )
                    if st.button("Save Natural Positions"):
                        update_player_natural_positions(player['Unique ID'], new_natural_positions)
                        clear_all_caches()
                        st.success(f"Updated natural positions for {player['Name']}.")
                        st.rerun()
                else:
                    st.warning("This player has no positions listed.")

                # --- Set Primary Role ---
                role_options = ["None"] + sorted(player.get('Assigned Roles', []))
                current_role = player.get('primary_role')
                role_index = role_options.index(current_role) if current_role in role_options else 0
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

def create_new_tactic_page():
    st.title("Create a New Tactical Formation")
    st.info("Design your formation on the pitch below. Use the dropdowns to select a role for each active position. You must select exactly one Goalkeeper and ten outfield players.")

    # --- Load necessary definitions ---
    definitions = get_definitions()
    pos_to_role_map = get_position_to_role_mapping()
    role_display_map = get_role_display_map()

    with st.form("new_tactic_form"):
        # --- Tactic Naming ---
        st.subheader("1. Tactic Name")
        c1, c2 = st.columns([2, 1])
        # --- CHANGE: Added explicit keys for easier clearing ---
        formation_name = c1.text_input("Formation Name (e.g., 'My Counter Press')", help="The descriptive name for your tactic.", key="new_tactic_name")
        formation_shape = c2.text_input("Formation Shape (e.g., '4-2-3-1')", help="The numerical shape, like '4-4-2' or '4-2-3-1'. This becomes part of the final name.", key="new_tactic_shape")

        final_tactic_name = f"{formation_shape} {formation_name}".strip() if formation_name else ""
        if final_tactic_name:
            st.write(f"**Full Tactic Name:** `{final_tactic_name}`")

        st.divider()
        st.subheader("2. Design Your Formation")

        # --- VISUAL PITCH LAYOUT USING STREAMLIT COLUMNS ---
        with st.container():
            st.markdown("""
                <style>
                div[data-testid="stVerticalBlock"] > div[style*="flex-direction: column;"] > div[data-testid="stHorizontalBlock"] {
                    background-color: #2a5d34;
                    border: 1px solid #ccc;
                    border-radius: 10px;
                    padding: 15px 10px;
                }
                </style>
            """, unsafe_allow_html=True)

            # --- Strikers (3 positions, centered) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Strikers</p>", unsafe_allow_html=True)
            s_cols = st.columns(5)
            with s_cols[1]:
                selections = {'STL': st.selectbox("STL", ["- Unused -"] + sorted(pos_to_role_map.get("ST (C)", []), key=role_display_map.get), key="role_STL", format_func=format_role_display)}
            with s_cols[2]:
                selections['STC'] = st.selectbox("STC", ["- Unused -"] + sorted(pos_to_role_map.get("ST (C)", []), key=role_display_map.get), key="role_STC", format_func=format_role_display)
            with s_cols[3]:
                selections['STR'] = st.selectbox("STR", ["- Unused -"] + sorted(pos_to_role_map.get("ST (C)", []), key=role_display_map.get), key="role_STR", format_func=format_role_display)

            # --- Attacking Midfield (5 positions) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Attacking Midfield</p>", unsafe_allow_html=True)
            am_cols = st.columns(5)
            selections['AML'] = am_cols[0].selectbox("AML", ["- Unused -"] + sorted(pos_to_role_map.get("AM (L)", []), key=role_display_map.get), key="role_AML", format_func=format_role_display)
            selections['AMCL'] = am_cols[1].selectbox("AMCL", ["- Unused -"] + sorted(pos_to_role_map.get("AM (C)", []), key=role_display_map.get), key="role_AMCL", format_func=format_role_display)
            selections['AMC'] = am_cols[2].selectbox("AMC", ["- Unused -"] + sorted(pos_to_role_map.get("AM (C)", []), key=role_display_map.get), key="role_AMC", format_func=format_role_display)
            selections['AMCR'] = am_cols[3].selectbox("AMCR", ["- Unused -"] + sorted(pos_to_role_map.get("AM (C)", []), key=role_display_map.get), key="role_AMCR", format_func=format_role_display)
            selections['AMR'] = am_cols[4].selectbox("AMR", ["- Unused -"] + sorted(pos_to_role_map.get("AM (R)", []), key=role_display_map.get), key="role_AMR", format_func=format_role_display)

            # --- Midfield (5 positions) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Midfield</p>", unsafe_allow_html=True)
            m_cols = st.columns(5)
            selections['ML'] = m_cols[0].selectbox("ML", ["- Unused -"] + sorted(pos_to_role_map.get("M (L)", []), key=role_display_map.get), key="role_ML", format_func=format_role_display)
            selections['MCL'] = m_cols[1].selectbox("MCL", ["- Unused -"] + sorted(pos_to_role_map.get("M (C)", []), key=role_display_map.get), key="role_MCL", format_func=format_role_display)
            selections['MC'] = m_cols[2].selectbox("MC", ["- Unused -"] + sorted(pos_to_role_map.get("M (C)", []), key=role_display_map.get), key="role_MC", format_func=format_role_display)
            selections['MCR'] = m_cols[3].selectbox("MCR", ["- Unused -"] + sorted(pos_to_role_map.get("M (C)", []), key=role_display_map.get), key="role_MCR", format_func=format_role_display)
            selections['MR'] = m_cols[4].selectbox("MR", ["- Unused -"] + sorted(pos_to_role_map.get("M (R)", []), key=role_display_map.get), key="role_MR", format_func=format_role_display)

            # --- Defensive Midfield (5 positions) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Defensive Midfield</p>", unsafe_allow_html=True)
            dm_cols = st.columns(5)
            selections['DML'] = dm_cols[0].selectbox("DML", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DML", format_func=format_role_display)
            selections['DMCL'] = dm_cols[1].selectbox("DMCL", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DMCL", format_func=format_role_display)
            selections['DMC'] = dm_cols[2].selectbox("DMC", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DMC", format_func=format_role_display)
            selections['DMCR'] = dm_cols[3].selectbox("DMCR", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DMCR", format_func=format_role_display)
            selections['DMR'] = dm_cols[4].selectbox("DMR", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DMR", format_func=format_role_display)

            # --- Defense (5 positions) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Defense</p>", unsafe_allow_html=True)
            d_cols = st.columns(5)
            selections['DL'] = d_cols[0].selectbox("DL/WBL", ["- Unused -"] + sorted(pos_to_role_map.get("D (L)", []) + pos_to_role_map.get("WB (L)", []), key=role_display_map.get), key="role_DL", format_func=format_role_display)
            selections['DCL'] = d_cols[1].selectbox("DCL", ["- Unused -"] + sorted(pos_to_role_map.get("D (C)", []), key=role_display_map.get), key="role_DCL", format_func=format_role_display)
            selections['DC'] = d_cols[2].selectbox("DC", ["- Unused -"] + sorted(pos_to_role_map.get("D (C)", []), key=role_display_map.get), key="role_DC", format_func=format_role_display)
            selections['DCR'] = d_cols[3].selectbox("DCR", ["- Unused -"] + sorted(pos_to_role_map.get("D (C)", []), key=role_display_map.get), key="role_DCR", format_func=format_role_display)
            selections['DR'] = d_cols[4].selectbox("DR/WBR", ["- Unused -"] + sorted(pos_to_role_map.get("D (R)", []) + pos_to_role_map.get("WB (R)", []), key=role_display_map.get), key="role_DR", format_func=format_role_display)

            # --- Goalkeeper (Mandatory) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Goalkeeper</p>", unsafe_allow_html=True)
            gk_roles = [role for role, name in get_role_display_map().items() if "GK" in role or "SK" in role]
            selected_gk_role = st.selectbox("Goalkeeper Role", options=gk_roles, label_visibility="collapsed", format_func=format_role_display, key="role_GK")

        st.divider()
        submitted = st.form_submit_button("Create New Tactic", type="primary")

    if submitted:
        with st.spinner("Validating and saving new tactic..."):
            # --- Validation (reading from session_state for reliability) ---
            final_tactic_name = f"{st.session_state.new_tactic_shape} {st.session_state.new_tactic_name}".strip()
            
            if not final_tactic_name or not st.session_state.new_tactic_name or not st.session_state.new_tactic_shape:
                st.error("Validation Failed: Please provide both a Formation Name and a Formation Shape.")
                return

            if final_tactic_name in definitions['tactic_roles']:
                st.error(f"Validation Failed: A tactic named '{final_tactic_name}' already exists. Please choose a different name.")
                return

            all_selections = {key.split('_')[1]: value for key, value in st.session_state.items() if key.startswith("role_")}
            outfield_players = {pos: role for pos, role in all_selections.items() if role != "- Unused -" and pos != "GK"}
            
            if len(outfield_players) != 10:
                st.error(f"Validation Failed: You must select exactly 10 outfield players. You have selected {len(outfield_players)}.")
                return

            # --- Data Structuring ---
            new_tactic_roles = {"GK": st.session_state.role_GK}
            new_tactic_roles.update(outfield_players)

            new_tactic_layout = {}
            for pos_key in outfield_players.keys():
                stratum, _ = MASTER_POSITION_MAP.get(pos_key, (None, None))
                if pos_key in ["ST", "STC"]: stratum = "Strikers"

                if stratum:
                    if stratum not in new_tactic_layout: new_tactic_layout[stratum] = []
                    new_tactic_layout[stratum].append(pos_key)

            # --- Saving ---
            definitions['tactic_roles'][final_tactic_name] = new_tactic_roles
            definitions['tactic_layouts'][final_tactic_name] = new_tactic_layout
            success, message = save_definitions(definitions)

            if success:
                st.success(f"Tactic '{final_tactic_name}' created successfully! Reloading application...")

                # ------------------- START OF NEW CODE -------------------
                # This block will clear all the form's widget states.
                
                # Find all keys associated with this form's widgets
                keys_to_delete = [k for k in st.session_state.keys() if k.startswith("role_") or k in ["new_tactic_name", "new_tactic_shape"]]
                
                # Safely delete each key from the session state
                for key in keys_to_delete:
                    if key in st.session_state:
                        del st.session_state[key]
                # -------------------- END OF NEW CODE --------------------

                clear_all_caches()
                st.rerun()
            else:
                st.error(f"Failed to save tactic: {message}")

def settings_page():
    # --- Fetch current theme settings once at the top ---
    theme_settings = get_theme_settings()
    current_mode = theme_settings.get('current_mode', 'night')

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

    with st.expander("🎨 Club Identity & Theme"):
        st.info(f"You are currently customizing the **{current_mode.capitalize()} Mode** theme. Use the toggle in the sidebar to switch modes.")
        st.info("Upload your club's logo and select its primary and secondary colors to personalize the app's theme.")

        logo_file = st.file_uploader("Upload Club Logo", type=['png', 'jpg', 'jpeg'], help="Recommended size: 200x200 pixels.")
        if logo_file is not None:
            try:
                assets_dir = os.path.join(PROJECT_ROOT, 'config', 'assets')
                os.makedirs(assets_dir, exist_ok=True)
                with open(os.path.join(assets_dir, "logo.png"), "wb") as f:
                    f.write(logo_file.getbuffer())
                st.success("Logo uploaded successfully!")
            except Exception as e:
                st.error(f"Error saving logo: {e}")

        # --- NEW: Two Color Pickers ---
        st.subheader(f"{current_mode.capitalize()} Mode Colors")
        
        # Dynamically get the colors for the current mode
        primary_val = theme_settings.get(f"{current_mode}_primary_color")
        text_val = theme_settings.get(f"{current_mode}_text_color")
        bg_val = theme_settings.get(f"{current_mode}_background_color")
        sec_bg_val = theme_settings.get(f"{current_mode}_secondary_background_color")
        c1, c2 = st.columns(2)
        with c1:
            new_primary = st.color_picker("Primary Color (Buttons, Highlights)", primary_val)
            new_bg = st.color_picker("Background Color", bg_val)
        with c2:
            new_text = st.color_picker("Text & Accent Color", text_val)
            new_sec_bg = st.color_picker("Secondary Background Color", sec_bg_val)
        
        # --- NEW: Live Contrast Warning ---
        st.caption("The secondary color is used for most text. For best readability, ensure it has strong contrast with the app's dark background and your primary color.")
        
        # --- Live Contrast Warning ---
        bg_contrast_ratio = calculate_contrast_ratio(new_text, new_bg)
        primary_contrast_ratio = calculate_contrast_ratio(new_text, new_primary)
        
        if bg_contrast_ratio < 4.5 or primary_contrast_ratio < 3.0:
            st.warning(f"""
                **Low Contrast Warning!**
                - Text on background contrast: {bg_contrast_ratio:.2f}:1 (Aim for 4.5:1)
                - Text on primary color contrast: {primary_contrast_ratio:.2f}:1 (Aim for 3:1)
                
                Text may be difficult to read with this combination.
            """)

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
                new_apt_weights[apt] = st.number_input(f"Weight for '{apt}'", 0.0, 5.0, current_weight, 0.01, key=f"apt_{apt}")
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
        st.subheader("Role & Position Multipliers")
        c1, c2, c3 = st.columns(3)
        with c1:
            key_mult = st.number_input("Key Attributes Multiplier", 1.0, 20.0, get_role_multiplier('key'), 0.1)
        with c2:
            pref_mult = st.number_input("Preferable Attr. Multiplier", 1.0, 20.0, get_role_multiplier('preferable'), 0.1)
        with c3:
            natural_pos_mult = st.number_input(
                "Natural Position Bonus", 
                min_value=1.0, 
                max_value=2.0, 
                value=get_selection_bonus('natural_position'), 
                step=0.01, # <-- Using 0.01 for finer control like 1.05
                help="Bonus multiplier applied in the Best XI calculator if a player is in one of their 'Natural Positions'. 1.0 = No bonus."
            )


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

# --- START: NEW DATABASE SETTINGS SECTION ---
    with st.expander("⚙️ Database Settings"):
        db_action = st.radio("Action", ["Select Existing Database", "Create New Database"], horizontal=True)
        
        current_db_name = get_db_name()
        db_to_set = current_db_name # Default to current DB
        is_valid_new_db = False

        if db_action == "Select Existing Database":
            available_dbs = get_available_databases()
            if not available_dbs:
                st.warning("No existing databases found. Create one below.")
            else:
                try:
                    current_index = available_dbs.index(current_db_name)
                except ValueError:
                    current_index = 0
                
                selected_db = st.selectbox("Select a database", options=available_dbs, index=current_index)
                db_to_set = selected_db

        else: # Create New Database
            new_db_name = st.text_input("Enter new database name (e.g., 'My FM24 Save')")
            if new_db_name:
                # Sanitize the name to create a valid filename
                sanitized_name = re.sub(r'[^\w\s-]', '', new_db_name).strip()
                if not sanitized_name:
                    st.error("Invalid name. Please use letters, numbers, spaces, or hyphens.")
                elif sanitized_name in get_available_databases():
                    st.error(f"A database named '{sanitized_name}' already exists.")
                else:
                    st.success(f"Ready to create and switch to '{sanitized_name}.db' on save.")
                    db_to_set = sanitized_name
                    is_valid_new_db = True
    # --- END: NEW DATABASE SETTINGS SECTION ---

    # This button remains outside the expanders
    if st.button("Save All Settings", type="primary"):
        set_favorite_tactics(new_fav_tactic1, new_fav_tactic2)

        # --- Update the theme settings dictionary with new values ---
        theme_settings[f"{current_mode}_primary_color"] = new_primary
        theme_settings[f"{current_mode}_text_color"] = new_text
        theme_settings[f"{current_mode}_background_color"] = new_bg
        theme_settings[f"{current_mode}_secondary_background_color"] = new_sec_bg

        # Save the updated dictionary back to config.ini
        save_theme_settings(theme_settings)

        # Apply the just-saved settings to the live theme
        set_theme_toml(new_primary, new_text, new_bg, new_sec_bg)
        
        # --- NEW: Save the APT weights ---
        for apt, val in new_apt_weights.items():
            set_apt_weight(apt, val)

        for cat, val in new_weights.items(): set_weight(cat.lower().replace(" ", "_"), val)
        for cat, val in new_gk_weights.items(): set_weight("gk_" + cat.lower().replace(" ", "_"), val)
        set_role_multiplier('key', key_mult)
        set_role_multiplier('preferable', pref_mult)
        set_age_threshold('outfielder', new_outfielder_age)
        set_age_threshold('goalkeeper', new_goalkeeper_age)
        set_selection_bonus('natural_position', natural_pos_mult)

        # --- START: NEW DATABASE SAVE LOGIC ---
        # Save only if the selection is a valid new DB name or different from the current one
        if (db_action == "Create New Database" and is_valid_new_db) or (db_action == "Select Existing Database" and db_to_set != current_db_name):
            set_db_name(db_to_set)
            st.toast(f"Switched active database to '{db_to_set}.db'", icon="💾")
        # --- END: NEW DATABASE SAVE LOGIC ---

        clear_all_caches()
        df = load_data()
        if df is not None: update_dwrs_ratings(df, get_valid_roles())
        st.success("Settings saved successfully!")
        st.info("Theme changes may require a full app restart (Ctrl+C in terminal and `streamlit run app.py`) to apply correctly.")
        st.rerun()

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
    elif page == "Create New Tactic":
        create_new_tactic_page()
    elif page == "Settings":
        settings_page()
    else:
        main_page(uploaded_file) # Default page

if __name__ == "__main__":
    main()