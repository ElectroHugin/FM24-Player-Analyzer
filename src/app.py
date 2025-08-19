# app.py

import streamlit as st
import pandas as pd
import re
from io import StringIO
import csv
import urllib.parse
from data_parser import load_data, parse_and_update_data, get_filtered_players, get_players_by_role, get_player_role_matrix
from sqlite_db import (update_player_roles, get_user_club, set_user_club, update_dwrs_ratings, get_all_players, 
                     get_dwrs_history, get_second_team_club, set_second_team_club, update_player_club, set_primary_role)
from constants import (CSS_STYLES, SORTABLE_COLUMNS, FILTER_OPTIONS, PLAYER_ROLE_MATRIX_COLUMNS,
                     get_player_roles, get_valid_roles, get_position_to_role_mapping, 
                     get_tactic_roles, get_tactic_layouts)
from config_handler import get_weight, set_weight, get_role_multiplier, set_role_multiplier, get_db_name, set_db_name
from definitions_handler import get_definitions, save_definitions

st.set_page_config(page_title="FM 2024 Player Dashboard", layout="wide")
st.markdown(CSS_STYLES, unsafe_allow_html=True)

def clear_all_caches():
    st.cache_data.clear()

def get_role_display_map():
    player_roles = get_player_roles()
    return {role: name for category in player_roles.values() for role, name in category.items()}

def format_role_display(role_abbr):
    return get_role_display_map().get(role_abbr, role_abbr)

def format_role_display_with_all(role_abbr):
    return "All Roles" if role_abbr == "All Roles" else get_role_display_map().get(role_abbr, role_abbr)

def sidebar():
    with st.sidebar:
        st.header("Navigation")
        page_options = ["All Players", "Assign Roles", "Role Analysis", "Player-Role Matrix", "Best Position Calculator", "Player Comparison", "DWRS Progress", "Edit Player Data", "Create New Role", "Settings"]
        page = st.radio("Go to", page_options)
        uploaded_file = st.file_uploader("Upload HTML File", type=["html"])
        df = load_data()
        club_options = ["Select a club"] + sorted(df['Club'].unique()) if df is not None else ["Select a club"]
        current_club = get_user_club() or "Select a club"
        selected_club = st.selectbox("Your Club", options=club_options, index=club_options.index(current_club))
        if selected_club != current_club and selected_club != "Select a club":
            set_user_club(selected_club)
            st.rerun()
        current_second = get_second_team_club() or "Select a club"
        selected_second = st.selectbox("Your Second Team", options=club_options, index=club_options.index(current_second))
        if selected_second != current_second and selected_second != "Select a club":
            set_second_team_club(selected_second)
            st.rerun()
        return page, uploaded_file

def main_page(uploaded_file):
    st.title("Player Dashboard")
    if uploaded_file:
        with st.spinner("Processing file..."):
            df = parse_and_update_data(uploaded_file)
        if df is None:
            st.error("Invalid HTML file: Must contain a table with a 'UID' column.")
            return
        clear_all_caches()
        update_dwrs_ratings(df, get_valid_roles())
        st.success("Data updated successfully!")
        st.rerun()
    df = load_data()
    if df is not None:
        st.subheader("All Players")
        st.dataframe(df, use_container_width=True, hide_index=True)
    else:
        st.info("Please upload an HTML file to display player data.")

def parse_position_string(pos_str):
    if not isinstance(pos_str, str): return set()
    final_pos = set()
    for part in [p.strip() for p in pos_str.split(',')]:
        match = re.match(r'([A-Z/]+) *(?:\(([RLC]+)\))?$', part.strip())
        if match:
            bases, sides = match.groups()
            for base in bases.split('/'):
                if sides:
                    for side in list(sides): final_pos.add(f"{base} ({side})")
                else:
                    final_pos.add(f"{base} (C)" if base == "ST" else base)
    return final_pos

