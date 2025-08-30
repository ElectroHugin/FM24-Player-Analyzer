# best_position.py

import streamlit as st
import pandas as pd

from constants import get_tactic_roles, get_tactic_layouts
from sqlite_db import get_user_club, get_second_team_club, get_favorite_tactics
from config_handler import get_theme_settings
from ui_components import display_tactic_grid, display_custom_header
from squad_logic import get_cached_squad_analysis, create_detailed_surplus_df
from utils import get_natural_role_sorter, format_role_display

def best_position_calculator_page(players):
    #st.title("Best Position Calculator")
    display_custom_header("Best XI Calculator")
    user_club = get_user_club()
    second_team_club = get_second_team_club()

    if not user_club:
        st.warning("Please select your club in the sidebar.")
        return

    # Tactic selection
    fav_tactic1, fav_tactic2 = get_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    sorted_tactics = []
    if fav_tactic1 and fav_tactic1 in all_tactics: sorted_tactics.append(fav_tactic1)
    if fav_tactic2 and fav_tactic2 in all_tactics and fav_tactic2 != fav_tactic1: sorted_tactics.append(fav_tactic2)
    for tactic in all_tactics:
        if tactic not in sorted_tactics: sorted_tactics.append(tactic)
    tactic = st.selectbox("Select Tactic", options=sorted_tactics, index=0)
    
    positions, layout = get_tactic_roles()[tactic], get_tactic_layouts()[tactic]
    theme_settings = get_theme_settings()
    current_mode = theme_settings.get('current_mode', 'night')

    # --- EFFICIENT CALCULATION STEP ---
    analysis_results = None
    with st.spinner("Analyzing squad structure..."):
        analysis_results = get_cached_squad_analysis(players, tactic, user_club, second_team_club)

    if not analysis_results:
        st.warning(f"Could not generate a squad for the '{tactic}' tactic. There may be no suitable players in your club.")
        return

    # Unpack ALL the results we need from the single cached dictionary
    first_team_squad_data = analysis_results["first_team_squad_data"]
    dev_squad_data = analysis_results["dev_squad_data"]
    injury_log = analysis_results.get("injury_log", [])
    my_club_players = analysis_results["my_club_players"]
    master_role_ratings = analysis_results["master_role_ratings"]

    # --- UI CREATION (The rest of the file is the same as yours) ---
    column_config = {
        "Best DWRS": st.column_config.NumberColumn(format="%d%%"),
        "Det": st.column_config.TextColumn(help="Determination"),
        "Wor": st.column_config.TextColumn(help="Work Rate"),
    }
    tab1, tab2 = st.tabs(["First Team Squad", "Youth & Second Team"])
    with tab1:
        st.header("First Team Analysis")
        with st.expander("How does it work?"):
            st.info("This tool uses a 'weakest link first' algorithm. Instead of picking the best player for each position one by one, it evaluates all open positions simultaneously and fills the one where the best available player provides the smallest upgrade. This creates a more balanced and often stronger overall team.")
        # Display the injury log in an expander if there are any injuries
        if injury_log:
            with st.expander("🚑 **Injury Report & Squad Adjustments:**"):
                for log_entry in injury_log:
                    st.markdown(f"- {log_entry}")
        xi_col, b_team_col = st.columns(2)
        with xi_col:
            xi_title = "Starting XI (Adjusted for Injuries)" if injury_log else "Starting XI"
            display_tactic_grid(first_team_squad_data["starting_xi"], xi_title, positions, layout, mode=current_mode)
        with b_team_col:
            display_tactic_grid(first_team_squad_data["b_team"], "B Team", positions, layout, mode=current_mode)

        st.divider()

        with st.expander("Additional Depth", expanded=True):
            best_depth_options = first_team_squad_data["best_depth_options"]
            if best_depth_options:
                role_sorter = get_natural_role_sorter()
                sorted_roles = sorted(best_depth_options.keys(), key=lambda r: role_sorter.get(r, (99,99)))
                for role in sorted_roles:
                    players_list = best_depth_options.get(role, [])
                    if players_list:
                        player_strs = [f"{p['name']} ({p['age']}) - {p['rating']}" for p in players_list]
                        display_str = ', '.join(player_strs)
                        st.markdown(f"**{format_role_display(role)}**: {display_str}")
            else:
                st.info("No other players were suitable as depth options for this tactic.")

    # --- All content for the second tab goes inside this "with" block ---
    with tab2:
        with st.expander("Development Squad Analysis"):
            st.info("This section displays the optimal Youth XI from your young players and the best XI for your Second Team based on the selected tactic.")

        second_col, youth_col = st.columns(2)

        with second_col:
            second_team_title = f"Second Team XI ({second_team_club or 'From Main Club Surplus'})"
            display_tactic_grid(dev_squad_data["second_team_xi"], second_team_title, positions, layout, mode=current_mode)
        
        with youth_col:
            display_tactic_grid(dev_squad_data["youth_xi"], "Youth XI", positions, layout, mode=current_mode)

        st.divider()
        st.subheader("Development & Transfer Candidates")
        st.success("✔️ To manage these players, go to the **Transfer & Loan Management** page in the sidebar!")

        loan_candidates_df = create_detailed_surplus_df(dev_squad_data.get("loan_candidates", []), master_role_ratings)
        if not loan_candidates_df.empty:
            st.markdown(f"**Promising Youth for Loan (Work Rate + Determination >= 20):**")
            st.dataframe(loan_candidates_df, hide_index=True, use_container_width=True, column_config=column_config)
        else:
            st.info("No promising youth players identified for loan.")

        sell_candidates_df = create_detailed_surplus_df(dev_squad_data.get("sell_candidates", []), master_role_ratings)
        if not sell_candidates_df.empty:
            st.markdown(f"**Surplus Players for Sale / Release:**")
            st.dataframe(sell_candidates_df, hide_index=True, use_container_width=True, column_config=column_config)
        else:
            st.info("No surplus players identified for sale.")