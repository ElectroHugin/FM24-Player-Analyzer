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
from page_views.player_profile import player_profile_page
from page_views.tactic_explorer import tactic_explorer_page
from page_views.gap_analysis import gap_analysis_page
from page_views.assign_roles import assign_roles_page
from page_views.national_squad_selection import national_squad_selection_page
from page_views.national_squad_matrix import national_squad_matrix_page
from page_views.national_best_xi import national_best_xi_page
from page_views.national_dashboard import national_dashboard_page
#from page_views.shortlist import shortlist_page

from data_parser import load_data, parse_and_update_data
from sqlite_db import (get_second_team_club, set_second_team_club, get_user_club, set_user_club, get_all_players, update_dwrs_ratings,
                        get_favorite_tactics, get_national_mode_enabled, update_player_club)
from constants import get_valid_roles, get_tactic_roles
from config_handler import save_theme_settings, get_theme_settings
from ui_components import clear_all_caches, display_strength_grid, display_custom_header, display_player_table
from data_parser import get_player_role_matrix
from definitions_handler import PROJECT_ROOT
from squad_logic import get_cached_squad_analysis
from utils import  hex_to_rgb, format_role_display, value_to_float
from theme_handler import set_theme_toml
from role_logic import auto_assign_roles_to_unassigned

st.set_page_config(page_title="FM 2024 Player Dashboard", layout="wide")

def _render_player_search(players):
    """Compact global player search for the sidebar (Club mode only).

    Matches on player NAME only (case-insensitive substring) and shows up to
    8 results as buttons labelled 'Name - Club . Position'. Clicking a result
    stores the target UID in session_state and flags a one-shot jump to the
    Player Profile page; the actual page switch is performed by option_menu's
    manual_select in sidebar().
    """
    st.text_input(
        "Player search",
        key="player_search_query",
        placeholder="🔎 Search player by name…",
        label_visibility="collapsed",
    )

    if not players:
        return

    query = (st.session_state.get("player_search_query") or "").strip().lower()
    if len(query) < 2:
        return  # require 2+ chars to avoid flooding the sidebar

    results = [p for p in players if query in (p.get("Name") or "").lower()]

    def _rank(p):
        name = (p.get("Name") or "").lower()
        if name.startswith(query):
            tier = 0
        elif any(word.startswith(query) for word in name.split()):
            tier = 1
        else:
            tier = 2
        return (tier, name)

    results.sort(key=_rank)

    if not results:
        st.caption("No players found.")
        return

    MAX_RESULTS = 8
    shown = results[:MAX_RESULTS]

    for i, p in enumerate(shown):
        uid = p.get("Unique ID")
        name = p.get("Name", "?")
        club = p.get("Club") or "—"
        pos = p.get("Position") or "—"
        st.markdown(f"**{name}** · {club} · {pos}")
        b_prof, b_edit = st.columns(2)
        if b_prof.button("👤 Profile", key=f"psearch_prof_{i}_{uid}", use_container_width=True):
            st.session_state["profile_target_uid"] = uid
            st.session_state["nav_to_profile"] = True
            st.rerun()
        if b_edit.button("✏️ Edit", key=f"psearch_edit_{i}_{uid}", use_container_width=True):
            st.session_state["edit_target_uid"] = uid
            st.session_state["nav_to_edit"] = True
            st.rerun()

    if len(results) > MAX_RESULTS:
        st.caption(f"+{len(results) - MAX_RESULTS} more — refine your search.")


