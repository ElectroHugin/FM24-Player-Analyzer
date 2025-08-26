# app.py

import streamlit as st
import pandas as pd
from streamlit_option_menu import option_menu
import os

from page_views.settings import settings_page
from page_views.new_tactic import create_new_tactic_page
from page_views.new_role import create_new_role_page
from page_views.edit_player import edit_player_data_page
from page_views.dwrs_progress import dwrs_progress_page
from page_views.player_comparison import player_comparison_page
from page_views.transfer_loan_management import transfer_loan_management_page
from page_views.best_position import best_position_calculator_page  
from page_views.player_role_matrix import player_role_matrix_page
from page_views.role_analysis import role_analysis_page
from page_views.assign_roles import assign_roles_page

from data_parser import load_data, parse_and_update_data
from sqlite_db import (get_second_team_club, set_second_team_club, get_user_club, set_user_club, get_all_players, update_dwrs_ratings,
                        get_favorite_tactics)
from constants import get_valid_roles, get_tactic_roles
from config_handler import save_theme_settings, get_theme_settings
from ui_components import clear_all_caches, player_quick_edit_dialog
from data_parser import get_player_role_matrix
from definitions_handler import PROJECT_ROOT
from squad_logic import get_cached_squad_analysis
from utils import  hex_to_rgb, format_role_display
from theme_handler import set_theme_toml

st.set_page_config(page_title="FM 2024 Player Dashboard", layout="wide")

def sidebar(df, players):
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

        if 'player_to_edit_id' not in st.session_state:
            st.session_state.player_to_edit_id = None

        # --- START: GLOBAL PLAYER SEARCH ---
        st.subheader("Global Player Search")
        search_query = st.text_input("Find Player by Name", label_visibility="collapsed", placeholder="Find Player by Name...")
        if search_query:
            # Filter players based on the search query (case-insensitive)
            results = [p for p in players if search_query.lower() in p['Name'].lower()]
            
            if results:
                # Display up to 5 results as buttons in the sidebar
                for player in results[:5]:
                    if st.button(f"{player['Name']} ({player['Club']})", key=f"search_result_{player['Unique ID']}", use_container_width=True):
                        st.session_state.player_to_edit_id = player['Unique ID']
                        st.rerun() # Rerun to open the dialog
            else:
                st.caption("No players found.")

        if st.session_state.player_to_edit_id:
            # Find the full player dictionary
            player_object = next((p for p in players if p['Unique ID'] == st.session_state.player_to_edit_id), None)
            if player_object:
                # Pass the player object to the dialog function
                player_quick_edit_dialog(player_object, get_user_club())
            else:
                # If player not found, reset the state
                st.session_state.player_to_edit_id = None
        # --- END: GLOBAL PLAYER SEARCH ---

        st.divider()
        #uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
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
            
        return actual_page


