# player_role_matrix.py

import streamlit as st
import pandas as pd
from io import StringIO
import math

from sqlite_db import (get_user_club, get_second_team_club, get_favorite_tactics,
                       get_shortlist_ids, set_shortlist_ids, get_club_country)
from constants import get_valid_roles, get_tactic_roles, get_personality_category
from data_parser import get_player_role_matrix
from utils import (get_last_name, get_natural_role_sorter, color_dwrs_by_value, value_to_float,
                   format_role_display, color_personality, color_attribute_by_value)
from talent_logic import add_talent_column
from ui_components import display_custom_header, clear_all_caches, personality_filter_controls, filter_df_by_personality


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

    # Personality filter (applies to club, second-team and scouted tables below)
    allowed_personalities = personality_filter_controls(full_matrix, key_prefix="matrix")
    full_matrix = filter_df_by_personality(full_matrix, allowed_personalities)

    # --- TALENT FILTER ---
    # Optional filter that narrows the rows of ALL tables below to young
    # players and adds a computed Talent Score. Nationality scope can be the
    # club's own country (for the board's home-grown demand), foreign players
    # only (to see what's hidden out there), or any nationality. Role columns
    # and role-based sorting keep working unchanged on top of it.
    club_country = get_club_country()
    talent_filter_on = False
    talent_scope = "Domestic"
    talent_age_cap = 21
    with st.expander("🌱 Talent Filter"):
        talent_filter_on = st.checkbox("Enable talent filter", value=False)
        scope_labels = {
            "Domestic": f"Domestic ({club_country})" if club_country else "Domestic (set country)",
            "Foreign": "Foreign (other nations)",
            "Any": "Any nationality",
        }
        talent_scope = st.radio(
            "Nationality scope",
            options=list(scope_labels.keys()),
            format_func=lambda s: scope_labels[s],
            horizontal=True,
            help="Domestic = your club's country (set in Settings → Club Identity). "
                 "Foreign = every other nationality. Any = no nationality restriction."
        )
        t1, t2, t3 = st.columns(3)
        with t1:
            talent_age_cap = st.slider("Maximum age", 15, 24, 21)
        with t2:
            talent_min_mentality = st.slider(
                "Min. Determination + Work Rate", 0, 40, 20,
                help="These two visible attributes are the strongest visible drivers of player development in FM."
            )
        with t3:
            talent_good_pers_only = st.checkbox("Good personalities only", value=False)
        st.caption(
            "**Talent Score** = best DWRS of the displayed roles "
            "+ 2 per year under the age cap (development runway) "
            "+ (Det + Wor − 20) / 4 "
            "+ 3 for a good / − 5 for a bad personality."
        )
        if talent_filter_on and talent_scope == "Domestic" and not club_country:
            st.warning("Set your **Club Country** in Settings → Club Identity & Theme to use the Domestic scope.")
            talent_filter_on = False

    if talent_filter_on:
        age_num = pd.to_numeric(full_matrix['Age'], errors='coerce')
        det_num = pd.to_numeric(full_matrix['Determination'], errors='coerce').fillna(0)
        wor_num = pd.to_numeric(full_matrix['Work Rate'], errors='coerce').fillna(0)

        if talent_scope == "Domestic":
            nat_mask = ((full_matrix['Nationality'] == club_country)
                        | (full_matrix['Second Nationality'] == club_country))
        elif talent_scope == "Foreign":
            has_nat = full_matrix['Nationality'].notna() & (full_matrix['Nationality'] != '')
            if club_country:
                nat_mask = (has_nat
                            & (full_matrix['Nationality'] != club_country)
                            & (full_matrix['Second Nationality'] != club_country))
            else:
                nat_mask = has_nat
        else:  # Any nationality
            nat_mask = pd.Series(True, index=full_matrix.index)

        talent_mask = (
            nat_mask
            & (age_num <= talent_age_cap)
            & ((det_num + wor_num) >= talent_min_mentality)
        )
        if talent_good_pers_only:
            talent_mask &= (full_matrix['Personality'].map(get_personality_category) == 'good')

        full_matrix = full_matrix[talent_mask].copy()
        full_matrix['Talent'] = add_talent_column(full_matrix, selected_roles, talent_age_cap).round(0)

        if full_matrix.empty:
            st.info("No players match the current talent criteria.")
            return

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
        # The Talent score column is formatted/colored like a DWRS value.
        score_cols = role_cols_df + [c for c in ('Talent',) if c in df_display.columns]

        # --- START OF CORRECTED STYLING LOGIC ---

        # Base styler for number formatting is always applied.
        styler = df_display.style.format("{:.0f}", subset=score_cols, na_rep="-")
        if 'Talent' in df_display.columns:
            styler = styler.apply(lambda col: [color_dwrs_by_value(v) for v in col], subset=['Talent'])
        mentality_cols = [c for c in ('Determination', 'Work Rate') if c in df_display.columns]
        if mentality_cols:
            styler = styler.map(color_attribute_by_value, subset=mentality_cols)

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

        if 'Personality' in df_display.columns:
            styler = styler.format(subset=['Personality'], na_rep="-")
            styler = styler.map(color_personality, subset=['Personality'])

        st.dataframe(styler, use_container_width=True, hide_index=True)
        # --- END OF CORRECTED STYLING LOGIC ---
        
        # CSV download button (unchanged)
        csv_buffer = StringIO()
        df_display.to_csv(csv_buffer, index=False)
        st.download_button(label=f"Download {title} Matrix as CSV", data=csv_buffer.getvalue(), file_name=f"{title.lower().replace(' ', '_')}_matrix.csv", mime="text/csv")

    # Calculate the total number of cells that will be rendered
    num_rows, num_cols = scouted_matrix.shape
    total_cells = num_rows * len(selected_roles) # A good approximation
    
    # Pandas has a default safety limit. We'll increase it if our table is larger,
    # then restore the original value afterwards to avoid app-wide side effects.
    current_max = pd.get_option("styler.render.max_elements")
    limit_was_increased = False
    if total_cells > current_max:
        new_limit = int(total_cells * 1.1)
        pd.set_option("styler.render.max_elements", new_limit)
        limit_was_increased = True
        st.info(f"💡 Large dataset detected. Temporarily increasing display limit to handle {total_cells:,} cells.")

    my_club_base_cols = ["Name", "Age", "Position", "Personality", "Wage"]
    if show_extra_details:
        my_club_base_cols.extend(["Left Foot", "Right Foot", "Height", "Transfer Value"])

    scouted_base_cols = ["Name", "Age", "Position", "Personality", "Club", "Transfer Value", "Wage"]
    if show_extra_details:
        scouted_base_cols.extend(["Left Foot", "Right Foot", "Height"])

    if talent_filter_on:
        # Show the score right after Age, plus the mentality attributes it uses.
        for cols in (my_club_base_cols, scouted_base_cols):
            cols.insert(cols.index("Age") + 1, "Talent")
            cols.extend(["Determination", "Work Rate"])

    my_club_display_cols = my_club_base_cols + selected_roles
    scouted_display_cols = scouted_base_cols + selected_roles

    if not show_second_team and second_team_club and not second_team_matrix.empty:
        combined_club_matrix = pd.concat([my_club_matrix, second_team_matrix]).sort_values(by='Name')
        prepare_and_display_df(combined_club_matrix, f"Players from {user_club} & Second Team", "combined_club", my_club_display_cols, use_full_style=True)
    else:
        prepare_and_display_df(my_club_matrix, f"Players from {user_club}", "my_club", my_club_display_cols, use_full_style=True)
        if second_team_club and show_second_team:
            prepare_and_display_df(second_team_matrix, f"Players from {second_team_club} (Second Team)", "second_team", my_club_display_cols, use_full_style=True)

    # Restore the original pandas option to avoid persistent app-wide side effects
    if limit_was_increased:
        pd.set_option("styler.render.max_elements", current_max)

    st.subheader("Scouted Players")
    
    if scouted_matrix.empty:
        st.info("No scouted players found.")
    else:
        shortlist_ids = get_shortlist_ids()

        with st.expander("🔍 Advanced Filtering & Sorting", expanded=True):
            sort_c1, sort_c2 = st.columns([2, 1])
            with sort_c1:
                talent_sort_options = ["Talent"] if talent_filter_on else []
                sort_options = ["Name", "Shortlist"] + talent_sort_options + selected_roles
                sort_by = st.selectbox(
                    "Filter & Sort by", options=sort_options,
                    format_func=lambda x: x if x in ("Name", "Shortlist", "Talent") else format_role_display(x)
                )
            with sort_c2:
                # Disable direction for shortlist filter as it doesn't apply
                sort_direction = st.radio("Direction", ["Descending", "Ascending"], horizontal=True, index=0, 
                                          disabled=(sort_by == "Shortlist"))

            dwrs_c1, age_c2, val_c3 = st.columns(3)
            with dwrs_c1:
                min_dwrs, max_dwrs = st.slider("Filter by DWRS Rating (for selected sort role)", 0, 100, (20, 100))
            with age_c2:
                max_age = st.slider("Filter by Max Age", 15, 40, 30)
            with val_c3:
                scouted_matrix['ValueNum'] = scouted_matrix['Transfer Value'].apply(value_to_float)
                buyable = scouted_matrix[scouted_matrix['ValueNum'] < 2_000_000_000]
                max_val_possible = buyable['ValueNum'].max() if not buyable.empty else 100_000_000
                slider_max = min(max_val_possible, 200_000_000)
                max_value = st.slider("Filter by Max Value (€M)", 0.0, slider_max / 1_000_000, slider_max / 1_000_000, 0.5) * 1_000_000

        # --- Apply Filtering and Sorting ---
        scouted_matrix['AgeNum'] = pd.to_numeric(scouted_matrix['Age'], errors='coerce')
        
        # Apply filters
        if sort_by == "Shortlist":
            filtered_df = scouted_matrix[scouted_matrix['Unique ID'].isin(shortlist_ids)].copy()
        else:
            # Original filtering logic
            filtered_df = scouted_matrix[
                (scouted_matrix['AgeNum'] <= max_age) &
                (scouted_matrix['ValueNum'] <= max_value)
            ].copy()
            
            if sort_by not in ("Name", "Talent"):
                # The DWRS range filter applies to role columns only; the
                # Talent score is already constrained by the talent filter.
                filtered_df = filtered_df[
                    (filtered_df[sort_by] >= min_dwrs) &
                    (filtered_df[sort_by] <= max_dwrs)
                ]

        search_term = st.text_input("Search by Name in Scouted Players", key="search_scouted")
        if search_term:
            filtered_df = filtered_df[filtered_df['Name'].str.contains(search_term, case=False, na=False)]

        # Apply sorting
        is_ascending = (sort_direction == "Ascending")
        if sort_by == "Name" or sort_by == "Shortlist":
            filtered_df['LastName'] = filtered_df['Name'].apply(get_last_name)
            sorted_df = filtered_df.sort_values(by='LastName', ascending=is_ascending).drop(columns=['LastName'])
        else: # This block now only runs for sorting by a role
            sorted_df = filtered_df.sort_values(by=sort_by, ascending=is_ascending, na_position='last')
        
        # --- PAGINATION CONTROLS (TOP) ---
        st.caption(f"Displaying {len(sorted_df)} matching players.")
        
        # Initialize session state for the page number
        if 'matrix_page_num' not in st.session_state:
            st.session_state.matrix_page_num = 1

        rows_per_page = 30
        total_pages = math.ceil(len(sorted_df) / rows_per_page) if rows_per_page > 0 else 1

        # Ensure page number is within valid range
        if st.session_state.matrix_page_num > total_pages:
            st.session_state.matrix_page_num = 1
        
        page_c1_top, page_c2_top = st.columns([1, 3])
        page_c1_top.number_input(
            "Page:", min_value=1, max_value=max(1, total_pages),
            key='matrix_page_num' # Link directly to session state
        )
        
        # Slice the dataframe for the current page
        start_idx = (st.session_state.matrix_page_num - 1) * rows_per_page
        end_idx = start_idx + rows_per_page
        df_paginated = sorted_df.iloc[start_idx:end_idx]

        # --- Display and Full Styling ---
        df_display = df_paginated[[c for c in scouted_display_cols if c in df_paginated.columns]]
        role_cols_df = [role for role in get_valid_roles() if role in df_display.columns]
        score_cols = role_cols_df + [c for c in ('Talent',) if c in df_display.columns]

        styler = df_display.style.format("{:.0f}", subset=score_cols, na_rep="-")
        def smart_full_styler(column):
            styles = [''] * len(column)
            valid_indices = column.dropna().index
            for index in valid_indices:
                styles[column.index.get_loc(index)] = color_dwrs_by_value(column[index])
            return styles
        styler = styler.apply(smart_full_styler, subset=score_cols)
        mentality_cols = [c for c in ('Determination', 'Work Rate') if c in df_display.columns]
        if mentality_cols:
            styler = styler.map(color_attribute_by_value, subset=mentality_cols)
        
        if 'Personality' in df_display.columns:
            styler = styler.format(subset=['Personality'], na_rep="-")
            styler = styler.map(color_personality, subset=['Personality'])

        st.dataframe(styler, use_container_width=True, hide_index=True)

        # --- CHANGE 3: Add the "Add to Shortlist" widget ---
        # --- CONTEXT-AWARE SHORTLIST MANAGEMENT ---
        st.markdown("---")

        # If the user is currently filtering by "Shortlist", show the removal tool.
        if sort_by == "Shortlist":
            st.subheader("Remove Players from Shortlist")

            # In this mode, sorted_df contains all the shortlisted players.
            # We create a map for the multiselect widget.
            shortlisted_players_map = dict(zip(sorted_df['Unique ID'], sorted_df['Name']))

            if not shortlisted_players_map:
                st.info("Your shortlist is currently empty.")
            else:
                players_to_remove = st.multiselect(
                    "Select players to remove from your shortlist:",
                    options=list(shortlisted_players_map.keys()),
                    format_func=lambda uid: shortlisted_players_map[uid],
                    placeholder="Choose players to remove..."
                )

                if st.button("Remove Selected Players from Shortlist", type="primary"):
                    if not players_to_remove:
                        st.warning("Please select at least one player to remove.")
                    else:
                        # Use set difference to calculate the new shortlist
                        new_shortlist = shortlist_ids.difference(set(players_to_remove))
                        set_shortlist_ids(new_shortlist)
                        st.success(f"Successfully removed {len(players_to_remove)} player(s) from the shortlist!")
                        clear_all_caches()
                        st.rerun()
        
        # Otherwise, in any other view, show the "Add to Shortlist" tool.
        else:
            st.subheader("Add Players to Shortlist")
            
            # Create a map of players currently visible on the page
            visible_players_map = dict(zip(df_paginated['Unique ID'], df_paginated['Name']))
            
            if not visible_players_map:
                st.info("No players are currently visible to add.")
            else:
                players_to_add = st.multiselect(
                    "Select from currently visible players:",
                    options=list(visible_players_map.keys()),
                    format_func=lambda uid: visible_players_map[uid],
                    placeholder="Choose players to add..."
                )
                
                if st.button("Add Selected Players to Shortlist", type="primary"):
                    if not players_to_add:
                        st.warning("Please select at least one player to add.")
                    else:
                        # Use set union to add new players
                        new_shortlist = shortlist_ids.union(set(players_to_add))
                        set_shortlist_ids(new_shortlist)
                        st.success(f"Successfully added {len(players_to_add)} player(s) to the shortlist!")
                        clear_all_caches() 
                        st.rerun()