def assign_roles_page():
    st.title("Assign Roles to Players")
    df = pd.DataFrame(get_all_players())
    if df.empty:
        st.info("No players available. Please upload player data.")
        return
    st.subheader("Filter & Sort")
    c1, c2, c3 = st.columns(3)
    filter_option = c1.selectbox("Filter by", options=FILTER_OPTIONS)
    club_filter = c2.selectbox("Filter by Club", options=["All"] + sorted(df['Club'].unique()))
    pos_filter = c3.selectbox("Filter by Position", options=["All"] + sorted(df['Position'].unique()))
    sort_column = c1.selectbox("Sort by", options=SORTABLE_COLUMNS)
    sort_order = c2.selectbox("Sort Order", options=["Ascending", "Descending"])
    filtered_df = get_filtered_players(filter_option, club_filter, pos_filter, sort_column, (sort_order == "Ascending"), get_user_club())
    search = st.text_input("Search by Name")
    if search: filtered_df = filtered_df[filtered_df['Name'].str.contains(search, case=False, na=False)]
    
    def handle_role_update(role_changes):
        if role_changes:
            update_player_roles(role_changes)
            affected = df[df['Unique ID'].isin(role_changes.keys())]
            update_dwrs_ratings(affected, get_valid_roles())
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

def role_analysis_page():
    st.title("Role Analysis")
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

# --- RESTORED: Player-Role Matrix page function ---
def player_role_matrix_page():
    st.title("Player-Role Matrix")
    st.write("View DWRS ratings for players in assigned roles. Select a tactic to see relevant roles, and toggle extra details.")
    
    user_club = get_user_club()
    second_team_club = get_second_team_club()
    if not user_club:
        st.warning("Please select your club in the sidebar to use this page.")
        return
    
    st.subheader("Display Options")
    col1, col2, col3, col4 = st.columns(4)
    with col1:
        tactic_options = ["All Roles"] + list(get_tactic_roles().keys())
        selected_tactic = st.selectbox("Select Tactic", options=tactic_options)
    with col2:
        show_extra_details = st.checkbox("Show Extra Details")
    with col3:
        show_second_team = st.checkbox("Show Second Team separately", value=False)
    with col4:
        hide_retired = st.checkbox("Hide 'Retired' Players", value=True)
    
    selected_roles = get_valid_roles() if selected_tactic == "All Roles" else sorted(list(set(get_tactic_roles()[selected_tactic].values())))
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

    def prepare_and_display_df(df, title, key_suffix, display_cols):
        st.subheader(title)
        search_term = st.text_input(f"Search by Name in {title}", key=f"search_{key_suffix}")
        if search_term:
            df = df[df['Name'].str.contains(search_term, case=False, na=False)]

        if df.empty:
            st.write("No players found for this category or matching the filter.")
            return

        existing_cols = [col for col in display_cols if col in df.columns]
        df_display = df[existing_cols]
        st.dataframe(df_display, use_container_width=True, hide_index=True)
        
        csv_buffer = StringIO()
        df_display.to_csv(csv_buffer, index=False)
        st.download_button(label=f"Download {title} Matrix as CSV", data=csv_buffer.getvalue(), file_name=f"{title.lower().replace(' ', '_')}_matrix.csv", mime="text/csv")

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
        prepare_and_display_df(combined_club_matrix, f"Players from {user_club} & Second Team", "combined_club", my_club_display_cols)
    else:
        # --- START OF FIX ---
        prepare_and_display_df(my_club_matrix, f"Players from {user_club}", "my_club", my_club_display_cols)
        if second_team_club and show_second_team:
            prepare_and_display_df(second_team_matrix, f"Players from {second_team_club} (Second Team)", "second_team", my_club_display_cols)
        # --- END OF FIX ---

    # --- FINAL FIX ---
    prepare_and_display_df(scouted_matrix, "Scouted Players", "scouted", scouted_display_cols)
    # --- END FINAL FIX ---

