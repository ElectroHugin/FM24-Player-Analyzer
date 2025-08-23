# app.py

from config_handler import get_age_threshold, get_apt_weight

def get_last_name(full_name):
    """Extracts the last name from a full name string."""
    if isinstance(full_name, str) and full_name:
        return full_name.split(' ')[-1]
    return ""

def calculate_squad_and_surplus(my_club_players, positions, master_role_ratings):
    """
    A single, reliable function to calculate the Starting XI, B-Team, Depth, and Surplus players.
    This version includes special logic for goalkeepers and is corrected to prevent TypeErrors.
    """
    def select_team(team_positions, available_players):
        team = {}
        players_team = set()
        while len(team) < len(team_positions):
            best_candidate_for_each_pos = {}
            empty_positions = [p for p in team_positions if p not in team]
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
                        selection_score = rating * apt_weight
                        if selection_score > max_score:
                            max_score = selection_score
                            best_candidate = { "player_id": player['Unique ID'], "player_name": player['Name'], "player_apt": apt, "rating": rating, "selection_score": selection_score, "position": pos }
                if best_candidate: best_candidate_for_each_pos[pos] = best_candidate
            if not best_candidate_for_each_pos: break 
            weakest_link_pos = min(best_candidate_for_each_pos, key=lambda p: best_candidate_for_each_pos[p]['selection_score'])
            winner = best_candidate_for_each_pos[weakest_link_pos]
            team[weakest_link_pos] = { "name": winner['player_name'], "rating": f"{int(winner['rating'])}%", "apt": winner['player_apt'] }
            players_team.add(winner['player_id'])
            available_players = [p for p in available_players if p['Unique ID'] != winner['player_id']]
        return team, available_players

    # --- Main Logic Execution ---
    starting_xi, remaining_players = select_team(positions, my_club_players)
    b_team, depth_pool = select_team(positions, remaining_players)
    
    default_player = {"name": "-", "rating": "0%", "apt": ""}
    for pos in positions:
        if pos not in starting_xi: starting_xi[pos] = default_player.copy()
        if pos not in b_team: b_team[pos] = default_player.copy()

    players_xi_or_b_team = {p['Unique ID'] for p in my_club_players if p not in depth_pool}
    
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
        "surplus_players": surplus_players, "youth_surplus": youth_surplus, "senior_surplus": senior_surplus
    }