# role_analysis.py

import streamlit as st
import pandas as pd

from sqlite_db import get_user_club, get_second_team_club, get_all_players
from constants import get_valid_roles
from data_parser import get_players_by_role
from utils import format_role_display, color_dwrs_by_value, get_last_name
from ui_components import display_custom_header, display_pros_and_cons
from role_analysis_logic import analyze_player_for_role

@st.cache_data
def get_role_pool(role, scope, user_club, second_club):
    """
    Return the list of players who have `role` assigned, filtered by `scope`
    ('my_club' | 'second_team' | 'scouted'), sorted by last name.

    Cached per (role, scope, user_club, second_club) so re-runs are instant;
    the cache is cleared by clear_all_caches() whenever player data changes.
    """
    players = get_all_players()
    pool = [p for p in players if role in p.get('Assigned Roles', [])]

    if scope == 'my_club':
        pool = [p for p in pool if p.get('Club') == user_club]
    elif scope == 'second_team':
        pool = [p for p in pool if p.get('Club') == second_club]
    elif scope == 'scouted':
        exclude = {user_club, second_club} if second_club else {user_club}
        pool = [p for p in pool if p.get('Club') not in exclude]

    pool.sort(key=lambda p: get_last_name(p.get('Name', '')))
    return pool

def display_styled_role_df(df, title, use_full_style=False, top_n=200):
    st.subheader(title)
    if df.empty:
        st.info("No players found for this category.")
        return

    # The column to style is always 'DWRS Rating (Normalized)'
    column_to_style = 'DWRS Rating (Normalized)'

    # Start with a base styler to format the numbers correctly
    styler = df.style.format({
        "DWRS Rating (Absolute)": "{:.2f}",
    })

    if use_full_style:
        # For small club tables, style the entire column
        styler = styler.apply(
            lambda x: x.map(color_dwrs_by_value),
            subset=[column_to_style]
        )
    else:
        # For the large scouted list, only style the top N players
        # The dataframe is already sorted, so we just need the top N indices
        top_indices = df.head(top_n).index
        styler = styler.apply(
            lambda x: x.map(color_dwrs_by_value),
            subset=pd.IndexSlice[top_indices, [column_to_style]]
        )

    st.dataframe(styler, use_container_width=True, hide_index=True)

def role_analysis_page():
    display_custom_header("Role Analysis")
    user_club, second_club = get_user_club(), get_second_team_club()
    if not user_club:
        st.warning("Please select your club in the sidebar.")
        return
        
    role = st.selectbox("Select Role", options=get_valid_roles(), format_func=format_role_display)
    
    my_df, second_df, scout_df = get_players_by_role(role, user_club, second_club)

    # --- Organize the three player groups into tabs for a cleaner layout ---
    if second_club:
        my_tab, second_tab, scout_tab = st.tabs([
            f"🏠 {user_club}",
            f"🔄 {second_club}",
            "🔍 Scouted Players",
        ])
        with my_tab:
            display_styled_role_df(my_df, f"Players from {user_club}", use_full_style=True)
        with second_tab:
            display_styled_role_df(second_df, f"Players from {second_club}", use_full_style=True)
        with scout_tab:
            display_styled_role_df(scout_df, "Scouted Players", use_full_style=False, top_n=200)
    else:
        my_tab, scout_tab = st.tabs([
            f"🏠 {user_club}",
            "🔍 Scouted Players",
        ])
        with my_tab:
            display_styled_role_df(my_df, f"Players from {user_club}", use_full_style=True)
        with scout_tab:
            display_styled_role_df(scout_df, "Scouted Players", use_full_style=False, top_n=200)
    # --- Pros & Cons analysis for a selected player in this role ---
    st.divider()
    st.subheader(f"Strengths & Weaknesses as {format_role_display(role)}")

    # Let the user scope the player list, mirroring the tabs above, so the
    # dropdown isn't flooded with every player in the database.
    scope_labels = {"my_club": f"🏠 {user_club}"}
    if second_club:
        scope_labels["second_team"] = f"🔄 {second_club}"
    scope_labels["scouted"] = "🔍 Scouted Players"

    scope = st.radio(
        "Player pool",
        options=list(scope_labels.keys()),
        format_func=lambda s: scope_labels[s],
        horizontal=True,
        key="pros_cons_scope",
    )

    role_pool = get_role_pool(role, scope, user_club, second_club)

    if not role_pool:
        st.info(f"No players with this role assigned in '{scope_labels[scope]}'.")
        return

    player_map = {
        p['Unique ID']: f"{p.get('Name', 'Unknown')} ({p.get('Club', '-')})"
        for p in role_pool
    }

    selected_uid = st.selectbox(
        "Select a player to analyze",
        options=list(player_map.keys()),
        format_func=lambda uid: player_map[uid],
    )

    if selected_uid:
        selected_player = next((p for p in role_pool if p['Unique ID'] == selected_uid), None)
        if selected_player:
            analysis = analyze_player_for_role(selected_player, role)
            display_pros_and_cons(analysis)