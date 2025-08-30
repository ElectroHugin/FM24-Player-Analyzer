# role_analysis.py

import streamlit as st
import pandas as pd

from sqlite_db import get_user_club, get_second_team_club
from constants import get_valid_roles
from data_parser import get_players_by_role
from utils import format_role_display, color_dwrs_by_value
from ui_components import display_custom_header

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

    # --- REPLACE st.dataframe calls with our new styled function ---
    display_styled_role_df(my_df, f"Players from {user_club}", use_full_style=True)
    
    if second_club:
        display_styled_role_df(second_df, f"Players from {second_club}", use_full_style=True)
        
    display_styled_role_df(scout_df, "Scouted Players", use_full_style=False, top_n=200)