def best_position_calculator_page():
    st.title("Best Position Calculator")
    st.write("Players with a 'Primary Role' are locked to that role and prioritized. All other positions are filled by the best available players.")
    
    # --- EASY TOGGLE FOR DEBUG LOG ---
    show_selection_log = False
    # ---------------------------------

    # --- Data Loading and Setup ---
    user_club = get_user_club()
    if not user_club:
        st.warning("Please select your club in the sidebar.")
        return

    tactic = st.selectbox("Select Tactic", options=list(get_tactic_roles().keys()))
    positions, layout = get_tactic_roles()[tactic], get_tactic_layouts()[tactic]
    all_players = get_all_players()
    my_club_players = [p for p in all_players if p['Club'] == user_club]
    
    log_messages = []

    # 1. Pre-calculate and store sorted lists of players for each role
    role_players = {}
    for pos, role in positions.items():
        df, _, _ = get_players_by_role(role, user_club)
        if not df.empty:
            df['DWRS'] = pd.to_numeric(df['DWRS Rating (Normalized)'].str.rstrip('%'))
            role_players[pos] = df.sort_values(by='DWRS', ascending=False)

    # --- Initialization ---
    starting_xi = {p: ("-", "0%") for p in positions}
    b_team = {p: ("-", "0%") for p in positions}
    depth = {p: [] for p in positions}
    assigned_player_ids = set()

    # --- STEP 1 & 2: PLACE ALL PLAYERS WITH A PRIMARY ROLE ---
    log_messages.append("--- Stage 1: Assigning players with a Primary Role ---")
    
    primary_role_candidates = {}
    for player_data in my_club_players:
        primary_role = player_data.get('primary_role')
        if primary_role:
            for pos, role in positions.items():
                if role == primary_role:
                    if pos not in primary_role_candidates:
                        primary_role_candidates[pos] = []
                    
                    rating = 0
                    if pos in role_players:
                        player_row = role_players[pos][role_players[pos]['Unique ID'] == player_data['Unique ID']]
                        if not player_row.empty:
                            rating = player_row.iloc[0]['DWRS']
                    
                    primary_role_candidates[pos].append({
                        'id': player_data['Unique ID'],
                        'name': player_data['Name'],
                        'rating': rating
                    })
                    log_messages.append(f"Found Primary Role Candidate: {player_data['Name']} for {pos} ({role}) at {rating:.0f}%.")
                    break
    
    for pos, candidates in primary_role_candidates.items():
        candidates.sort(key=lambda x: x['rating'], reverse=True)
        
        if len(candidates) > 0:
            starter = candidates[0]
            starting_xi[pos] = (starter['name'], f"{int(starter['rating'])}%")
            assigned_player_ids.add(starter['id'])
            log_messages.append(f"SUCCESS: Placed {starter['name']} in Starting XI at {pos} (Primary Role).")

        if len(candidates) > 1:
            backup = candidates[1]
            b_team[pos] = (backup['name'], f"{int(backup['rating'])}%")
            assigned_player_ids.add(backup['id'])
            log_messages.append(f"SUCCESS: Placed {backup['name']} in B-Team at {pos} (Primary Role).")

    # --- STEP 3: FILL REMAINING SPOTS WITH BEST AVAILABLE PLAYERS ---
    log_messages.append("\n--- Stage 2: Filling remaining spots with best available players ---")

    remaining_assignments = []
    for pos, df_role_players in role_players.items():
        for _, player in df_role_players.iterrows():
            if player['Unique ID'] not in assigned_player_ids:
                remaining_assignments.append({
                    'rating': player['DWRS'],
                    'player_name': player['Name'],
                    'player_id': player['Unique ID'],
                    'position': pos,
                })

    remaining_assignments.sort(key=lambda x: x['rating'], reverse=True)

    for assignment in remaining_assignments:
        pos = assignment['position']
        p_id = assignment['player_id']
        if starting_xi[pos][0] == "-" and p_id not in assigned_player_ids:
            starting_xi[pos] = (assignment['player_name'], f"{int(assignment['rating'])}%")
            assigned_player_ids.add(p_id)
            log_messages.append(f"SUCCESS: Placed {assignment['player_name']} in Starting XI at {pos} (Best Available).")

    for assignment in remaining_assignments:
        pos = assignment['position']
        p_id = assignment['player_id']
        if b_team[pos][0] == "-" and p_id not in assigned_player_ids:
            b_team[pos] = (assignment['player_name'], f"{int(assignment['rating'])}%")
            assigned_player_ids.add(p_id)
            log_messages.append(f"SUCCESS: Placed {assignment['player_name']} in B-Team at {pos} (Best Available).")
            
    # --- STEP 4: POPULATE DEPTH CHART (THE FIX) ---
    log_messages.append("\n--- Stage 3: Populating depth chart ---")
    for pos in positions:
        if pos in role_players:
            # Filter for players not in the starting XI or B-Team
            unassigned_for_role = role_players[pos][~role_players[pos]['Unique ID'].isin(assigned_player_ids)]
            
            # Take the top 3 unassigned players for depth
            for _, player in unassigned_for_role.head(3).iterrows():
                depth[pos].append((player['Name'], f"{int(player['DWRS'])}%"))
                log_messages.append(f"DEPTH: Added {player['Name']} ({int(player['DWRS'])}%) to depth for {pos}.")

    # --- Display Log (Conditionally) ---
    if show_selection_log:
        with st.expander("Show Team Selection Log"):
            st.code('\n'.join(log_messages))

    # --- Display Logic (no changes needed) ---
    def display_layout(team, title):
        st.subheader(title)
        gk_pos_key = 'GK'
        gk_role = positions.get(gk_pos_key, "GK")
        name, rating = team.get(gk_pos_key, ("-", "0%"))
        gk_display = f"<div style='text-align: center;'><b>{gk_pos_key}</b> ({gk_role})<br>{name}<br><i>{rating}</i></div>"
        
        for row in reversed(layout):
            cols = st.columns(len(row))
            for i, pos in enumerate(row):
                name, rating = team.get(pos, ("-", "0%"))
                role = positions.get(pos, "")
                cols[i].markdown(f"<div style='text-align: center; border: 1px solid #444; border-radius: 5px; padding: 10px; height: 100%;'><b>{pos}</b> ({role})<br>{name}<br><i>{rating}</i></div>", unsafe_allow_html=True)
            st.write("")
        
        st.markdown(gk_display, unsafe_allow_html=True)

    display_layout(starting_xi, "Starting XI")
    st.divider()
    display_layout(b_team, "B Team")
    st.divider()

    st.subheader("Additional Depth")
    for pos, players in depth.items():
        if players:
            player_str = ', '.join([f'{name} ({rating})' for name, rating in players])
            st.markdown(f"**{pos} ({positions[pos]})**: {player_str}")

