# squad_logic.py

import streamlit as st
import pandas as pd

from config_handler import get_age_threshold, get_apt_weight, get_selection_bonus, get_squad_management_setting
from constants import get_position_to_role_mapping, TACTICAL_SLOT_TO_GAME_POSITIONS
from utils import parse_position_string, format_role_display
from constants import get_valid_roles, get_tactic_roles
from data_parser import get_players_by_role
from sqlite_db import get_all_players

def get_last_name(full_name):
    """Extracts the last name from a full name string."""
    if isinstance(full_name, str) and full_name:
        return full_name.split(' ')[-1]
    return ""

# --- START: CORRECTED HELPER FUNCTION ---
def _apply_footedness_swaps(team, all_players, positions):
    """
    A helper function to swap players in symmetrical positions based on their preferred foot.
    This is applied AFTER the team has been selected.
    A swap only occurs if the roles for the two positions are identical.
    """
    # Define the symmetrical pairs of tactical positions
    symmetrical_pairs = [
        ('DCL', 'DCR'), ('DMCL', 'DMCR'),
        ('MCL', 'MCR'), ('AMCL', 'AMCR'),
        ('STL', 'STR')
    ]
    
    player_map = {p['Unique ID']: p for p in all_players}

    for pos_L, pos_R in symmetrical_pairs:
        # Check if both symmetrical positions are in the team
        if pos_L in team and pos_R in team:
            
            # --- NEW: Check if the roles for these positions are the same ---
            role_L = positions.get(pos_L)
            role_R = positions.get(pos_R)
            
            if role_L != role_R:
                continue # If roles are different, do not swap.
            # --- END NEW ---

            player_L_data = team[pos_L]
            player_R_data = team[pos_R]
            
            player_L = player_map.get(player_L_data.get('player_id'))
            player_R = player_map.get(player_R_data.get('player_id'))

            if not player_L or not player_R:
                continue

            # The ideal swap condition: a right-footer is on the left and a left-footer is on the right
            if player_L.get('Preferred Foot') == 'Right' and player_R.get('Preferred Foot') == 'Left':
                team[pos_L], team[pos_R] = team[pos_R], team[pos_L]
                
    return team

@st.cache_data
def get_cached_squad_analysis(players, tactic, user_club, second_team_club):
    """
    A single, cached function to perform all squad calculations.
    Returns a dictionary with all necessary dataframes and lists.
    """
    if not players or not tactic or not user_club:
        return {}

    my_club_players = [p for p in players if p.get('Club') == user_club]
    second_team_players = [p for p in players if p.get('Club') == second_team_club] if second_team_club else []

    if not my_club_players:
        return {}

    # --- This is the robust master rating calculation ---
    master_role_ratings = {}
    clubs_to_rate = [user_club]
    if second_team_club:
        clubs_to_rate.append(second_team_club)
    
    all_relevant_players = [p for p in get_all_players() if p.get('Club') in clubs_to_rate]
    if all_relevant_players:
        for role in get_valid_roles():
            ratings_df, _, _ = get_players_by_role(role, clubs_to_rate[0], clubs_to_rate[1] if len(clubs_to_rate) > 1 else None)
            if not ratings_df.empty:
                ratings_df['DWRS'] = pd.to_numeric(ratings_df['DWRS Rating (Normalized)'].str.rstrip('%'))
                valid_ratings = ratings_df[ratings_df['Club'].isin(clubs_to_rate)]
                master_role_ratings[role] = valid_ratings.set_index('Unique ID')['DWRS'].to_dict()
    # --- End of master rating calculation ---

    positions = get_tactic_roles().get(tactic, {})
    if not positions:
        return {}

    first_team_squad_data = calculate_squad_and_surplus(my_club_players, positions, master_role_ratings)
    
    dev_squad_data = calculate_development_squads(
        second_team_players, 
        first_team_squad_data["depth_pool"], 
        positions,           
        master_role_ratings  
    )

    final_core_squad_ids = first_team_squad_data.get("core_squad_ids", set())

    # Prepare and return a comprehensive dictionary of results
    final_core_squad_ids = first_team_squad_data.get("core_squad_ids", set())
    core_squad_df = pd.DataFrame([p for p in my_club_players if p['Unique ID'] in final_core_squad_ids])

    return {
        "master_role_ratings": master_role_ratings,
        "first_team_squad_data": first_team_squad_data,
        "dev_squad_data": dev_squad_data,
        "core_squad_df": core_squad_df,
        "my_club_players": my_club_players,
        "second_team_players": second_team_players
    }

