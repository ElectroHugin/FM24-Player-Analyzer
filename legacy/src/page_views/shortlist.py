# page_views/shortlist.py

import streamlit as st
import pandas as pd
from ui_components import display_custom_header
from sqlite_db import get_shortlist_ids, set_shortlist_ids

def shortlist_page(players):
    """
    A dedicated page for viewing all players and managing a persistent shortlist.
    """
    display_custom_header("Player Shortlist")
    
    df = pd.DataFrame(players)
    if df.empty:
        st.info("No player data has been loaded into the application.")
        return

    current_shortlist_ids = get_shortlist_ids()

    if 'shortlist_selection' not in st.session_state:
        st.session_state.shortlist_selection = set(current_shortlist_ids)
    
    col1, col2 = st.columns(2)

    # --- COLUMN 1: ALL PLAYERS POOL ---
    with col1:
        st.subheader("All Players")
        search_term = st.text_input("Search all players by name...", key="search_all_players")

        available_players_df = df[~df['Unique ID'].isin(st.session_state.shortlist_selection)]
        if search_term:
            available_players_df = available_players_df[available_players_df['Name'].str.contains(search_term, case=False, na=False)]

        with st.container(height=600):
            if available_players_df.empty:
                st.info("No available players match your search, or all players have been shortlisted.")
            else:
                for _, player in available_players_df.sort_values(by="Name").iterrows():
                    row = st.columns([0.8, 0.2])
                    with row[0]:
                        # --- THIS IS THE FIX ---
                        # Try to convert age to int, but use a fallback if it fails
                        try:
                            age_display = int(player['Age'])
                        except (ValueError, TypeError):
                            age_display = "N/A" # Or you could use 0, or "?"
                        
                        st.markdown(f"**{player['Name']}** ({age_display})")
                        # --- END OF FIX ---
                        st.caption(f"{player['Club']} | {player['Position']}")
                    with row[1]:
                        if st.button("Add", key=f"add_shortlist_{player['Unique ID']}", use_container_width=True):
                            st.session_state.shortlist_selection.add(player['Unique ID'])
                            st.rerun()
                    st.divider()

    # --- COLUMN 2: CURRENT SHORTLIST ---
    with col2:
        st.subheader(f"Shortlisted Players ({len(st.session_state.shortlist_selection)})")
        
        shortlist_df = df[df['Unique ID'].isin(st.session_state.shortlist_selection)]

        with st.container(height=600):
            if shortlist_df.empty:
                st.info("No players have been shortlisted yet.")
            else:
                for _, player in shortlist_df.sort_values(by="Name").iterrows():
                    row = st.columns([0.8, 0.2])
                    with row[0]:
                        # --- APPLY THE SAME FIX HERE ---
                        try:
                            age_display = int(player['Age'])
                        except (ValueError, TypeError):
                            age_display = "N/A"

                        st.markdown(f"**{player['Name']}** ({age_display})")
                        # --- END OF FIX ---
                        st.caption(f"{player['Club']} | {player['Position']}")
                    with row[1]:
                        if st.button("Remove", key=f"remove_shortlist_{player['Unique ID']}", use_container_width=True):
                            st.session_state.shortlist_selection.remove(player['Unique ID'])
                            st.rerun()
                    st.divider()

    st.divider()

    # --- FINAL SAVE ACTION ---
    if st.button("Save Shortlist", type="primary", use_container_width=True):
        with st.spinner("Saving shortlist..."):
            set_shortlist_ids(st.session_state.shortlist_selection)
        st.success(f"Successfully saved {len(st.session_state.shortlist_selection)} players to the shortlist!")
        if 'shortlist_selection' in st.session_state:
            del st.session_state.shortlist_selection