def player_comparison_page():
    st.title("Player Comparison")
    df = pd.DataFrame(get_all_players())
    if df.empty:
        st.info("No players available.")
        return
    names = st.multiselect("Select players to compare", options=sorted(df['Name'].unique()))
    if names:
        #st.dataframe(df[df['Name'].isin(names)].set_index('Name').T, use_container_width=True)
        comparison_df = df[df['Name'].isin(names)].copy()
        
        # Convert the 'Assigned Roles' list to a readable string to prevent the ArrowTypeError
        if 'Assigned Roles' in comparison_df.columns:
            comparison_df['Assigned Roles'] = comparison_df['Assigned Roles'].apply(
                lambda roles: ', '.join(roles) if isinstance(roles, list) else roles
            )

        # Set index, transpose, and display the corrected DataFrame
        st.dataframe(comparison_df.set_index('Name').T, use_container_width=True)

def dwrs_progress_page():
    st.title("DWRS Progress Over Time")
    df = pd.DataFrame(get_all_players())
    if df.empty:
        st.info("No players available.")
        return
    names = st.multiselect("Select Players", options=sorted(df['Name'].unique()))
    role = st.selectbox("Select Role", ["All Roles"] + get_valid_roles(), format_func=format_role_display_with_all)
    if names:
        ids = df[df['Name'].isin(names)]['Unique ID'].tolist()
        history = get_dwrs_history(ids, role if role != "All Roles" else None)
        if not history.empty:
            history['dwrs_normalized'] = pd.to_numeric(history['dwrs_normalized'].str.rstrip('%'))
            history = history.merge(df[['Unique ID', 'Name']], left_on='unique_id', right_on='Unique ID')
            history['label'] = history['Name'] + (' - ' + history['role'] if role == "All Roles" else '')
            pivot = history.pivot_table(index='timestamp', columns='label', values='dwrs_normalized', aggfunc='mean')
            st.line_chart(pivot)
        else: st.info("No historical data available for selected players/role.")

