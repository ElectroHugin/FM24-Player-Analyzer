# role_analysis.py

import streamlit as st

from sqlite_db import get_user_club, get_second_team_club
from constants import get_valid_roles
from data_parser import get_players_by_role
from utils import format_role_display
from ui_components import display_custom_header

def role_analysis_page():
    #st.title("Role Analysis")
    display_custom_header("Role Analysis")
    user_club, second_club = get_user_club(), get_second_team_club()
    if not user_club:
        st.warning("Please select your club in the sidebar.")
        return
    role = st.selectbox("Select Role", options=get_valid_roles(), format_func=format_role_display)
    my_df, second_df, scout_df = get_players_by_role(role, user_club, second_club)
    st.subheader(f"Players from {user_club}")
    st.dataframe(my_df, use_container_width=True, hide_index=True)
    if second_club:
        st.subheader(f"Players from {second_club}")
        st.dataframe(second_df, use_container_width=True, hide_index=True)
    st.subheader("Scouted Players")
    st.dataframe(scout_df, use_container_width=True, hide_index=True)