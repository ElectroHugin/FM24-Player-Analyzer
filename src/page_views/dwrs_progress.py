# dwrs_progress.py

import streamlit as st
import pandas as pd

from sqlite_db import get_user_club, get_dwrs_history, get_favorite_tactics
from constants import get_valid_roles, get_tactic_roles
from utils import format_role_display, get_last_name
from ui_components import display_custom_header

def dwrs_progress_page(players):
    #st.title("DWRS Player Development")
    display_custom_header("DWRS Player Development")
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