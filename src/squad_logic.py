# squad_logic.py

import streamlit as st
import pandas as pd
import copy

from config_handler import get_age_threshold, get_apt_weight, get_selection_bonus, get_squad_management_setting
from constants import get_position_to_role_mapping, TACTICAL_SLOT_TO_GAME_POSITIONS
from utils import parse_position_string, format_role_display
from constants import get_valid_roles, get_tactic_roles
from data_parser import get_players_by_role
from sqlite_db import get_all_players, get_latest_dwrs_ratings

def get_last_name(full_name):
    """Extracts the last name from a full name string."""
    if isinstance(full_name, str) and full_name:
        return full_name.split(' ')[-1]
    return ""

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

st.cache_data
def get_master_role_ratings(user_club, second_team_club=None):
    """
    Calculates and caches a master dictionary of NUMERIC DWRS ratings for all players
    in the user's club and second team across all valid roles.
    This new version reads from pre-calculated data instead of recalculating.
    """

    all_ratings_data = get_latest_dwrs_ratings()
    master_ratings_numeric = {}

    for role, player_ratings in all_ratings_data.items():
        if role not in master_ratings_numeric:
            master_ratings_numeric[role] = {}
            
        # --- START OF CORRECTION ---
        # The value from the dictionary is now a tuple: (absolute_val, normalized_str)
        # We need to unpack it correctly.
        for player_id, rating_tuple in player_ratings.items():
            try:
                # We only need the second item from the tuple (the normalized string) for this function.
                _absolute_val, normalized_str = rating_tuple
                
                # Convert '85%' to 85.0 for the squad building algorithm
                master_ratings_numeric[role][player_id] = float(normalized_str.rstrip('%'))
            except (ValueError, TypeError, AttributeError):
                # This handles cases of bad data or if the tuple isn't structured as expected.
                master_ratings_numeric[role][player_id] = 0.0
        # --- END OF CORRECTION ---
                
    return master_ratings_numeric

def _find_next_best_player(role, player_pool, master_role_ratings, excluded_ids):
    """Finds the best available player for a given role from a pool."""
    best_player = None
    max_rating = -1
    for player in player_pool:
        if player['Unique ID'] in excluded_ids:
            continue
        rating = master_role_ratings.get(role, {}).get(player['Unique ID'], 0)
        if rating > max_rating:
            max_rating = rating
            best_player = player
    return best_player, max_rating

def _handle_injury_promotions(ideal_xi, ideal_b_team, ideal_depth, remaining_pool, all_players, positions, master_role_ratings):
    """
    Takes ideal squads, adjusts for injuries, and adds a 'promoted_due_to_injury'
    flag to players who have been moved up.
    """
    player_map = {p['Unique ID']: p for p in all_players}
    
    adjusted_xi = copy.deepcopy(ideal_xi)
    adjusted_b_team = copy.deepcopy(ideal_b_team)
    adjusted_depth = copy.deepcopy(ideal_depth)
    
    injury_log = []
    
    for pos, xi_player_data in ideal_xi.items():
        player_id = xi_player_data.get('player_id')
        if not player_id: continue
            
        player_obj = player_map.get(player_id)
        if player_obj and 'inj' in player_obj.get('Information', '').lower():
            
            b_team_player_for_pos = ideal_b_team.get(pos)
            log_entry = f"**{pos}:** {xi_player_data['name']} is injured."
            
            if b_team_player_for_pos and b_team_player_for_pos.get('player_id'):
                # Promote the B-Team player and ADD THE FLAG
                promoted_xi_player = copy.deepcopy(b_team_player_for_pos)
                promoted_xi_player['promoted_due_to_injury'] = True # <-- THE NEW FLAG
                adjusted_xi[pos] = promoted_xi_player
                log_entry += f" {b_team_player_for_pos['name']} promoted from B-Team."
                
                role = positions.get(pos)
                depth_player_for_role_list = ideal_depth.get(role)
                
                if depth_player_for_role_list:
                    depth_player = depth_player_for_role_list[0]
                    # Promote the Depth player and ADD THE FLAG
                    b_team_replacement = {
                        "player_id": next((p['Unique ID'] for p in all_players if p['Name'] == depth_player['name']), None),
                        "name": depth_player['name'], "rating": depth_player['rating'], "apt": depth_player.get('apt', ''),
                        "promoted_due_to_injury": True # <-- THE NEW FLAG
                    }
                    adjusted_b_team[pos] = b_team_replacement
                    log_entry += f" {depth_player['name']} promoted from Depth."
                    adjusted_depth[role] = []
                else:
                    adjusted_b_team[pos] = {"name": "-", "rating": "0%", "apt": ""}
                    log_entry += " No depth player available."
            else:
                adjusted_xi[pos] = {"name": "-", "rating": "0%", "apt": ""}
                log_entry += " No B-Team player available to promote."

            injury_log.append(log_entry)

    vacant_depth_roles = [role for role, players in adjusted_depth.items() if not players]
    if vacant_depth_roles:
        current_squad_ids = {p_data['player_id'] for squad in [adjusted_xi, adjusted_b_team] for p_data in squad.values() if 'player_id' in p_data}
        for role in vacant_depth_roles:
            new_depth_player, rating = _find_next_best_player(role, remaining_pool, master_role_ratings, current_squad_ids)
            if new_depth_player:
                adjusted_depth[role] = [{"name": new_depth_player['Name'], "rating": f"{int(rating)}%", "apt": new_depth_player.get("Agreed Playing Time", "") or "", "age": new_depth_player.get("Age", "N/A")}]
                injury_log.append(f"**Depth:** New player {new_depth_player['Name']} called up to cover {format_role_display(role)}.")

    return adjusted_xi, adjusted_b_team, adjusted_depth, injury_log

