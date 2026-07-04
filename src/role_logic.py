# role_logic.py

import pandas as pd

from sqlite_db import get_all_players, update_player_roles, update_dwrs_ratings
from utils import parse_position_string
from constants import get_position_to_role_mapping, get_valid_roles
from ui_components import clear_all_caches

def auto_assign_roles_to_unassigned():
    """
    Finds all players with no assigned roles, assigns them default roles based on
    their positions, and immediately recalculates their DWRS.
    Returns the number of players updated.
    """

    all_players_df = pd.DataFrame(get_all_players())
    if all_players_df.empty:
        return 0

    unassigned_df = all_players_df[all_players_df['Assigned Roles'].apply(lambda x: not isinstance(x, list) or not x)]

    if unassigned_df.empty:
        return 0

    changes = {}
    pos_map = get_position_to_role_mapping()
    # Position strings repeat heavily across a big scouting database; parse
    # each distinct string only once instead of per player (iterrows was the
    # other slow part here).
    roles_for_position_str = {}
    for uid, pos_str in zip(unassigned_df['Unique ID'], unassigned_df['Position']):
        if pos_str not in roles_for_position_str:
            player_positions = parse_position_string(pos_str)
            roles_for_position_str[pos_str] = sorted(
                set(r for pos in player_positions for r in pos_map.get(pos, []))
            )
        roles = roles_for_position_str[pos_str]
        if roles:
            changes[uid] = roles
    
    if changes:
        # Update the roles in the database
        update_player_roles(changes)
        
        # Now, recalculate DWRS *only* for the affected players
        affected_players_df = all_players_df[all_players_df['Unique ID'].isin(changes.keys())].copy()
        
        # We need to manually add the new roles to the dataframe for the calculation
        def get_new_roles(row):
            return changes.get(row['Unique ID'], row['Assigned Roles'])
        affected_players_df['Assigned Roles'] = affected_players_df.apply(get_new_roles, axis=1)

        update_dwrs_ratings(affected_players_df, get_valid_roles())

        clear_all_caches()
        return len(changes)
    
    return 0