def edit_player_data_page():
    st.title("Edit Player Data")
    user_club = get_user_club()
    df = pd.DataFrame(get_all_players())
    if df.empty:
        st.info("No players loaded.")
        return
    search = st.text_input("Search for a player by name")
    if search:
        results = df[df['Name'].str.contains(search, case=False, na=False)]
        if not results.empty:
            options = [f"{n} ({c})" for n, c in zip(results['Name'], results['Club'])]
            selected = st.selectbox("Select a player to edit", options=options)
            if selected:
                player = results.iloc[options.index(selected)].to_dict()
                st.write(f"### Editing: {player['Name']}")
                st.subheader("Update Club")
                new_club = st.text_input("Club Name", value=player['Club'])
                if st.button("Save Club Change"):
                    update_player_club(player['Unique ID'], new_club)
                    clear_all_caches()
                    st.success(f"Updated club to '{new_club}'.")
                    st.rerun()
                if player['Club'] == user_club:
                    st.subheader("Set Primary Role")
                    st.info("This role will be prioritized in the 'Best Position Calculator'.")
                    options = ["None"] + sorted(player.get('Assigned Roles', []))
                    current = player.get('primary_role')
                    index = options.index(current) if current and current in options else 0
                    new_role = st.selectbox("Primary Role", options, index, format_func=lambda x: "None" if x == "None" else format_role_display(x))
                    if st.button("Save Primary Role"):
                        set_primary_role(player['Unique ID'], new_role if new_role != "None" else None)
                        clear_all_caches()
                        st.success(f"Set primary role to {new_role}.")
                        st.rerun()
        else: st.warning("No players found.")

def create_new_role_page():
    st.title("Create a New Player Role")
    st.info("Define a new field player role. The short name is generated automatically as you type. After creation, the app will reload with the new role available everywhere.")

    # Attribute lists as you defined them
    TECHNICAL_ATTRS = ["Corners", "Crossing", "Dribbling", "Finishing", "First Touch", "Free Kick Taking", "Heading", "Long Shots", "Long Throws", "Marking", "Passing", "Penalty Taking", "Tackling", "Technique"]
    MENTAL_ATTRS = ["Aggression", "Anticipation", "Bravery", "Composure", "Concentration", "Decisions", "Determination", "Flair", "Leadership", "Off the Ball", "Positioning", "Teamwork", "Vision", "Work Rate"]
    PHYSICAL_ATTRS = ["Acceleration", "Agility", "Balance", "Jumping Reach", "Natural Fitness", "Pace", "Stamina", "Strength"]
    ALL_ATTRIBUTES = TECHNICAL_ATTRS + MENTAL_ATTRS + PHYSICAL_ATTRS

    # --- PART 1: Inputs that need live feedback (moved outside the form) ---
    st.subheader("1. Basic Role Information")
    
    c1, c2, c3 = st.columns(3)
    with c1:
        role_name = st.text_input("Full Role Name (e.g., 'Advanced Playmaker')", key="new_role_name", help="The descriptive name of the role.")
    with c2:
        role_cat = st.selectbox("Role Category", ["Defense", "Midfield", "Attack"], key="new_role_cat")
    with c3:
        role_duty = st.selectbox("Role Duty", ["Defend", "Support", "Attack", "Automatic", "Cover", "Stopper"], key="new_role_duty")

    # Auto-generate and display short name dynamically
    if role_name:
        short_name_suggestion = "".join([word[0] for word in role_name.split()]).upper()
        duty_suffix = role_duty[0] if role_duty not in ["Cover", "Stopper"] else role_duty[:2]
        final_short_name = f"{short_name_suggestion}-{duty_suffix}"
        st.write(f"**Generated Short Name:** `{final_short_name}` (This will be the unique ID)")
    else:
        final_short_name = ""
        st.write("**Generated Short Name:** `...`")


    with st.form("new_role_form"):
        st.subheader("2. Position Mapping")
        definitions = get_definitions()
        possible_positions = sorted([p for p in definitions['position_to_role_mapping'].keys() if p != "GK"])
        assigned_positions = st.multiselect("Assign this role to positions:", options=possible_positions, help="Select one or more positions where this role can be used.", key="new_role_positions")

        st.subheader("3. Key and Preferable Attributes")
        st.warning("A 'Key' attribute gets the highest multiplier. A 'Preferable' attribute gets a medium multiplier. If both are checked, 'Key' will be prioritized.")

        tc, mc, pc = st.columns(3)
        with tc:
            st.markdown("#### Technical")
            for attr in TECHNICAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        with mc:
            st.markdown("#### Mental")
            for attr in MENTAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        with pc:
            st.markdown("#### Physical")
            for attr in PHYSICAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        
        submitted = st.form_submit_button("Create New Role", type="primary")

    if submitted:
        with st.spinner("Validating and saving new role..."):
            if not role_name or not assigned_positions:
                st.error("Validation Failed: Please provide a Full Role Name and at least one Position Mapping.")
                return
            
            if final_short_name in definitions['role_specific_weights']:
                st.error(f"Validation Failed: The short name '{final_short_name}' already exists. Please choose a different role name or duty.")
                return

            key_attrs, pref_attrs = [], []
            for attr in ALL_ATTRIBUTES:
                if st.session_state.get(f"key_{attr}"):
                    key_attrs.append(attr)
                elif st.session_state.get(f"pref_{attr}"):
                    pref_attrs.append(attr)

            full_role_display_name = f"{role_name} ({role_duty})"
            
            definitions['player_roles'][role_cat][final_short_name] = full_role_display_name
            definitions['role_specific_weights'][final_short_name] = {"key": key_attrs, "preferable": pref_attrs}
            for pos in assigned_positions:
                if pos in definitions['position_to_role_mapping']:
                    definitions['position_to_role_mapping'][pos].append(final_short_name)
                    definitions['position_to_role_mapping'][pos].sort()

            success, message = save_definitions(definitions)

            if success:
                st.success(f"Role '{full_role_display_name}' created successfully! Reloading application...")
                
                # --- FIX: Clear all session state keys to reset the form completely ---
                keys_to_delete = [k for k in st.session_state.keys() if k.startswith('new_role_') or k.startswith('key_') or k.startswith('pref_')]
                for key in keys_to_delete:
                    del st.session_state[key]
                
                clear_all_caches()
                st.rerun()
            else:
                st.error(f"Failed to save role: {message}")

