# assign_roles.py

import streamlit as st
from constants import FILTER_OPTIONS, SORTABLE_COLUMNS, get_valid_roles, get_position_to_role_mapping
from data_parser import get_filtered_players
from sqlite_db import get_user_club, update_player_roles, update_dwrs_ratings
from utils import format_role_display, parse_position_string
from ui_components import clear_all_caches, display_custom_header

def assign_roles_page(df):
    #st.title("Assign Roles to Players")
    display_custom_header("Assign Roles to Players")
    # --- START: Initialize session state for filters ---
    if 'ar_filter_option' not in st.session_state:
        st.session_state.ar_filter_option = "All Players"
    if 'ar_club_filter' not in st.session_state:
        st.session_state.ar_club_filter = "All"
    if 'ar_pos_filter' not in st.session_state:
        st.session_state.ar_pos_filter = "All"
    if 'ar_sort_column' not in st.session_state:
        st.session_state.ar_sort_column = "Name"
    if 'ar_sort_order' not in st.session_state:
        st.session_state.ar_sort_order = "Ascending"
    if 'ar_search' not in st.session_state:
        st.session_state.ar_search = ""
    # --- END: Initialize session state ---
    if df.empty:
        st.info("No players available. Please upload player data.")
        return
    
    st.subheader("Filter & Sort")
    c1, c2, c3 = st.columns(3)

    # The 'key' argument automatically links the widget's state to st.session_state
    filter_option = c1.selectbox("Filter by", options=FILTER_OPTIONS, key='ar_filter_option')
    club_filter = c2.selectbox("Filter by Club", options=["All"] + sorted(df['Club'].unique()), key='ar_club_filter')
    pos_filter = c3.selectbox("Filter by Position", options=["All"] + sorted(df['Position'].unique()), key='ar_pos_filter')
    sort_column = c1.selectbox("Sort by", options=SORTABLE_COLUMNS, key='ar_sort_column')
    sort_order = c2.selectbox("Sort Order", options=["Ascending", "Descending"], key='ar_sort_order')
    search = st.text_input("Search by Name", key='ar_search')

    # Now, use the values from session_state to perform the filtering
    filtered_df = get_filtered_players(
        st.session_state.ar_filter_option, 
        st.session_state.ar_club_filter, 
        st.session_state.ar_pos_filter, 
        st.session_state.ar_sort_column, 
        (st.session_state.ar_sort_order == "Ascending"), 
        get_user_club()
    )

    if st.session_state.ar_search: 
        filtered_df = filtered_df[filtered_df['Name'].str.contains(st.session_state.ar_search, case=False, na=False)]
    
    def handle_role_update(role_changes):
        if role_changes:
            update_player_roles(role_changes)
            # Get the dataframe of affected players
            affected_df = df[df['Unique ID'].isin(role_changes.keys())].copy()
            
            # Manually update the 'Assigned Roles' in our copied dataframe to reflect the change
            # This ensures the DWRS calculation uses the *new* roles.
            def get_new_roles(row):
                return role_changes.get(row['Unique ID'], row['Assigned Roles'])
            affected_df['Assigned Roles'] = affected_df.apply(get_new_roles, axis=1)

            # Now, recalculate with the correct, updated data
            update_dwrs_ratings(affected_df, get_valid_roles())
            clear_all_caches()
            st.success(f"Successfully updated roles for {len(role_changes)} players!")
            st.rerun()
        else: st.info("No changes to apply.")

    st.subheader("Automatic Role Assignment")
    c1, c2 = st.columns(2)
    if c1.button("Auto-Assign to Unassigned Players"):
        unassigned = df[df['Assigned Roles'].apply(lambda x: not x)]
        changes = {p['Unique ID']: sorted(list(set(r for pos in parse_position_string(p['Position']) for r in get_position_to_role_mapping().get(pos, [])))) for _, p in unassigned.iterrows()}
        handle_role_update({k: v for k, v in changes.items() if v})
    if c2.button("⚠️ Auto-Assign to ALL Players"):
        changes = {p['Unique ID']: sorted(list(set(r for pos in parse_position_string(p['Position']) for r in get_position_to_role_mapping().get(pos, [])))) for _, p in df.iterrows()}
        handle_role_update({k: v for k, v in changes.items() if set(v) != set(df[df['Unique ID'] == k]['Assigned Roles'].iloc[0])})
    
    st.subheader("Assign/Edit Roles Individually")
    st.dataframe(filtered_df[['Name', 'Position', 'Club', 'Assigned Roles']], use_container_width=True, hide_index=True)
    changes = {}
    for _, row in filtered_df.iterrows():
        new = st.multiselect(f"Roles for {row['Name']} ({row['Position']})", options=get_valid_roles(), default=row['Assigned Roles'], key=f"roles_{row['Unique ID']}", format_func=format_role_display)
        #new = st.multiselect(f"Roles for {row['Name']}", options=get_valid_roles(), default=row['Assigned Roles'], key=f"roles_{row['Unique ID']}", format_func=format_role_display)
        if new != row['Assigned Roles']: changes[row['Unique ID']] = new
    if st.button("Save All Individual Changes"): handle_role_update(changes)