def create_detailed_surplus_df(player_list, master_role_ratings):
    if not player_list:
        return pd.DataFrame()

    data = []
    for player in player_list:
        best_dwrs = 0
        best_role_abbr = ''
        # Find the best DWRS rating among the player's assigned roles
        assigned_roles = player.get('Assigned Roles', [])
        if assigned_roles:
            for role in assigned_roles:
                rating = master_role_ratings.get(role, {}).get(player['Unique ID'], 0)
                if rating > best_dwrs:
                    best_dwrs = rating
                    best_role_abbr = role
        
        player_data = {
            "Name": player['Name'],
            "Age": player.get('Age', 'N/A'),
            "Position": player.get('Position', 'N/A'),
            "Best Role": format_role_display(best_role_abbr) if best_role_abbr else "N/A",
            "Best DWRS": int(best_dwrs),
            "Det": player.get('Determination', 'N/A'),
            "Wor": player.get('Work Rate', 'N/A'),
            "Transfer": "✅" if player.get('transfer_status', 0) else "❌",
            "Loan": "✅" if player.get('loan_status', 0) else "❌",
        }
        data.append(player_data)
    
    return pd.DataFrame(data)

def calculate_squad_and_surplus(my_club_players, positions, master_role_ratings):
    """
    Calculates squads using a smart depth-filling algorithm that prioritizes
    full coverage while respecting a soft cap on a player's *unique* roles.
    """
    # --- 1. Get Manager's Preferences from Config ---
    MAX_DEPTH_ROLES = get_squad_management_setting('max_roles_per_depth_player')

    # --- 2. Select Starting XI and B-Team (Your 'weakest link' algorithm is preserved here) ---
    def select_team(team_positions, available_players):
        # This is your original, unchanged 'weakest link' function
        team_with_ids = {}
        players_team = set()
        natural_pos_multiplier = get_selection_bonus('natural_position')
        pos_to_role_map = get_position_to_role_mapping()
        role_to_game_positions = {r: [gp for gp, roles in pos_to_role_map.items() if r in roles] for r in get_valid_roles()}

        while len(team_with_ids) < len(team_positions):
            best_candidate_for_each_pos = {}
            empty_positions = [p for p in team_positions if p not in team_with_ids]
            for pos in empty_positions:
                role = team_positions[pos]
                best_candidate = None
                max_score = -1
                for player in available_players:
                    if player['Unique ID'] in players_team: continue
                    allowed_game_positions = TACTICAL_SLOT_TO_GAME_POSITIONS.get(pos, [])
                    player_game_positions = parse_position_string(player.get('Position', ''))
                    if not any(p_pos in allowed_game_positions for p_pos in player_game_positions): continue
                    primary_role = player.get('primary_role')
                    if primary_role and primary_role != role: continue
                    rating = master_role_ratings.get(role, {}).get(player['Unique ID'], 0)
                    if rating > 0:
                        apt = player.get("Agreed Playing Time", "None") or "None"
                        apt_weight = get_apt_weight(apt)
                        natural_pos_bonus = 1.0
                        valid_game_positions_for_role = role_to_game_positions.get(role, [])
                        player_natural_positions = player.get('natural_positions', [])
                        if any(pnp in valid_game_positions_for_role for pnp in player_natural_positions):
                            natural_pos_bonus = natural_pos_multiplier
                        selection_score = rating * apt_weight * natural_pos_bonus
                        if selection_score > max_score:
                            max_score = selection_score
                            best_candidate = {"player_id": player['Unique ID'], "player_name": player['Name'], "player_apt": apt, "rating": rating, "selection_score": selection_score, "position": pos}
                if best_candidate: best_candidate_for_each_pos[pos] = best_candidate
            if not best_candidate_for_each_pos: break
            weakest_link_pos = min(best_candidate_for_each_pos, key=lambda p: best_candidate_for_each_pos[p]['selection_score'])
            winner = best_candidate_for_each_pos[weakest_link_pos]
            team_with_ids[weakest_link_pos] = {"player_id": winner['player_id'], "name": winner['player_name'], "rating": f"{int(winner['rating'])}%", "apt": winner['player_apt']}
            players_team.add(winner['player_id'])
            available_players = [p for p in available_players if p['Unique ID'] != winner['player_id']]
        return team_with_ids, available_players

    xi_with_ids, remaining_for_b_team = select_team(positions, my_club_players)
    b_team_with_ids, depth_pool = select_team(positions, remaining_for_b_team)

    starting_xi = _apply_footedness_swaps(xi_with_ids, my_club_players, positions)
    b_team = _apply_footedness_swaps(b_team_with_ids, my_club_players, positions)

    default_player = {"name": "-", "rating": "0%", "apt": ""}
    for pos in positions:
        if pos not in starting_xi: starting_xi[pos] = default_player.copy()
        if pos not in b_team: b_team[pos] = default_player.copy()

    # --- 3. Final Smart Depth Calculation (Role-Based) ---
    xi_ids = {p['player_id'] for p in starting_xi.values() if 'player_id' in p}
    b_team_ids = {p['player_id'] for p in b_team.values() if 'player_id' in p}
    players_xi_or_b_team = xi_ids.union(b_team_ids)
    
    available_depth_players = [p for p in my_club_players if p['Unique ID'] not in players_xi_or_b_team]
    
    best_depth_options = {} # This will now be keyed by ROLE, not POSITION
    depth_player_ids = set()
    player_unique_roles_covered = {}

    if available_depth_players:
        # --- START OF THE FIX ---
        # Get the unique set of roles needed for this tactic
        unique_roles_needed = sorted(list(set(positions.values())))

        # Iterate through the UNIQUE ROLES, not every single position
        for role in unique_roles_needed:
        # --- END OF THE FIX ---
            sorted_candidates = sorted(
                available_depth_players,
                key=lambda p: master_role_ratings.get(role, {}).get(p['Unique ID'], -1),
                reverse=True
            )
            
            best_pick = None
            for candidate in sorted_candidates:
                candidate_id = candidate['Unique ID']
                roles_covered = player_unique_roles_covered.get(candidate_id, set())
                if role in roles_covered or len(roles_covered) < MAX_DEPTH_ROLES:
                    best_pick = candidate
                    break 
            
            if best_pick is None and sorted_candidates:
                best_pick = sorted_candidates[0]

            if best_pick:
                rating = master_role_ratings.get(role, {}).get(best_pick['Unique ID'], 0)
                if rating > 0:
                    # --- START OF THE FIX ---
                    # The KEY is now the ROLE
                    best_depth_options[role] = [{
                        "name": best_pick['Name'],
                        "rating": f"{int(rating)}%",
                        "apt": best_pick.get("Agreed Playing Time", "") or "",
                        "age": best_pick.get("Age", "N/A")
                    }]
                    # --- END OF THE FIX ---
                    depth_player_ids.add(best_pick['Unique ID'])
                    
                    if best_pick['Unique ID'] not in player_unique_roles_covered:
                        player_unique_roles_covered[best_pick['Unique ID']] = set()
                    player_unique_roles_covered[best_pick['Unique ID']].add(role)
    
    # --- 4. Finalize and Return ---
    core_squad_ids = players_xi_or_b_team.union(depth_player_ids)
    surplus_players = [p for p in my_club_players if p['Unique ID'] not in core_squad_ids]
    
    youth_surplus, senior_surplus = [], []
    outfielder_age_limit = get_age_threshold('outfielder')
    goalkeeper_age_limit = get_age_threshold('goalkeeper')
    for player in surplus_players:
        age_str = player.get('Age')
        age = int(age_str) if age_str and age_str.isdigit() else 99
        is_gk = 'GK' in player.get('Position', '')
        if (is_gk and age <= goalkeeper_age_limit) or (not is_gk and age <= outfielder_age_limit):
            youth_surplus.append(player)
        else:
            senior_surplus.append(player)
    
    youth_surplus.sort(key=lambda p: get_last_name(p['Name']))
    senior_surplus.sort(key=lambda p: get_last_name(p['Name']))
    
    return {
        "starting_xi": starting_xi, "b_team": b_team, "best_depth_options": best_depth_options,
        "surplus_players": surplus_players, "youth_surplus": youth_surplus, "senior_surplus": senior_surplus,
        "depth_pool": depth_pool,
        "core_squad_ids": core_squad_ids
    }

