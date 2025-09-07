# page_views/national_dashboard.py

import streamlit as st
import pandas as pd

from ui_components import display_custom_header, display_strength_grid, clear_all_caches
from sqlite_db import (get_national_team_settings, get_national_squad_ids, get_national_favorite_tactics, 
                       update_dwrs_ratings, set_national_squad_ids)
from data_parser import parse_and_update_data, get_player_role_matrix, load_data
from role_logic import auto_assign_roles_to_unassigned
from squad_logic import calculate_squad_and_surplus, get_master_role_ratings
from constants import get_tactic_roles, get_valid_roles
from utils import format_role_display
from config_handler import save_theme_settings, get_theme_settings

def national_dashboard_page(df, players):
    # --- 1. SETUP & DATA LOADING ---
    nat_name, nat_code, nat_age = get_national_team_settings()
    display_custom_header("National Dashboard")

    if not all([nat_name, nat_code, nat_age]):
        st.warning("Please configure your national team details in Settings to use this feature.")
        return

    squad_ids = get_national_squad_ids()
    squad_players = [p for p in players if p['Unique ID'] in squad_ids]
    squad_df = pd.DataFrame(squad_players)

    # --- 2. DEDICATED UPLOAD SECTION (MODIFIED) ---
    with st.expander("⬆️ Upload New Player Data"):
        with st.form("upload_form_nat", clear_on_submit=True):
            uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
            
            # --- THIS IS YOUR NEW CHECKBOX ---
            replace_squad = st.checkbox(
                "Replace national squad with players from this file", 
                value=False, # Default to unticked, as requested
                help="If checked, the current national squad will be cleared and replaced with every player found in this HTML file. Use this for quick squad updates."
            )
            # --- END OF NEW WIDGET ---
            
            auto_assign = st.checkbox("Automatically assign roles to new/unassigned players", value=True)
            submitted = st.form_submit_button("Process File")

            if submitted and uploaded_file is not None:
                # --- THIS IS THE MODIFIED LOGIC ---
                with st.spinner("Processing file..."):
                    full_df, affected_ids = parse_and_update_data(uploaded_file)
                
                if full_df is None:
                    st.error("Invalid HTML file: Must contain a table with a 'UID' column.")
                else:
                    # Step 1 (Optional): Replace the national squad if the box was checked
                    if replace_squad:
                        with st.spinner("Replacing national squad..."):
                            set_national_squad_ids(affected_ids)
                        st.toast(f"National squad replaced with {len(affected_ids)} players from the file.", icon="🔁")

                    # Step 2 (Optional): Auto-assign roles
                    if auto_assign:
                        with st.spinner("Auto-assigning roles..."):
                            num_assigned = auto_assign_roles_to_unassigned()
                        st.toast(f"Assigned roles to {num_assigned} players.", icon="✨")

                    # Step 3: Calculate DWRS for the newly updated players
                    with st.spinner(f"Calculating DWRS for {len(affected_ids)} updated players..."):
                        clear_all_caches()
                        final_df = load_data()
                        if final_df is not None:
                            update_dwrs_ratings(final_df, get_valid_roles(), affected_ids)

                    st.success("Data updated and ratings calculated successfully!")
                    st.rerun()
                # --- END OF MODIFIED LOGIC ---

    if squad_df.empty:
        st.info("No players have been selected for the national squad yet. Go to 'National Squad Selection' to begin.")
        return

    # --- 3. TACTIC SELECTION FOR ANALYSIS ---
    st.subheader("Squad Analysis")
    fav_tactic1, _ = get_national_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    tactic_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
    selected_tactic = st.selectbox("Analyze Squad based on Tactic:", options=all_tactics, index=tactic_index)

    # --- 4. SQUAD CALCULATION ---
    analysis_results = None
    with st.spinner(f"Analyzing your squad's fit for the '{selected_tactic}' tactic..."):
        master_ratings = get_master_role_ratings()
        analysis_results = calculate_squad_and_surplus(squad_players, get_tactic_roles()[selected_tactic], master_ratings)

    # --- 5. DISPLAY KPIS (Key Performance Indicators) ---
    st.markdown("---")
    squad_df['Age'] = pd.to_numeric(squad_df['Age'], errors='coerce')
    avg_age = squad_df['Age'].mean()

    col1, col2, col3, col4 = st.columns(4)
    col1.metric("Players in Squad", f"{len(squad_df)}")
    col2.metric("Average Age", f"{avg_age:.1f}")

    # --- 6. POSITIONAL STRENGTH & SQUAD TABLE ---
    st.markdown("---")
    strength_col, table_col = st.columns([2, 3])

    with strength_col:
        positional_strengths = {}
        tactic_positions = get_tactic_roles()[selected_tactic]
        xi_squad = analysis_results["starting_xi"]
        b_team_squad = analysis_results["b_team"]
        depth_options = analysis_results["best_depth_options"]

        for pos_key, role in tactic_positions.items():
            ratings = []
            def add_rating(player_obj):
                if player_obj and player_obj.get('name') != '-':
                    try: ratings.append(int(player_obj['rating'].rstrip('%')))
                    except (ValueError, TypeError): pass
            add_rating(xi_squad.get(pos_key))
            add_rating(b_team_squad.get(pos_key))
            # The player objects in depth_options are slightly different, need to access rating key
            for player in depth_options.get(role, []):
                if player and player.get('name') != '-':
                    try: ratings.append(int(player['rating'].rstrip('%')))
                    except (ValueError, TypeError): pass

            # --- THIS IS THE FIX ---
            # We now calculate and include 'min' and 'max'
            if ratings:
                positional_strengths[pos_key] = {
                    'avg': sum(ratings) / len(ratings), 
                    'min': min(ratings), 
                    'max': max(ratings)
                }
            else:
                positional_strengths[pos_key] = {'avg': 0, 'min': 0, 'max': 0}
            # --- END OF FIX ---
        
        # This also needs the current_mode for theme-awareness
        current_theme_mode = get_theme_settings().get('current_mode', 'night')
        display_strength_grid(positional_strengths, selected_tactic, mode=current_theme_mode)

    with table_col:
        st.subheader("Current National Squad")
        st.dataframe(squad_df[["Name", "Age", "Club", "Position"]], use_container_width=True, hide_index=True)

    # --- 7. POTENTIAL CALL-UPS (Player Suggestions) ---
    st.markdown("---")
    st.subheader("🎯 Potential Call-Ups")
    st.info("Discover potential upgrades from the eligible player pool who are not currently in your squad.")

    full_matrix = get_player_role_matrix()
    if full_matrix.empty:
        st.warning("No player matrix data available.")
        return

    # Filter for all players eligible for the nation
    full_matrix['AgeNum'] = pd.to_numeric(full_matrix['Age'], errors='coerce')
    eligible_pool = full_matrix[
        (full_matrix['Nationality'] == nat_code) | (full_matrix['Second Nationality'] == nat_code)
    ]
    if int(nat_age) < 99:
        eligible_pool = eligible_pool[eligible_pool['AgeNum'] <= int(nat_age)]
    
    # Split between players in the squad and those available for call-up
    squad_matrix = full_matrix[full_matrix['Unique ID'].isin(squad_ids)].copy()
    available_matrix = eligible_pool[~eligible_pool['Unique ID'].isin(squad_ids)].copy()

    # UI Filter (Age only)
    max_age = st.slider("Maximum Age for Suggestions", 15, int(nat_age), int(nat_age))
    filtered_available = available_matrix[available_matrix['AgeNum'] <= max_age]

    # Core Logic: Find Upgrades
    roles_tactic = sorted(list(set(get_tactic_roles()[selected_tactic].values())))
    suggestions = []

    for role in roles_tactic:
        if role not in squad_matrix.columns or role not in filtered_available.columns:
            continue

        squad_best_rating = squad_matrix[role].max()
        if pd.isna(squad_best_rating): squad_best_rating = 0 

        potential_upgrades = filtered_available[filtered_available[role] > squad_best_rating]
        
        if not potential_upgrades.empty:
            best_upgrade = potential_upgrades.loc[potential_upgrades[role].idxmax()]
            suggestions.append({
                "role": role,
                "player": best_upgrade['Name'],
                "rating": best_upgrade[role],
                "age": best_upgrade['Age'],
                "club": best_upgrade['Club'],
                "position": best_upgrade['Position'],
                "squad_best": squad_best_rating
            })

    # Display Suggestions
    if not suggestions:
        st.success("✅ Your squad is looking strong! No clear upgrades found in the eligible player pool.")
    else:
        num_cols = min(len(suggestions), 4)
        cols = st.columns(num_cols)
        for i, sug in enumerate(suggestions):
            with cols[i % num_cols]:
                with st.container(border=True):
                    st.markdown(f"**{sug['player']}**")
                    st.markdown(f"Upgrade for **{format_role_display(sug['role'])}**")
                    st.divider()
                    st.markdown(f"🎯 **Rating:** {sug['rating']:.0f}% (Your Best: {sug['squad_best']:.0f}%)")
                    st.markdown(f"🎂 **Age:** {sug['age']}")
                    st.caption(f"Club: {sug['club']}")
                    st.caption(f"Positions: {sug['position']}")