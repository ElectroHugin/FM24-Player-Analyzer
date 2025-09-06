# page_views/national_squad_selection.py

import streamlit as st
import pandas as pd
from ui_components import display_custom_header
from sqlite_db import (get_national_team_settings, get_national_squad_ids, 
                       set_national_squad_ids)
from utils import get_last_name

def national_squad_selection_page(players):
    """
    A dedicated page for viewing eligible players and managing the national squad.
    """
    # --- 1. INITIAL SETUP AND DATA LOADING ---
    nat_name, nat_code, nat_age = get_national_team_settings()
    
    display_custom_header(f"{nat_name or 'National Squad'} Selection")

    # Check if the national team has been configured in settings
    if not all([nat_name, nat_code, nat_age]):
        st.warning("Please configure your national team details fully in the Settings page to use this feature.")
        return

    df = pd.DataFrame(players)
    if df.empty:
        st.info("No player data has been loaded into the application.")
        return

    # --- 2. CORE FILTERING LOGIC ---
    # Prepare dataframes by converting age to a numeric type for filtering
    nat_age = int(nat_age)
    df['Age'] = pd.to_numeric(df['Age'], errors='coerce')

    # Filter for players who have the correct primary or secondary nationality
    eligible_df = df[
        (df['Nationality'] == nat_code) | (df['Second Nationality'] == nat_code)
    ].copy()
    
    # If it's a youth team (age limit is not 99), also filter by age
    if nat_age < 99:
        eligible_df = eligible_df[eligible_df['Age'] <= nat_age]

    if eligible_df.empty:
        st.error(f"No players found with nationality '{nat_code}' matching the age criteria (<= {nat_age}).")
        return

    # Load the list of IDs for players who are already in the squad
    current_squad_ids = get_national_squad_ids()

    # --- 3. DYNAMIC UI WITH SESSION STATE FOR INTERACTIVITY ---
    # Initialize the session state to hold the user's selections during their session
    if 'national_squad_selection' not in st.session_state:
        st.session_state.national_squad_selection = set(current_squad_ids)
    
    # Define the two main columns for the layout
    col1, col2 = st.columns(2)

    # --- COLUMN 1: AVAILABLE PLAYER POOL ---
    with col1:
        st.subheader("Available Player Pool")
        search_term = st.text_input("Search available players by name...", key="search_available")

        # Further filter the pool to show players who are NOT yet selected
        available_players_df = eligible_df[~eligible_df['Unique ID'].isin(st.session_state.national_squad_selection)]
        if search_term:
            available_players_df = available_players_df[available_players_df['Name'].str.contains(search_term, case=False, na=False)]

        # Display players in a scrollable container
        with st.container(height=500):
            if available_players_df.empty:
                st.info("No available players match your search.")
            else:
                for _, player in available_players_df.sort_values(by="Name").iterrows():
                    row = st.columns([0.8, 0.2])
                    with row[0]:
                        st.markdown(f"**{player['Name']}** ({int(player['Age'])})")
                        st.caption(f"{player['Club']} | {player['Position']}")
                    with row[1]:
                        if st.button("Add", key=f"add_{player['Unique ID']}", use_container_width=True):
                            st.session_state.national_squad_selection.add(player['Unique ID'])
                            st.rerun() # Rerun to update the lists instantly
                    st.divider()

    # --- COLUMN 2: CURRENTLY SELECTED SQUAD ---
    with col2:
        st.subheader(f"Current Squad ({len(st.session_state.national_squad_selection)} Players)")
        
        # Filter the main eligible list to get the full data for players in the current squad
        squad_df = eligible_df[eligible_df['Unique ID'].isin(st.session_state.national_squad_selection)]

        with st.container(height=500):
            if squad_df.empty:
                st.info("No players have been added to the squad yet.")
            else:
                for _, player in squad_df.sort_values(by="Name").iterrows():
                    row = st.columns([0.8, 0.2])
                    with row[0]:
                        st.markdown(f"**{player['Name']}** ({int(player['Age'])})")
                        st.caption(f"{player['Club']} | {player['Position']}")
                    with row[1]:
                        if st.button("Remove", key=f"remove_{player['Unique ID']}", use_container_width=True):
                            st.session_state.national_squad_selection.remove(player['Unique ID'])
                            st.rerun() # Rerun to update the lists instantly
                    st.divider()

    st.divider()

    # --- 4. FINAL SAVE ACTION ---
    if st.button("Save National Squad to Database", type="primary", use_container_width=True):
        with st.spinner("Saving squad..."):
            # Use the dedicated database function to save the final set of IDs
            set_national_squad_ids(st.session_state.national_squad_selection)
        st.success(f"Successfully saved {len(st.session_state.national_squad_selection)} players to the national squad!")
        # It's good practice to clear the temporary session state after saving
        del st.session_state.national_squad_selection