def calculate_development_squads(second_team_club_players, first_team_remnants, positions, master_role_ratings):
    """
    Calculates the Youth XI and Second Team XI using a robust process of elimination
    to ensure every surplus player is correctly categorized.
    """
    outfielder_age_limit = get_age_threshold('outfielder')
    goalkeeper_age_limit = get_age_threshold('goalkeeper')

    # --- 1. Calculate Second Team ---
    second_team_pool = second_team_club_players if second_team_club_players else first_team_remnants
    second_team_squad_data = calculate_squad_and_surplus(second_team_pool, positions, master_role_ratings)
    second_team_xi = second_team_squad_data["starting_xi"]

    second_team_xi_names = {player['name'] for player in second_team_xi.values() if player['name'] != '-'}
    second_team_player_ids = {p['Unique ID'] for p in second_team_pool if p['Name'] in second_team_xi_names}

    # --- 2. Calculate Youth Team ---
    youth_pool = []
    for player in first_team_remnants:
        if player['Unique ID'] in second_team_player_ids:
            continue
        age_str = player.get('Age')
        age = int(age_str) if age_str and age_str.isdigit() else 99
        is_gk = 'GK' in player.get('Position', '')
        if (is_gk and age <= goalkeeper_age_limit) or (not is_gk and age <= outfielder_age_limit):
            youth_pool.append(player)
    
    youth_squad_data = calculate_squad_and_surplus(youth_pool, positions, master_role_ratings)
    youth_xi = youth_squad_data["starting_xi"]

    youth_xi_names = {player['name'] for player in youth_xi.values() if player['name'] != '-'}
    youth_player_ids = {p['Unique ID'] for p in youth_pool if p['Name'] in youth_xi_names}

    # --- 3. Definitive Surplus Calculation (BUG FIX) ---
    # The final surplus is anyone in the remnant pool who is NOT in the Second XI and NOT in the Youth XI.
    final_surplus_players = []
    for player in first_team_remnants:
        if player['Unique ID'] not in second_team_player_ids and player['Unique ID'] not in youth_player_ids:
            final_surplus_players.append(player)

    # --- 4. Categorize the Final Surplus ---
    loan_candidates = []
    sell_candidates = []
    for player in final_surplus_players:
        age_str = player.get('Age')
        age = int(age_str) if age_str and age_str.isdigit() else 99
        is_gk = 'GK' in player.get('Position', '')
        is_young = (is_gk and age <= goalkeeper_age_limit) or (not is_gk and age <= outfielder_age_limit)

        if is_young:
            try:
                work_rate = int(player.get('Work Rate', 0) or 0)
                determination = int(player.get('Determination', 0) or 0)
                if (work_rate + determination) >= 20:
                    loan_candidates.append(player)
                else:
                    sell_candidates.append(player) # Young but not "promising" -> SELL
            except (ValueError, TypeError):
                sell_candidates.append(player)
        else:
            sell_candidates.append(player) # Too old -> SELL

    loan_candidates.sort(key=lambda p: get_last_name(p['Name']))
    sell_candidates.sort(key=lambda p: get_last_name(p['Name']))

    return {
        "youth_xi": youth_xi,
        "second_team_xi": second_team_xi,
        "loan_candidates": loan_candidates,
        "sell_candidates": sell_candidates
    }