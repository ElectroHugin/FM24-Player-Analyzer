# squad_logic.py

from config_handler import get_age_threshold, get_apt_weight, get_selection_bonus
from constants import get_position_to_role_mapping

def get_last_name(full_name):
    """Extracts the last name from a full name string."""
    if isinstance(full_name, str) and full_name:
        return full_name.split(' ')[-1]
    return ""

def _apply_footedness_swaps(team, all_players):
    """
    A helper function to swap players in symmetrical positions based on their preferred foot.
    This is applied AFTER the team has been selected.
    """
    # Define the symmetrical pairs of tactical positions
    symmetrical_pairs = [
        ('DCL', 'DCR'), ('DMCL', 'DMCR'),
        ('MCL', 'MCR'), ('AMCL', 'AMCR')
    ]
    
    # Create a quick lookup map for player objects by their ID
    player_map = {p['Unique ID']: p for p in all_players}

    for pos_L, pos_R in symmetrical_pairs:
        # Check if both symmetrical positions are actually in the team
        if pos_L in team and pos_R in team:
            player_L_data = team[pos_L]
            player_R_data = team[pos_R]
            
            # Get the full player objects using the IDs we stored
            player_L = player_map.get(player_L_data.get('player_id'))
            player_R = player_map.get(player_R_data.get('player_id'))

            if not player_L or not player_R:
                continue

            # The ideal swap condition: a right-footer is on the left and a left-footer is on the right
            if player_L.get('Preferred Foot') == 'Right' and player_R.get('Preferred Foot') == 'Left':
                # Swap them
                team[pos_L], team[pos_R] = team[pos_R], team[pos_L]
                
    return team

