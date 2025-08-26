# transfer_loan_management.py

import streamlit as st
import pandas as pd

from sqlite_db import get_user_club, get_second_team_club, get_favorite_tactics, update_player_transfer_status, update_player_loan_status, update_player_club
from constants import get_valid_roles, get_tactic_roles
from data_parser import get_players_by_role
from squad_logic import calculate_squad_and_surplus, calculate_development_squads, get_master_role_ratings
from config_handler import get_age_threshold

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

        # --- REFACTORED: Use the new centralized function ---
        master_ratings = get_master_role_ratings(user_club, second_team_club)

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