def sidebar(df, players):
    with st.sidebar:
        # --- Get theme colors first, as they are used everywhere ---
        theme_settings = get_theme_settings()
        current_mode = theme_settings.get('current_mode', 'night')
        primary_color = theme_settings.get(f"{current_mode}_primary_color")
        secondary_color = theme_settings.get(f"{current_mode}_text_color")
        secondary_bg_color = theme_settings.get(f"{current_mode}_secondary_background_color")
        rgb = hex_to_rgb(primary_color)
        hover_color = f"rgba({rgb[0]}, {rgb[1]}, {rgb[2]}, 0.15)"

        # --- Initialize session state for the management mode ---
        is_national_mode_enabled = get_national_mode_enabled()
        if 'management_mode' not in st.session_state:
            st.session_state.management_mode = "Club"

        # --- Dynamic Logo Display (Now at the top) ---
        col1, col2, col3 = st.columns([1, 2, 1])
        with col2:
            if st.session_state.management_mode == "National" and is_national_mode_enabled:
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
        
        # --- DYNAMIC NAVIGATION MENU ---
        if st.session_state.management_mode == "National" and is_national_mode_enabled:
            # Define the core pages for National mode
            page_options = ["National Dashboard", "National Squad", "Squad Matrix", "Best XI"]
            page_icons = ["house", "people-fill", "table", "trophy"]
            page_title = "National Team"
            page_mapping = { 
                "National Dashboard": "National Dashboard",
                "National Squad": "National Squad Selection", 
                "Squad Matrix": "National Squad Matrix",
                "Best XI": "National Best XI",
            }
            # --- THIS IS THE CHANGE: Use insert() to add "Assign Roles" in the second position ---
            page_options.insert(1, "Assign Roles")
            page_icons.insert(1, "person-plus")
            page_mapping["Assign Roles"] = "Assign Roles" # The mapping is the same as the club version
            # --- END OF CHANGE ---
            
        else: # Club Management
            # This section remains unchanged
            page_options = ["Dashboard", "Assign Roles", "Role Analysis", "Profile", "Squad Matrix", "Best XI", "Gap Analysis", "Tactic Explorer", "Transfers", "Comparison", "Development", "Edit Player"]
            page_icons = ["house", "person-plus", "search", "person-badge", "table", "trophy", "binoculars", "compass", "arrow-left-right", "people", "graph-up", "pencil-square"]
            page_title = "Club Navigation"
            page_mapping = { "Dashboard": "All Players", "Assign Roles": "Assign Roles", "Role Analysis": "Role Analysis", "Profile": "Player Profile", "Squad Matrix": "Player-Role Matrix", "Best XI": "Best Position Calculator", "Gap Analysis": "Gap Analysis", "Tactic Explorer": "Tactic Explorer", "Transfers": "Transfer & Loan Management", "Comparison": "Player Comparison", "Development": "DWRS Progress", "Edit Player": "Edit Player Data"}
            
        # This section for global pages also remains unchanged
        page_options.extend(["New Role", "New Tactic", "Settings"])
        page_icons.extend(["person-badge", "clipboard-plus", "gear"])
        page_mapping["New Role"] = "Create New Role"
        page_mapping["New Tactic"] = "Create New Tactic"
        page_mapping["Settings"] = "Settings"
        # --- END OF CHANGE ---

        # One-shot programmatic navigation target (set by the global player
        # search). When a result is clicked we flag a jump to the Profile page;
        # the menu then selects "Profile" on this run and we immediately reset
        # the flag so normal navigation works again afterwards.
        manual_idx = None
        if st.session_state.get("nav_to_profile"):
            if "Profile" in page_options:
                manual_idx = page_options.index("Profile")
            st.session_state["nav_to_profile"] = False
        elif st.session_state.get("nav_to_edit"):
            if "Edit Player" in page_options:
                manual_idx = page_options.index("Edit Player")
            st.session_state["nav_to_edit"] = False

        page = option_menu(
            menu_title=page_title,
            options=page_options,
            icons=page_icons,
            menu_icon="list-ul",
            default_index=0,
            manual_select=manual_idx,
            key=f"nav_menu_{st.session_state.management_mode}",
            styles={
                "container": {"padding": "5px !important", "background-color": "transparent"},
                "icon": {"color": secondary_color, "font-size": "20px"},
                "nav-link": {"font-size": "16px", "text-align": "left", "margin":"0px", "--hover-color": hover_color},
                "nav-link-selected": {"background-color": primary_color},
            }
        )
        actual_page = page_mapping.get(page)

        # --- Club Selectors (only show in club mode) ---
        if st.session_state.management_mode == "Club":
            _render_player_search(players)
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

        # --- NEW: MODE SWITCHER AND THEME TOGGLE AT THE BOTTOM ---
        st.divider()

        # Only show the mode switcher if the feature is enabled in settings
        if is_national_mode_enabled:
            # Inject CSS to style the st.radio to look like a segmented control
            st.markdown(f"""
                <style>
                    /* Main container for the radio buttons */
                    div[data-testid="stRadio"] > div {{
                        border: 1px solid {primary_color};
                        border-radius: 5px;
                        padding: 2px;
                        display: flex;
                        justify-content: space-around;
                    }}
                    /* Hide the default radio button circle */
                    div[data-testid="stRadio"] input[type="radio"] {{
                        display: none;
                    }}
                    /* Style for each option's label */
                    div[data-testid="stRadio"] label {{
                        border-radius: 3px;
                        padding: 4px 8px;
                        color: {secondary_color};
                        user-select: none;
                        cursor: pointer;
                        flex-grow: 1;
                        text-align: center;
                        transition: all 0.2s ease-in-out;
                    }}
                    /* Style for the SELECTED option's label */
                    div[data-testid="stRadio"] div[role="radiogroup"] > div:has(input:checked) > label {{
                        background-color: {primary_color};
                        color: white; /* Text color on the active button */
                        font-weight: 600;
                    }}
                    /* Hover effect for UNSELECTED options */
                    div[data-testid="stRadio"] div[role="radiogroup"] > div:not(:has(input:checked)):hover > label {{
                        background-color: {hover_color};
                    }}
                </style>
            """, unsafe_allow_html=True)

            # Determine the index for the radio button based on session state
            current_mode_index = 1 if st.session_state.management_mode == "National" else 0
            
            selected_mode = st.radio(
                "Management Mode", 
                options=["Club", "National"], 
                index=current_mode_index,
                horizontal=True,
            )
            
            # If the user clicks a different mode, update the state and rerun
            if selected_mode != st.session_state.management_mode:
                st.session_state.management_mode = selected_mode
                st.rerun()

        # The theme toggle remains at the very bottom
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

    # --- 1. DEDICATED UPLOAD SECTION (HEAVILY MODIFIED) ---
    with st.expander("⬆️ Upload New Player Data"):
        with st.form("upload_form", clear_on_submit=True):
            uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
            
            # --- THIS IS YOUR NEW CHECKBOX ---
            is_squad_update = st.checkbox(
                "This file is a full squad update for my club(s)", 
                value=False,
                help="Check this if the file contains ONLY players from your main and second team. The app will identify players who have left and ask you to confirm their departure."
            )
            
            auto_assign = st.checkbox("Automatically assign roles to new/unassigned players", value=True)
            submitted = st.form_submit_button("Process File")

            if submitted and uploaded_file is not None:
                # --- THIS IS THE NEW TWO-STEP LOGIC ---
                with st.spinner("Processing file..."):
                    full_df, affected_ids = parse_and_update_data(uploaded_file)
                
                if full_df is None:
                    # parse_and_update_data already shows a detailed error message,
                    # so we just stop here without adding a second, redundant one.
                    pass
                else:
                    # After processing, run other optional steps
                    if auto_assign:
                        with st.spinner("Auto-assigning roles..."):
                            num_assigned = auto_assign_roles_to_unassigned()
                        st.toast(f"Assigned roles to {num_assigned} players.", icon="✨")

                    with st.spinner("Calculating DWRS for updated players..."):
                        clear_all_caches()
                        final_df = load_data()
                        if final_df is not None:
                            update_dwrs_ratings(final_df, get_valid_roles(), affected_ids)
                    
                    # --- NEW DEPARTURE DETECTION LOGIC ---
                    if is_squad_update and user_club:
                        st.session_state.missing_players_to_resolve = []
                        second_club = get_second_team_club()
                        
                        # Get all players currently listed at the user's clubs in the DB
                        db_players = get_all_players()
                        club_players_db = {
                            p['Unique ID']: p for p in db_players 
                            if p.get('Club') == user_club or (second_club and p.get('Club') == second_club)
                        }
                        
                        # Find which of them were NOT in the uploaded file
                        uploaded_ids = set(affected_ids)
                        missing_ids = set(club_players_db.keys()) - uploaded_ids

                        if missing_ids:
                            st.session_state.missing_players_to_resolve = [
                                club_players_db[uid] for uid in missing_ids
                            ]
                            st.toast(f"Detected {len(missing_ids)} players who may have left the club. Please resolve below.", icon="👋")
                        else:
                            st.success("Squad update complete! All club players accounted for.")
                            st.rerun()
                    else:
                        st.success("Data updated and ratings calculated successfully!")
                        st.rerun()

    # --- 2. NEW UI FOR RESOLVING DEPARTURES ---
    # This entire section will only appear when there are players to resolve.
    if 'missing_players_to_resolve' in st.session_state and st.session_state.missing_players_to_resolve:
        st.warning("Action Required: Player Departures")
        st.info("The following players are in the database at your club(s) but were not in the squad file you just uploaded. Please confirm their status.")

        with st.form("resolve_departures_form"):
            for player in st.session_state.missing_players_to_resolve:
                uid = player['Unique ID']
                st.markdown(f"**{player['Name']}** ({player['Club']})")
                
                # Use columns for a neat layout
                c1, c2 = st.columns([1, 2])
                
                # The radio button provides the "Keep" or "Left" choice
                action = c1.radio(
                    "Action", 
                    options=["Keep at Club", "Player has left"], 
                    key=f"action_{uid}", 
                    horizontal=True, 
                    label_visibility="collapsed"
                )
                
                # The text input for the new club
                c2.text_input("New Club (if left)", key=f"new_club_{uid}", placeholder="Enter new club or 'Retired'")
            
            save_departures = st.form_submit_button("Confirm All Departures", type="primary")

            if save_departures:
                with st.spinner("Updating player records..."):
                    players_updated = 0
                    for player in st.session_state.missing_players_to_resolve:
                        uid = player['Unique ID']
                        action = st.session_state[f"action_{uid}"]
                        
                        # If user selected that the player left...
                        if action == "Player has left":
                            new_club = st.session_state[f"new_club_{uid}"].strip()
                            # ...and they provided a new club name...
                            if new_club:
                                # ...update the player's club in the database.
                                update_player_club(uid, new_club)
                                players_updated += 1
                
                st.success(f"Successfully updated {players_updated} player record(s).")
                # Clean up the session state to hide this UI and finish the process
                del st.session_state.missing_players_to_resolve
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
        display_player_table(my_club_df)
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
        display_player_table(my_club_df)

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


