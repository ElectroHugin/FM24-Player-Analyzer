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
from page_views.national_squad_selection import national_squad_selection_page
from page_views.national_squad_matrix import national_squad_matrix_page
from page_views.national_best_xi import national_best_xi_page

from data_parser import load_data, parse_and_update_data
from sqlite_db import (get_second_team_club, set_second_team_club, get_user_club, set_user_club, get_all_players, update_dwrs_ratings,
                        get_favorite_tactics, get_national_mode_enabled)
from constants import get_valid_roles, get_tactic_roles
from config_handler import save_theme_settings, get_theme_settings
from ui_components import clear_all_caches, display_strength_grid, display_custom_header
from data_parser import get_player_role_matrix
from definitions_handler import PROJECT_ROOT
from squad_logic import get_cached_squad_analysis
from utils import  hex_to_rgb, format_role_display, value_to_float
from theme_handler import set_theme_toml
from role_logic import auto_assign_roles_to_unassigned

st.set_page_config(page_title="FM 2024 Player Dashboard", layout="wide")

def sidebar(df, players):
    with st.sidebar:
        # --- Check if National Mode is enabled ---
        is_national_mode_enabled = get_national_mode_enabled()
        
        if 'management_mode' not in st.session_state:
            st.session_state.management_mode = "Club Management"

        # --- STEP 1: GET THEME COLORS FIRST ---
        # We move this block to the top so the tab buttons can use the colors.
        theme_settings = get_theme_settings()
        current_mode = theme_settings.get('current_mode', 'night')
        primary_color = theme_settings.get(f"{current_mode}_primary_color")
        secondary_color = theme_settings.get(f"{current_mode}_text_color")
        rgb = hex_to_rgb(primary_color)
        hover_color = f"rgba({rgb[0]}, {rgb[1]}, {rgb[2]}, 0.15)"
        # --- END OF STEP 1 ---

        if is_national_mode_enabled:
            selected_mode = option_menu(
                menu_title=None,
                options=["Club Management", "National Management"],
                icons=["shield-shaded", "flag"],
                menu_icon="cast", 
                default_index=0 if st.session_state.management_mode == "Club Management" else 1,
                orientation="horizontal",
                 # --- STEP 2: USE DYNAMIC COLORS IN THE STYLES ---
                 styles={
                    "container": {"padding": "0!important", "background-color": "transparent"},
                    # Use the dynamic secondary_color for the icon and text
                    "icon": {"color": secondary_color, "font-size": "16px"},
                    "nav-link": {
                        "font-size": "14px", 
                        "text-align": "center", 
                        "margin":"0px", 
                        # Use the dynamic hover_color
                        "--hover-color": hover_color,
                        "color": secondary_color # Explicitly set text color
                    },
                    # Use the dynamic primary_color for the selected tab's background
                    "nav-link-selected": {"background-color": primary_color},
                }
                 # --- END OF STEP 2 ---
            )
            if selected_mode != st.session_state.management_mode:
                st.session_state.management_mode = selected_mode
                st.rerun()
            st.divider()

        # --- Dynamic Logo Display (no changes here) ---
        col1, col2, col3 = st.columns([1, 2, 1])
        with col2:
            if st.session_state.management_mode == "National Management":
                logo_path = os.path.join(PROJECT_ROOT, 'config', 'assets', 'flag.png')
                header_text = "Please upload flag..."
            else: 
                logo_path = os.path.join(PROJECT_ROOT, 'config', 'assets', 'logo.png')
                header_text = "Please upload logo..."

            if os.path.exists(logo_path):
                st.image(logo_path)
            else:
                st.header(header_text)
                default_logo_path = os.path.join(PROJECT_ROOT, 'config', 'assets', 'default.png')
                st.image(default_logo_path)
        
        # --- REVISED DYNAMIC NAVIGATION MENU ---
        if st.session_state.management_mode == "National Management":
            page_options = ["National Squad", "Squad Matrix", "Best XI", "Role Analysis", "Development"]
            page_icons = ["people-fill", "table", "trophy", "search", "graph-up"]
            page_title = "National Team"
            page_mapping = { "National Squad": "National Squad Selection", "Squad Matrix": "National Squad Matrix", "Best XI": "National Best XI", "Role Analysis": "National Role Analysis", "Development": "National Development" }
        else: # Club Management
            page_options = ["Dashboard", "Assign Roles", "Role Analysis", "Squad Matrix", "Best XI", "Transfers", "Comparison", "Development", "Edit Player", "New Role", "New Tactic"]
            page_icons = ["house", "person-plus", "search", "table", "trophy", "arrow-left-right", "people", "graph-up", "pencil-square", "person-badge", "clipboard-plus"]
            page_title = "Club Navigation"
            page_mapping = { "Dashboard": "All Players", "Assign Roles": "Assign Roles", "Role Analysis": "Role Analysis", "Squad Matrix": "Player-Role Matrix", "Best XI": "Best Position Calculator", "Transfers": "Transfer & Loan Management", "Comparison": "Player Comparison", "Development": "DWRS Progress", "Edit Player": "Edit Player Data", "New Role": "Create New Role", "New Tactic": "Create New Tactic"}

        # --- THIS IS THE CHANGE: Add Settings to BOTH modes ---
        page_options.append("Settings")
        page_icons.append("gear")
        page_mapping["Settings"] = "Settings"

        page = option_menu(
            menu_title=page_title,
            options=page_options,
            icons=page_icons,
            menu_icon="list-ul",
            default_index=0,
            styles={
                "container": {"padding": "5px !important", "background-color": "transparent"},
                "icon": {"color": secondary_color, "font-size": "20px"},
                "nav-link": {"font-size": "16px", "text-align": "left", "margin":"0px", "--hover-color": hover_color},
                "nav-link-selected": {"background-color": primary_color},
            }
        )
        
        actual_page = page_mapping.get(page)

        # --- Club Selectors (no changes here) ---
        if st.session_state.management_mode == "Club Management":
            st.divider()
            club_options = ["Select a club"] + sorted(df['Club'].unique()) if df is not None else ["Select a club"]
            current_club = get_user_club() or "Select a club"
            club_index = club_options.index(current_club) if current_club in club_options else 0
            selected_club = st.selectbox("Your Club", options=club_options, index=club_index)

            if selected_club != current_club and selected_club != "Select a club":
                set_user_club(selected_club)
                st.rerun()
            
            current_second = get_second_team_club() or "Select a club"
            selected_second = st.selectbox("Your Second Team", options=club_options, index=club_options.index(current_second) if current_second in club_options else 0)

            if selected_second != current_second and selected_second != "Select a club":
                set_second_team_club(selected_second)
                st.rerun()

        # --- Theme Toggle (no changes here) ---
        st.divider()
        is_day_mode = st.toggle("☀️ Day Mode", value=(current_mode == 'day'))
        new_mode = 'day' if is_day_mode else 'night'
        if new_mode != current_mode:
            theme_settings['current_mode'] = new_mode
            save_theme_settings(theme_settings)
            set_theme_toml(
                theme_settings[f"{new_mode}_primary_color"],
                theme_settings[f"{new_mode}_text_color"],
                theme_settings[f"{new_mode}_background_color"],
                theme_settings[f"{new_mode}_secondary_background_color"]
            )
            st.rerun()
            
        return actual_page


