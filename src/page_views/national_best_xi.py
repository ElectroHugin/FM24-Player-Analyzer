# page_views/national_best_xi.py

import streamlit as st
import pandas as pd

from constants import get_tactic_roles, get_tactic_layouts
from sqlite_db import get_national_team_settings, get_national_squad_ids, get_national_favorite_tactics
from config_handler import get_theme_settings
from ui_components import display_tactic_grid, display_custom_header
from squad_logic import calculate_squad_and_surplus, get_master_role_ratings
from utils import get_natural_role_sorter, format_role_display

def national_best_xi_page(players):
    """
    Calculates and displays the Best XI, B-Team, and depth for the selected national squad.
    """
    # --- 1. SETUP & DATA LOADING ---
    nat_name, _, _ = get_national_team_settings()
    display_custom_header(f"{nat_name or 'National'} Best XI")

    if not nat_name:
        st.warning("Please configure your national team details in Settings to use this page.")
        return

    # Filter the master player list to only include players from the national squad
    squad_ids = get_national_squad_ids()
    if not squad_ids:
        st.info("No players have been selected for the squad. Go to 'National Squad Selection' to build your team.")
        return
        
    squad_players = [p for p in players if p['Unique ID'] in squad_ids]
    
    # --- 2. TACTIC SELECTION ---
    fav_tactic1, fav_tactic2 = get_national_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    sorted_tactics = []
    if fav_tactic1 and fav_tactic1 in all_tactics: sorted_tactics.append(fav_tactic1)
    if fav_tactic2 and fav_tactic2 in all_tactics and fav_tactic2 != fav_tactic1: sorted_tactics.append(fav_tactic2)
    for tactic in all_tactics:
        if tactic not in sorted_tactics: sorted_tactics.append(tactic)
    
    tactic = st.selectbox("Select Tactic", options=sorted_tactics, index=0)
    
    positions = get_tactic_roles()[tactic]
    layout = get_tactic_layouts()[tactic]
    theme_settings = get_theme_settings()
    current_mode = theme_settings.get('current_mode', 'night')

    # --- 3. CORE CALCULATION ---
    squad_analysis = None
    with st.spinner("Calculating best lineup for the national squad..."):
        # Get the master dictionary of pre-calculated DWRS ratings
        master_role_ratings = get_master_role_ratings()

        # REUSE the core logic from squad_logic, feeding it our national player pool
        squad_analysis = calculate_squad_and_surplus(squad_players, positions, master_role_ratings)

    if not squad_analysis:
        st.error("Could not generate a squad. Ensure players are assigned relevant roles.")
        return

    # --- 4. UI DISPLAY (Simplified from the club version) ---
    st.header("National Team Analysis")
    with st.expander("How does it work?"):
        st.info("This tool uses a 'weakest link first' algorithm. Instead of picking the best player for each position one by one, it evaluates all open positions simultaneously and fills the one where the best available player provides the smallest upgrade. This creates a more balanced and often stronger overall team.")
    
    # Display Starting XI and B-Team grids side-by-side
    xi_col, b_team_col = st.columns(2)
    with xi_col:
        display_tactic_grid(squad_analysis["starting_xi"], "Starting XI", positions, layout, mode=current_mode)
    with b_team_col:
        display_tactic_grid(squad_analysis["b_team"], "B Team", positions, layout, mode=current_mode)

    st.divider()

    # Display the remaining players as depth options
    with st.expander("Additional Depth", expanded=True):
        best_depth_options = squad_analysis["best_depth_options"]
        if best_depth_options:
            role_sorter = get_natural_role_sorter()
            sorted_roles = sorted(best_depth_options.keys(), key=lambda r: role_sorter.get(r, (99,99)))

            for role in sorted_roles:
                players_list = best_depth_options.get(role, [])
                if players_list:
                    # Note: National players don't have an APT, so we remove it from the display string
                    player_strs = [
                        f"{p['name']} ({p['age']}) - {p['rating']}" 
                        for p in players_list
                    ]
                    display_str = ', '.join(player_strs)
                    st.markdown(f"**{format_role_display(role)}**: {display_str}")
        else:
            st.info("No other players in the squad were suitable as depth options for this tactic.")