@st.cache_data
def get_cached_squad_analysis(players, tactic, user_club, second_team_club):
    """
    A single, cached function to perform all squad calculations, including injury adjustments.
    """
    if not players or not tactic or not user_club:
        return {}

    my_club_players = [p for p in players if p.get('Club') == user_club]
    second_team_players = [p for p in players if p.get('Club') == second_team_club] if second_team_club else []

    if not my_club_players:
        return {}

    master_role_ratings = get_master_role_ratings(user_club, second_team_club)
    positions = get_tactic_roles().get(tactic, {})
    if not positions:
        return {}

    # Step 1: Calculate the IDEAL squads as before.
    ideal_squad_data = calculate_squad_and_surplus(my_club_players, positions, master_role_ratings)
    
    # Step 2: Apply the new injury adjustment layer.
    adjusted_xi, adjusted_b_team, adjusted_depth, injury_log = _handle_injury_promotions(
        ideal_squad_data["starting_xi"],
        ideal_squad_data["b_team"],
        ideal_squad_data["best_depth_options"],
        ideal_squad_data["depth_pool"],
        my_club_players,
        positions,
        master_role_ratings
    )
    
    # Step 3: Package the ADJUSTED data for the UI.
    first_team_squad_data = {
        "starting_xi": adjusted_xi,
        "b_team": adjusted_b_team,
        "best_depth_options": adjusted_depth,
        "depth_pool": ideal_squad_data["depth_pool"],
        "core_squad_ids": ideal_squad_data["core_squad_ids"]
    }

    # The dev squad calculation remains the same.
    dev_squad_data = calculate_development_squads(
        second_team_players, 
        first_team_squad_data["depth_pool"], 
        positions,           
        master_role_ratings  
    )

    core_squad_df = pd.DataFrame([p for p in my_club_players if p['Unique ID'] in first_team_squad_data["core_squad_ids"]])

    # Return everything the UI needs.
    return {
        "master_role_ratings": master_role_ratings,
        "first_team_squad_data": first_team_squad_data,
        "dev_squad_data": dev_squad_data,
        "core_squad_df": core_squad_df,
        "my_club_players": my_club_players,
        "second_team_players": second_team_players,
        "injury_log": injury_log # Pass the log to the UI
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
        # Get the unique set of roles needed for this tactic
        unique_roles_needed = sorted(list(set(positions.values())))

        # Iterate through the UNIQUE ROLES, not every single position
        for role in unique_roles_needed:
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
                    # The KEY is now the ROLE
                    best_depth_options[role] = [{
                        "name": best_pick['Name'],
                        "rating": f"{int(rating)}%",
                        "apt": best_pick.get("Agreed Playing Time", "") or "",
                        "age": best_pick.get("Age", "N/A")
                    }]
                    depth_player_ids.add(best_pick['Unique ID'])
                    
                    if best_pick['Unique ID'] not in player_unique_roles_covered:
                        player_unique_roles_covered[best_pick['Unique ID']] = set()
                    player_unique_roles_covered[best_pick['Unique ID']].add(role)
    
    # --- 4. Finalize and Return ---
    core_squad_ids = players_xi_or_b_team.union(depth_player_ids)
    
    return {
        "starting_xi": starting_xi, 
        "b_team": b_team, 
        "best_depth_options": best_depth_options,
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