def main_page(uploaded_file, df, players): # Add 'players' to the function signature
    st.title("Club Dashboard")
    user_club = get_user_club()

    # --- 1. DEDICATED UPLOAD SECTION ---
    with st.expander("⬆️ Upload New Player Data"):
        with st.form("upload_form", clear_on_submit=True):
            uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
            submitted = st.form_submit_button("Process File")

            if submitted and uploaded_file is not None:
                with st.spinner("Processing file..."):
                    df_new = parse_and_update_data(uploaded_file)
                if df_new is None:
                    st.error("Invalid HTML file: Must contain a table with a 'UID' column.")
                else:
                    clear_all_caches()
                    update_dwrs_ratings(df_new, get_valid_roles())
                    st.success("Data updated successfully!")
                    st.rerun() # Rerun to reflect changes everywhere

    if df is None or not user_club:
        st.info("Please upload a player data file and select 'Your Club' in the sidebar to view the dashboard.")
        return

    # --- 2. TACTIC SELECTION FOR DASHBOARD ANALYSIS ---
    st.subheader("Dashboard Analysis")
    fav_tactic1, _ = get_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    try:
        default_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
    except ValueError:
        default_index = 0
    
    selected_tactic = st.selectbox("Analyze Squad based on Tactic:", options=all_tactics, index=default_index)

    # --- 3. GET SQUAD DATA FROM THE CACHED FUNCTION ---
    analysis_results = None # Initialize to None
    with st.spinner(f"Analyzing your squad's fit for the '{selected_tactic}' tactic..."):
        second_team_club = get_second_team_club()
        analysis_results = get_cached_squad_analysis(players, selected_tactic, user_club, second_team_club)

    if not analysis_results or analysis_results["core_squad_df"].empty:
        st.warning(f"Could not generate a squad for the '{selected_tactic}' tactic. There may be no suitable players in your club.")
        # We can still show the main player table
        st.subheader(f"Players at {user_club}")
        my_club_df = df[df['Club'] == user_club]
        st.dataframe(my_club_df, use_container_width=True, hide_index=True)
        return
        
    core_squad_df = analysis_results["core_squad_df"]

    if core_squad_df is None or core_squad_df.empty:
        st.warning(f"Could not generate a squad for the '{selected_tactic}' tactic. There may be no suitable players in your club.")
        return
        
    # --- 4. DISPLAY KPIS (Key Performance Indicators) ---
    st.markdown("---")
    st.subheader(f"Core Squad Overview (Starting XI + B-Team for '{selected_tactic}')")
    
    # Convert value strings like '€1.2M' to numbers
    def value_to_float(value_str):
        if not isinstance(value_str, str):
            return 0.0

        # Handle ranges by taking the lower value
        if ' - ' in value_str:
            value_str = value_str.split(' - ')[0]

        value_str = value_str.replace('€', '').strip()
        
        try:
            if 'M' in value_str:
                return float(value_str.replace('M', '')) * 1_000_000
            elif 'K' in value_str:
                return float(value_str.replace('K', '')) * 1_000
            # Handle cases with no K/M suffix or empty strings
            return float(value_str) if value_str else 0.0
        except ValueError:
            # If after all cleaning it's still not a valid number, return 0
            return 0.0

    core_squad_df['Transfer Value Num'] = core_squad_df['Transfer Value'].apply(value_to_float)
    core_squad_df['Age'] = pd.to_numeric(core_squad_df['Age'], errors='coerce')
    
    total_value = core_squad_df['Transfer Value Num'].sum()
    avg_value = core_squad_df['Transfer Value Num'].mean()
    avg_age = core_squad_df['Age'].mean()

    col1, col2, col3, col4 = st.columns(4)
    col1.metric("Players in Core Squad", f"{len(core_squad_df)}")
    col2.metric("Total Squad Value", f"€{total_value/1_000_000:.2f}M")
    col3.metric("Average Player Value", f"€{avg_value/1_000_000:.2f}M")
    col4.metric("Average Age", f"{avg_age:.1f}")

    # --- 5. ROLE PROFICIENCY CHART & SQUAD TABLE ---
    st.markdown("---")
    chart_col, table_col = st.columns([2, 3]) # Give more space to the table

    with chart_col:
        st.subheader("Role Proficiency")
        
        # Calculate average DWRS for each role in the selected tactic
        roles_tactic = sorted(list(set(get_tactic_roles()[selected_tactic].values())))
        role_ratings = []
        
        # We need the full matrix to get DWRS scores
        full_matrix = get_player_role_matrix(user_club)
        if not full_matrix.empty:
            # Filter the matrix to only include our core squad players
            core_squad_matrix = full_matrix[full_matrix['Name'].isin(core_squad_df['Name'])]
            
            for role in roles_tactic:
                if role in core_squad_matrix.columns:
                    avg_dwrs = core_squad_matrix[role].mean()
                    role_ratings.append({'Role': format_role_display(role), 'Average DWRS': avg_dwrs})
        
        if role_ratings:
            ratings_df = pd.DataFrame(role_ratings).sort_values('Average DWRS', ascending=False)
            st.bar_chart(ratings_df, x='Role', y='Average DWRS')
        else:
            st.info("No rating data available for these roles.")

    with table_col:
        st.subheader(f"Players at {user_club}")
        my_club_df = df[df['Club'] == user_club]
        st.dataframe(my_club_df, use_container_width=True, hide_index=True)


def main():
     # --- START REFACTOR ---
    df = load_data() # This hits the @st.cache_data function once
    players = get_all_players()
    page = sidebar(df, players) 
    
    # Use st.query_params to allow linking to specific pages
    query_params = st.query_params
    if "page" in query_params:
        page = query_params["page"][0]
    
    if page == "All Players":
        main_page(None, df, players)
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
        main_page(None, df, players)

if __name__ == "__main__":
    main()