def calculate_squad_and_surplus(my_club_players, positions, master_role_ratings):
    """
    A single, reliable function to calculate the Starting XI, B-Team, Depth, and Surplus players.
    This version includes the Natural Position Bonus during selection and a post-selection
    Footedness Swap for tactical optimization.
    """
    
    # --- START: NEW PRE-CALCULATION STEP for Natural Position logic ---
    pos_to_role_map = get_position_to_role_mapping()
    role_to_game_positions = {}
    for game_pos, roles in pos_to_role_map.items():
        for r in roles:
            if r not in role_to_game_positions:
                role_to_game_positions[r] = []
            role_to_game_positions[r].append(game_pos)
    # --- END: NEW PRE-CALCULATION STEP ---

    def select_team(team_positions, available_players):
        team_with_ids = {}
        players_team = set()

        # Get the bonus multiplier from config.ini
        natural_pos_multiplier = get_selection_bonus('natural_position')

        while len(team_with_ids) < len(team_positions):
            best_candidate_for_each_pos = {}
            empty_positions = [p for p in team_positions if p not in team_with_ids]
            for pos in empty_positions:
                role = team_positions[pos]
                best_candidate = None
                max_score = -1
                for player in available_players:
                    if player['Unique ID'] in players_team: continue
                    primary_role = player.get('primary_role')
                    if primary_role and primary_role != role: continue
                    
                    rating = master_role_ratings.get(role, {}).get(player['Unique ID'], 0)
                    if rating > 0:
                        apt = player.get("Agreed Playing Time", "None") or "None"
                        apt_weight = get_apt_weight(apt)
                        
                        # --- START: REVISED Natural Position Bonus Logic ---
                        natural_pos_bonus = 1.0
                        valid_game_positions_for_role = role_to_game_positions.get(role, [])
                        player_natural_positions = player.get('natural_positions', [])
                        if any(pnp in valid_game_positions_for_role for pnp in player_natural_positions):
                            natural_pos_bonus = natural_pos_multiplier
                        # --- END: REVISED Natural Position Bonus Logic ---
                        
                        selection_score = rating * apt_weight * natural_pos_bonus
                        
                        if selection_score > max_score:
                            max_score = selection_score
                            best_candidate = { 
                                "player_id": player['Unique ID'], 
                                "player_name": player['Name'], 
                                "player_apt": apt, 
                                "rating": rating, 
                                "selection_score": selection_score, 
                                "position": pos 
                            }
                if best_candidate: best_candidate_for_each_pos[pos] = best_candidate
            
            if not best_candidate_for_each_pos: break 
            
            weakest_link_pos = min(best_candidate_for_each_pos, key=lambda p: best_candidate_for_each_pos[p]['selection_score'])
            winner = best_candidate_for_each_pos[weakest_link_pos]
            
            # Store the full winner object, including the player_id
            team_with_ids[weakest_link_pos] = {
                "player_id": winner['player_id'],
                "name": winner['player_name'], 
                "rating": f"{int(winner['rating'])}%", 
                "apt": winner['player_apt']
            }
            players_team.add(winner['player_id'])
            available_players = [p for p in available_players if p['Unique ID'] != winner['player_id']]
            
        return team_with_ids, available_players

    # --- Main Logic Execution ---
    
    # 1. Select the initial teams based on score
    starting_xi_with_ids, remaining_players = select_team(positions, my_club_players)
    b_team_with_ids, depth_pool = select_team(positions, remaining_players)
    
    # 2. Apply the post-selection footedness swap logic
    starting_xi = _apply_footedness_swaps(starting_xi_with_ids, my_club_players)
    b_team = _apply_footedness_swaps(b_team_with_ids, my_club_players)

    # 3. Fill any empty slots with default players for display
    default_player = {"name": "-", "rating": "0%", "apt": ""}
    for pos in positions:
        if pos not in starting_xi: starting_xi[pos] = default_player.copy()
        if pos not in b_team: b_team[pos] = default_player.copy()

    # (The rest of the function remains identical to your original version)
    
    # --- This part is now based on the players selected in the two teams ---
    xi_ids = {p['player_id'] for p in starting_xi.values() if 'player_id' in p}
    b_team_ids = {p['player_id'] for p in b_team.values() if 'player_id' in p}
    players_xi_or_b_team = xi_ids.union(b_team_ids)
    
    depth_pool = [p for p in my_club_players if p['Unique ID'] not in players_xi_or_b_team]
    
    best_depth_options = {}
    depth_player_ids = set()
    if depth_pool:
        for pos, role in positions.items():
            if pos == 'GK':
                gks_depth = [p for p in depth_pool if 'GK' in p.get('Position', '')]
                sorted_gks = sorted(gks_depth, key=lambda p: master_role_ratings.get(role, {}).get(p['Unique ID'], -1), reverse=True)
                top_depth_gks = sorted_gks[:2] 
                if top_depth_gks:
                    best_depth_options[pos] = []
                    for gk in top_depth_gks:
                        rating = master_role_ratings.get(role, {}).get(gk['Unique ID'], 0)
                        if rating > 0:
                            best_depth_options[pos].append({ "name": gk['Name'], "rating": f"{int(rating)}%", "apt": gk.get("Agreed Playing Time", "") or "" })
                            depth_player_ids.add(gk['Unique ID'])
            else:
                outfielders_depth = [p for p in depth_pool if 'GK' not in p.get('Position', '')]
                if outfielders_depth:
                    best_candidate = max(outfielders_depth, key=lambda p: master_role_ratings.get(role, {}).get(p['Unique ID'], -1), default=None)
                    if best_candidate:
                        rating = master_role_ratings.get(role, {}).get(best_candidate['Unique ID'], 0)
                        if rating > 0:
                            best_depth_options[pos] = [{ "name": best_candidate['Name'], "rating": f"{int(rating)}%", "apt": best_candidate.get("Agreed Playing Time", "") or "" }]
                            depth_player_ids.add(best_candidate['Unique ID'])

    players_with_a_role_ids = players_xi_or_b_team.union(depth_player_ids)
    surplus_players = [p for p in my_club_players if p['Unique ID'] not in players_with_a_role_ids]
    
    youth_surplus = []
    senior_surplus = []

    outfielder_age_limit = get_age_threshold('outfielder')
    goalkeeper_age_limit = get_age_threshold('goalkeeper')

    for player in surplus_players:
        age_str = player.get('Age')
        age = int(age_str) if age_str and age_str.isdigit() else 99
        is_gk = 'GK' in player.get('Position', '')
        if is_gk:
            if age <= goalkeeper_age_limit: youth_surplus.append(player)
            else: senior_surplus.append(player)
        else:
            if age <= outfielder_age_limit: youth_surplus.append(player)
            else: senior_surplus.append(player)

    youth_surplus.sort(key=lambda p: get_last_name(p['Name']))
    senior_surplus.sort(key=lambda p: get_last_name(p['Name']))
    
    return {
        "starting_xi": starting_xi, "b_team": b_team, "best_depth_options": best_depth_options,
        "surplus_players": surplus_players, "youth_surplus": youth_surplus, "senior_surplus": senior_surplus,
        "depth_pool": depth_pool
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