def settings_page():
    st.title("Settings")
    st.subheader("Global Stat Weights")
    new_weights = {cat: st.number_input(f"{cat} Weight", 0.0, 10.0, get_weight(cat.lower().replace(" ", "_"), val), 0.1) for cat, val in { "Extremely Important": 8.0, "Important": 4.0, "Good": 2.0, "Decent": 1.0, "Almost Irrelevant": 0.2 }.items()}
    st.subheader("Goalkeeper Stat Weights")
    new_gk_weights = {cat: st.number_input(f"{cat} Weight", 0.0, 10.0, get_weight("gk_" + cat.lower().replace(" ", "_"), val), 0.1, key=f"gk_{cat}") for cat, val in { "Top Importance": 10.0, "High Importance": 8.0, "Medium Importance": 6.0, "Key": 4.0, "Preferable": 2.0, "Other": 0.5 }.items()}
    st.subheader("Role Multipliers")
    key_mult = st.number_input("Key Attributes Multiplier", 1.0, 20.0, get_role_multiplier('key'), 0.1)
    pref_mult = st.number_input("Preferable Attributes Multiplier", 1.0, 20.0, get_role_multiplier('preferable'), 0.1)
    st.subheader("Database Settings")
    db_name = st.text_input("Database Name (no .db)", value=get_db_name())
    if st.button("Save All Settings"):
        for cat, val in new_weights.items(): set_weight(cat.lower().replace(" ", "_"), val)
        for cat, val in new_gk_weights.items(): set_weight("gk_" + cat.lower().replace(" ", "_"), val)
        set_role_multiplier('key', key_mult)
        set_role_multiplier('preferable', pref_mult)
        set_db_name(db_name)
        clear_all_caches()
        df = load_data()
        if df is not None: update_dwrs_ratings(df, get_valid_roles())
        st.success("Settings updated successfully!")
        st.rerun()

# --- FIXED: Main function with clear if/elif structure ---
def main():
    page, uploaded_file = sidebar()
    
    # Use st.query_params to allow linking to specific pages
    query_params = st.query_params
    if "page" in query_params:
        page = query_params["page"][0]
    
    if page == "All Players":
        main_page(uploaded_file)
    elif page == "Assign Roles":
        assign_roles_page()
    elif page == "Role Analysis":
        role_analysis_page()
    elif page == "Player-Role Matrix":
        player_role_matrix_page()
    elif page == "Best Position Calculator":
        best_position_calculator_page()
    elif page == "Player Comparison":
        player_comparison_page()
    elif page == "DWRS Progress":
        dwrs_progress_page()
    elif page == "Edit Player Data":
        edit_player_data_page()
    elif page == "Create New Role":
        create_new_role_page()
    elif page == "Settings":
        settings_page()
    else:
        main_page(uploaded_file) # Default page

if __name__ == "__main__":
    main()