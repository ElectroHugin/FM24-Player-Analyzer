# player_role_matrix.py

import streamlit as st
import pandas as pd
from io import StringIO

from sqlite_db import get_user_club, get_second_team_club, get_favorite_tactics
from constants import get_valid_roles, get_tactic_roles
from data_parser import get_player_role_matrix
from utils import get_last_name, get_natural_role_sorter, color_dwrs_by_value
from ui_components import display_custom_header


def player_role_matrix_page():
    #st.title("Player-Role Matrix")
    display_custom_header("Squad Matrix")
    st.write("View DWRS ratings for players in assigned roles. Select a tactic to see relevant roles, and toggle extra details.")
    
    user_club = get_user_club()
    second_team_club = get_second_team_club()
    if not user_club:
        st.warning("Please select your club in the sidebar to use this page.")
        return
    
    st.subheader("Display Options")
    col1, col2, col3, col4 = st.columns(4)
    with col1:
        fav_tactic1, fav_tactic2 = get_favorite_tactics()
        all_tactics = sorted(list(get_tactic_roles().keys()))
        
        # Create the sorted list
        sorted_tactics = []
        if fav_tactic1 and fav_tactic1 in all_tactics:
            sorted_tactics.append(fav_tactic1)
        if fav_tactic2 and fav_tactic2 in all_tactics and fav_tactic2 != fav_tactic1:
            sorted_tactics.append(fav_tactic2)
        
        # Add remaining tactics
        for tactic in all_tactics:
            if tactic not in sorted_tactics:
                sorted_tactics.append(tactic)
        
        tactic_options = ["All Roles"] + sorted_tactics
        
        # The index will be 1 if a favorite is set, otherwise 0
        default_index = 1 if fav_tactic1 else 0
        
        selected_tactic = st.selectbox("Select Tactic", options=tactic_options, index=default_index)
    with col2:
        show_extra_details = st.checkbox("Show Extra Details")
    with col3:
        show_second_team = st.checkbox("Show Second Team separately", value=False)
    with col4:
        hide_retired = st.checkbox("Hide 'Retired' Players", value=True)
    
    role_sorter = get_natural_role_sorter()
    base_roles = get_valid_roles() if selected_tactic == "All Roles" else list(set(get_tactic_roles()[selected_tactic].values()))
    selected_roles = sorted(base_roles, key=lambda r: role_sorter.get(r, (99, 99)))
    full_matrix = get_player_role_matrix(user_club, second_team_club)
    
    if full_matrix.empty:
        st.info("No player data to generate matrix.")
        return

    if hide_retired:
        full_matrix = full_matrix[full_matrix['Club'].str.lower() != 'retired']

    my_club_matrix = full_matrix[full_matrix['Club'] == user_club].copy()
    second_team_matrix = full_matrix[full_matrix['Club'] == second_team_club].copy() if second_team_club else pd.DataFrame()
    exclude_clubs = [user_club]
    if second_team_club: exclude_clubs.append(second_team_club)
    scouted_matrix = full_matrix[~full_matrix['Club'].isin(exclude_clubs)].copy()

    def prepare_and_display_df(df, title, key_suffix, display_cols, use_full_style=False, top_n=20):
        st.subheader(title)
        
        # Data preparation and search logic (this is all correct)
        if not df.empty:
            df['LastName'] = df['Name'].apply(get_last_name)
            df = df.sort_values(by=['LastName', 'Name']).drop(columns=['LastName'])
        search_term = st.text_input(f"Search by Name in {title}", key=f"search_{key_suffix}")
        if search_term:
            df = df[df['Name'].str.contains(search_term, case=False, na=False)]
        if df.empty:
            st.write("No players found for this category or matching the filter.")
            return
        existing_cols = [col for col in display_cols if col in df.columns]
        df_display = df[existing_cols]
        role_cols_df = [role for role in get_valid_roles() if role in df_display.columns]

        # --- START OF CORRECTED STYLING LOGIC ---
        
        # Base styler for number formatting is always applied.
        styler = df_display.style.format("{:.0f}", subset=role_cols_df, na_rep="-")

        if use_full_style:
            # For the "My Club" table, we use a smart function that styles
            # only the cells that are not empty (not None/NaN).
            
            def smart_full_styler(column):
                styles = [''] * len(column)
                # Find the index positions of all cells that have a valid number
                valid_indices = column.dropna().index
                # For ONLY those valid cells, calculate and apply the color style
                for index in valid_indices:
                    # get_loc finds the integer position of the index label
                    styles[column.index.get_loc(index)] = color_dwrs_by_value(column[index])
                return styles

            # Apply this intelligent function to each role column
            styler = styler.apply(smart_full_styler, subset=role_cols_df)

        else:
            # For the large scouted table, we continue to use the high-performance "Top N" logic.
            for role in role_cols_df:
                top_indices = df_display[role].nlargest(top_n).index
                styler = styler.apply(
                    lambda x: x.map(color_dwrs_by_value),
                    subset=pd.IndexSlice[top_indices, [role]]
                )

        st.dataframe(styler, use_container_width=True, hide_index=True)
        # --- END OF CORRECTED STYLING LOGIC ---
        
        # CSV download button (unchanged)
        csv_buffer = StringIO()
        df_display.to_csv(csv_buffer, index=False)
        st.download_button(label=f"Download {title} Matrix as CSV", data=csv_buffer.getvalue(), file_name=f"{title.lower().replace(' ', '_')}_matrix.csv", mime="text/csv")

    # Calculate the total number of cells that will be rendered
    num_rows, num_cols = scouted_matrix.shape
    total_cells = num_rows * len(selected_roles) # A good approximation
    
    # Pandas has a default safety limit. We'll increase it if our table is larger.
    # We add a buffer (e.g., * 1.1) just to be safe.
    current_max = pd.get_option("styler.render.max_elements")
    if total_cells > current_max:
        new_limit = int(total_cells * 1.1)
        pd.set_option("styler.render.max_elements", new_limit)
        st.info(f"💡 Large dataset detected. Temporarily increasing display limit to handle {total_cells:,} cells.")

    my_club_base_cols = ["Name", "Age", "Position", "Wage"]
    if show_extra_details:
        my_club_base_cols.extend(["Left Foot", "Right Foot", "Height", "Transfer Value"])
    my_club_display_cols = my_club_base_cols + selected_roles

    scouted_base_cols = ["Name", "Age", "Position", "Club", "Transfer Value", "Wage"]
    if show_extra_details:
        scouted_base_cols.extend(["Left Foot", "Right Foot", "Height"])
    scouted_display_cols = scouted_base_cols + selected_roles

    if not show_second_team and second_team_club and not second_team_matrix.empty:
        combined_club_matrix = pd.concat([my_club_matrix, second_team_matrix]).sort_values(by='Name')
        prepare_and_display_df(combined_club_matrix, f"Players from {user_club} & Second Team", "combined_club", my_club_display_cols, use_full_style=True)
    else:
        prepare_and_display_df(my_club_matrix, f"Players from {user_club}", "my_club", my_club_display_cols, use_full_style=True)
        if second_team_club and show_second_team:
            prepare_and_display_df(second_team_matrix, f"Players from {second_team_club} (Second Team)", "second_team", my_club_display_cols, use_full_style=True)

    st.subheader("Scouted Players Styling")
    top_n_to_style = st.slider(
        "Highlight Top N Players per Role", 
        min_value=5, max_value=50, value=20, step=5,
        help="To maintain performance on large lists, only the top N players for each role will be color-graded."
    )

    prepare_and_display_df(scouted_matrix, "Scouted Players", "scouted", scouted_display_cols, use_full_style=False, top_n=top_n_to_style)