def main_page(uploaded_file, df, players): # Add 'players' to the function signature
    display_custom_header("Dashboard")
    user_club = get_user_club()

    # --- 1. DEDICATED UPLOAD SECTION (IMPROVED) ---
    with st.expander("⬆️ Upload New Player Data"):
        with st.form("upload_form", clear_on_submit=True):
            uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
            
            # --- NEW: Add the auto-assign checkbox ---
            auto_assign = st.checkbox("Automatically assign roles to new/unassigned players", value=True, help="After uploading, the app will automatically assign default roles to any player who doesn't have them, based on their positions.")
            
            submitted = st.form_submit_button("Process File")

            if submitted and uploaded_file is not None:
                with st.spinner("Processing file..."):
                    # Step 1: Save player attributes to the database
                    full_df, affected_ids = parse_and_update_data(uploaded_file)
                
                if full_df is None:
                    st.error("Invalid HTML file: Must contain a table with a 'UID' column.")
                else:
                    # Step 2 (Optional): Auto-assign roles if the box is checked
                    if auto_assign:

                        with st.spinner("Auto-assigning roles to new players..."):
                            num_assigned = auto_assign_roles_to_unassigned()
                        st.toast(f"Assigned roles to {num_assigned} players.", icon="✨")

                    # Step 3: Now, calculate DWRS for everyone.
                    # We reload the data to make sure we have the newly assigned roles.
                    with st.spinner(f"Calculating DWRS for {len(affected_ids)} updated players..."):
                        clear_all_caches()
                        # We need the most up-to-date data for the calculation
                        final_df = load_data()
                        if final_df is not None:
                            # Pass the affected_ids to the function
                            update_dwrs_ratings(final_df, get_valid_roles(), affected_ids)

                    st.success("Data updated and ratings calculated successfully!")
                    st.rerun()

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

    core_squad_df['Transfer Value Num'] = core_squad_df['Transfer Value'].apply(value_to_float)
    core_squad_df['Age'] = pd.to_numeric(core_squad_df['Age'], errors='coerce')
    
    # Create a clean version of the value column specifically for summing,
    # where "Not for Sale" (the huge number) is replaced with 0.
    UNBUYABLE_VALUE = 2_000_000_000
    core_squad_df['Value For Sum'] = core_squad_df['Transfer Value Num'].replace(UNBUYABLE_VALUE, 0.0)
    
    # Now, use this new, clean column for all calculations.
    total_value = core_squad_df['Value For Sum'].sum()
    avg_value = core_squad_df['Value For Sum'].mean()
    avg_age = core_squad_df['Age'].mean()

    col1, col2, col3, col4 = st.columns(4)
    col1.metric("Players in Core Squad", f"{len(core_squad_df)}")
    col2.metric("Total Squad Value", f"€{total_value/1_000_000:.2f}M")
    col3.metric("Average Player Value", f"€{avg_value/1_000_000:.2f}M")
    col4.metric("Average Age", f"{avg_age:.1f}")

    # --- 5. POSITIONAL STRENGTH & SQUAD TABLE ---
    st.markdown("---")
    strength_col, table_col = st.columns([2, 3])

    with strength_col:
        
        positional_strengths = {}
        tactic_positions = get_tactic_roles()[selected_tactic]
        xi_squad = analysis_results["first_team_squad_data"]["starting_xi"]
        b_team_squad = analysis_results["first_team_squad_data"]["b_team"]
        depth_options = analysis_results["first_team_squad_data"]["best_depth_options"]

        for pos_key, role in tactic_positions.items():
            ratings = []
            def add_rating(player_obj):
                if player_obj and player_obj.get('name') != '-':
                    try: ratings.append(int(player_obj['rating'].rstrip('%')))
                    except (ValueError, TypeError): pass

            add_rating(xi_squad.get(pos_key))
            add_rating(b_team_squad.get(pos_key))
            for player in depth_options.get(role, []): add_rating(player)

            if ratings:
                positional_strengths[pos_key] = {'avg': sum(ratings) / len(ratings), 'min': min(ratings), 'max': max(ratings)}
            else:
                positional_strengths[pos_key] = {'avg': 0, 'min': 0, 'max': 0}

        current_theme_mode = get_theme_settings().get('current_mode', 'night')
        display_strength_grid(positional_strengths, selected_tactic, mode=current_theme_mode)

    with table_col:
        st.subheader(f"Players at {user_club}")
        my_club_df = df[df['Club'] == user_club]
        st.dataframe(my_club_df, use_container_width=True, hide_index=True)

    # ------------------- START OF NEW TRANSFER SUGGESTIONS SECTION -------------------
    st.markdown("---")
    st.subheader("🎯 Transfer Targets")
    st.info("Discover potential upgrades from your scouted players list based on the roles in your current tactic.")

    # --- 1. Get the complete player-role matrix data ---
    full_matrix = get_player_role_matrix(user_club, second_team_club)
    
    if full_matrix.empty:
        st.warning("No player matrix data available. Please upload player data.")
        return

    # --- 2. Prepare the data for filtering and comparison ---
    my_club_matrix = full_matrix[full_matrix['Club'] == user_club].copy()
    
    exclude_clubs = [user_club]
    if second_team_club: exclude_clubs.append(second_team_club)
    scouted_matrix = full_matrix[~full_matrix['Club'].isin(exclude_clubs)].copy()

    # Convert columns to numeric for filtering
    scouted_matrix['AgeNum'] = pd.to_numeric(scouted_matrix['Age'], errors='coerce')
    scouted_matrix['ValueNum'] = scouted_matrix['Transfer Value'].apply(value_to_float)
    scouted_matrix.dropna(subset=['AgeNum', 'ValueNum'], inplace=True) # Drop players with no age/value

    # --- 3. UI Filters (Sliders) ---
    filter_c1, filter_c2 = st.columns(2)
    with filter_c1:
        max_age = st.slider("Maximum Age", 15, 40, 28)
    with filter_c2:
        # --- 2. Make the slider robust to the sentinel value ---
        # Create a temporary dataframe of only buyable players to set the slider's max
        buyable_players = scouted_matrix[scouted_matrix['ValueNum'] < 2_000_000_000]
        max_val_possible = buyable_players['ValueNum'].max() if not buyable_players.empty else 100_000_000
        
        slider_max = min(max_val_possible, 200_000_000)

        max_val_slider = st.slider(
            "Maximum Transfer Value (€ Millions)", 
            min_value=0.0, 
            max_value=slider_max / 1_000_000, 
            value=(10.0), 
            step=0.5
        ) * 1_000_000

    # --- 4. Core Logic: Find Upgrades ---
    filtered_scouts = scouted_matrix[
        (scouted_matrix['AgeNum'] <= max_age) & 
        (scouted_matrix['ValueNum'] <= max_val_slider)
    ]

    roles_tactic = sorted(list(set(get_tactic_roles()[selected_tactic].values())))
    suggestions = []

    for role in roles_tactic:
        if role not in my_club_matrix.columns or role not in filtered_scouts.columns:
            continue

        my_best_rating = my_club_matrix[role].max()
        # If we have no players for a role, any scouted player is an upgrade
        if pd.isna(my_best_rating): my_best_rating = 0 

        potential_upgrades = filtered_scouts[filtered_scouts[role] > my_best_rating]
        
        if not potential_upgrades.empty:
            # Find the best of the potential upgrades
            best_upgrade = potential_upgrades.loc[potential_upgrades[role].idxmax()]
            suggestions.append({
                "role": role,
                "player": best_upgrade['Name'],
                "rating": best_upgrade[role],
                "value_str": best_upgrade['Transfer Value'],
                "age": best_upgrade['Age'],
                "club": best_upgrade['Club'],
                "position": best_upgrade['Position']
            })

    # --- 5. Display Suggestions in a Visually Appealing Way ---
    if not suggestions:
        st.success("✅ Your squad is looking strong! No clear upgrades found within your filter criteria.")
    else:
        # Determine the number of columns (max 4)
        num_cols = min(len(suggestions), 4)
        cols = st.columns(num_cols)
        
        for i, sug in enumerate(suggestions):
            with cols[i % num_cols]:
                with st.container(border=True):
                    st.markdown(f"**{sug['player']}**")
                    st.markdown(f"Upgrade for **{format_role_display(sug['role'])}**")
                    st.divider()
                    st.markdown(f"🎯 **Rating:** {sug['rating']:.0f}% (Your Best: {my_club_matrix[sug['role']].max():.0f}%)")
                    st.markdown(f"💰 **Value:** {sug['value_str']}")
                    st.markdown(f"🎂 **Age:** {sug['age']}")
                    st.caption(f"Club: {sug['club']}")
                    st.caption(f"Positions: {sug['position']}")

    # -------------------- END OF NEW TRANSFER SUGGESTIONS SECTION --------------------


def main():
    df = load_data() 
    players = get_all_players()
    page = sidebar(df, players) 
    
    query_params = st.query_params
    if "page" in query_params:
        page = query_params["page"][0]
    
    # --- UPDATED ROUTER TO HANDLE ALL PAGES ---
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
    # --- NEW: National Page Routing ---
    elif page == "National Squad Selection":
        national_squad_selection_page(players)
    elif page == "National Squad Matrix": 
        national_squad_matrix_page(players)
    elif page == "National Best XI":
        national_best_xi_page(players)
    else:
        # Fallback to the main page if something goes wrong
        main_page(None, df, players)

if __name__ == "__main__":
    main()