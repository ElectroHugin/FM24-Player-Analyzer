# edit_player.py

import streamlit as st
from sqlite_db import get_user_club, update_player_club, update_player_apt, update_player_natural_positions, set_primary_role
from utils import get_last_name, format_role_display, parse_position_string
from constants import GK_APT_OPTIONS, FIELD_PLAYER_APT_OPTIONS
from ui_components import clear_all_caches

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