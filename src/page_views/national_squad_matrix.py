# page_views/national_squad_matrix.py

import streamlit as st
import pandas as pd
from io import StringIO
import math

from sqlite_db import get_national_team_settings, get_national_squad_ids, get_national_favorite_tactics
from constants import get_valid_roles, get_tactic_roles
from data_parser import get_player_role_matrix
from utils import get_last_name, get_natural_role_sorter, color_dwrs_by_value, format_role_display
from ui_components import display_custom_header

def national_squad_matrix_page(players):
    """
    Displays a DWRS matrix focused on the national team squad and the eligible player pool.
    """
    # --- 1. SETUP & DATA LOADING ---
    nat_name, nat_code, nat_age = get_national_team_settings()
    display_custom_header(f"{nat_name or 'National'} Squad Matrix")

    if not all([nat_name, nat_code, nat_age]):
        st.warning("Please configure your national team details in Settings to use this page.")
        return

    # --- 2. DISPLAY OPTIONS ---
    st.subheader("Display Options")
    col1, col2, col3 = st.columns(3)
    with col1:
        fav_tactic1, _ = get_national_favorite_tactics()
        all_tactics = ["All Roles"] + sorted(list(get_tactic_roles().keys()))
        tactic_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
        selected_tactic = st.selectbox("Select Tactic to Filter Roles", options=all_tactics, index=tactic_index)
    with col2:
        show_extra_details = st.checkbox("Show Extra Details (Foot, Height)")
    with col3:
        hide_retired = st.checkbox("Hide 'Retired' Players", value=True)

    # --- 3. DATA PREPARATION ---
    # Determine which role columns to show based on tactic selection
    role_sorter = get_natural_role_sorter()
    base_roles = get_valid_roles() if selected_tactic == "All Roles" else list(set(get_tactic_roles()[selected_tactic].values()))
    selected_roles = sorted(base_roles, key=lambda r: role_sorter.get(r, (99, 99)))
    
    # Get the complete player matrix data
    full_matrix = get_player_role_matrix()
    if full_matrix.empty:
        st.info("No player data available to generate matrix.")
        return

    if hide_retired:
        full_matrix = full_matrix[full_matrix['Club'].str.lower() != 'retired']

    # Filter the full matrix to find all players eligible for the national team
    nat_age = int(nat_age)
    full_matrix['AgeNum'] = pd.to_numeric(full_matrix['Age'], errors='coerce')
    eligible_df = full_matrix[
        (full_matrix['Nationality'] == nat_code) | (full_matrix['Second Nationality'] == nat_code)
    ]
    if nat_age < 99:
        eligible_df = eligible_df[eligible_df['AgeNum'] <= nat_age]

    # Split the eligible players into two groups: those in the squad and those who are not
    squad_player_ids = get_national_squad_ids()
    squad_df = eligible_df[eligible_df['Unique ID'].isin(squad_player_ids)].copy()
    available_pool_df = eligible_df[~eligible_df['Unique ID'].isin(squad_player_ids)].copy()

    # --- 4. DISPLAY LOGIC ---
    # Define base columns (no financial data)
    base_cols = ["Name", "Age", "Position", "Club"]
    if show_extra_details:
        base_cols.extend(["Left Foot", "Right Foot", "Height"])
    
    display_cols = base_cols + selected_roles

    # --- Display the Current National Squad Table ---
    st.subheader(f"Current National Squad ({len(squad_df)} Players)")
    if squad_df.empty:
        st.info("No players have been selected for the squad yet. Go to 'National Squad Selection' to add players.")
    else:
        # Sort by name for consistent display
        squad_df['LastName'] = squad_df['Name'].apply(get_last_name)
        squad_df = squad_df.sort_values(by=['LastName', 'Name']).drop(columns=['LastName'])
        
        # Style and display the dataframe
        styler = squad_df[display_cols].style.format("{:.0f}", subset=selected_roles, na_rep="-")
        styler = styler.apply(lambda col: col.map(color_dwrs_by_value), subset=selected_roles)
        st.dataframe(styler, use_container_width=True, hide_index=True)

    st.divider()

    # --- Display the Available Player Pool with Advanced Filtering ---
    st.subheader(f"Eligible Player Pool ({len(available_pool_df)} Players)")
    if available_pool_df.empty:
        st.info("No other eligible players found in the database.")
    else:
        with st.expander("🔍 Advanced Filtering & Sorting", expanded=True):
            sort_c1, sort_c2 = st.columns([2, 1])
            with sort_c1:
                sort_options = ["Name"] + selected_roles
                sort_by = st.selectbox(
                    "Sort by", options=sort_options,
                    format_func=lambda x: "Name" if x == "Name" else format_role_display(x)
                )
            with sort_c2:
                sort_direction = st.radio("Direction", ["Descending", "Ascending"], horizontal=True, index=0)

            dwrs_c1, age_c2 = st.columns(2)
            with dwrs_c1:
                min_dwrs, max_dwrs = st.slider("Filter by DWRS Rating (for selected sort role)", 0, 100, (20, 100))
            with age_c2:
                max_age_filter = st.slider("Filter by Max Age", 15, nat_age, nat_age)

        # Apply filtering and sorting to the available pool
        filtered_df = available_pool_df[available_pool_df['AgeNum'] <= max_age_filter].copy()
        
        if sort_by != "Name":
            filtered_df = filtered_df[
                (filtered_df[sort_by] >= min_dwrs) &
                (filtered_df[sort_by] <= max_dwrs)
            ]

        # Apply sorting
        is_ascending = (sort_direction == "Ascending")
        if sort_by == "Name":
            filtered_df['LastName'] = filtered_df['Name'].apply(get_last_name)
            sorted_df = filtered_df.sort_values(by='LastName', ascending=is_ascending).drop(columns=['LastName'])
        else:
            sorted_df = filtered_df.sort_values(by=sort_by, ascending=is_ascending, na_position='last')
        
        st.caption(f"Displaying {len(sorted_df)} matching players from the pool.")
        
        # Style and display the filtered & sorted dataframe
        styler_pool = sorted_df[display_cols].style.format("{:.0f}", subset=selected_roles, na_rep="-")
        styler_pool = styler_pool.apply(lambda col: col.map(color_dwrs_by_value), subset=selected_roles)
        st.dataframe(styler_pool, use_container_width=True, hide_index=True)