def load_app_data():
    """
    Single, centralized entry point for loading all player data.

    Both load_data() and get_all_players() are already @st.cache_data-cached,
    so calling this repeatedly is cheap; the point of routing every page
    through one helper is consistency and a single place to refresh after an
    upload. Returns a (df, players) tuple.
    """
    df = load_data()
    players = get_all_players()
    return df, players


def main():
    # --- Centralized data loading: load ONCE here, pass to pages ---
    df, players = load_app_data()
    page = sidebar(df, players)

    query_params = st.query_params
    if "page" in query_params:
        page = query_params["page"][0]
    
    # --- ROUTER ---
    # Pages receive pre-loaded data (df / players) from main() where they use it.
    # role_analysis_page and player_role_matrix_page intentionally take no args:
    # they pull their data through dedicated cached helpers (get_players_by_role,
    # get_player_role_matrix) rather than the shared players list.
    if page == "All Players":
        main_page(None, df, players)
    elif page == "Assign Roles":
        assign_roles_page(df)
    elif page == "Role Analysis":
        role_analysis_page()
    elif page == "Player Profile":
        player_profile_page(players)
    elif page == "Player-Role Matrix":
        player_role_matrix_page()
    elif page == "Best Position Calculator":
        best_position_calculator_page(players)
    elif page == "Gap Analysis":
        gap_analysis_page(players)
    elif page == "Transfer & Loan Management":
        transfer_loan_management_page(players)
    #elif page == "Shortlist": 
    #    shortlist_page(players)
    elif page == "Player Comparison":
        player_comparison_page(players)
    elif page == "DWRS Progress":
        dwrs_progress_page(players)
    elif page == "Edit Player Data":
        edit_player_data_page(players)
    elif page == "Tactic Explorer":
        tactic_explorer_page()
    elif page == "Create New Role":
        create_new_role_page()
    elif page == "Create New Tactic":
        create_new_tactic_page()
    elif page == "Settings":
        settings_page()
    # --- National Page Routing ---
    elif page == "National Dashboard": 
        national_dashboard_page(df, players)
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