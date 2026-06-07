# gap_analysis_logic.py
#
# Business logic for the "Squad Gap Analysis" tool. UI-free so it can be
# reused or tested independently.
#
# It identifies two kinds of weaknesses in a calculated Best XI / B-Team:
#   * Displacement (hidden gap): a good player is played below his best
#     tactic-relevant slot, or on the wrong side. The slot he was pulled
#     FROM is the real gap.
#   * Drop-off (obvious gap): a slot is filled by a player well below the
#     team's median DWRS.
#
# All "best position" reasoning is restricted to slots that exist in the
# CURRENT tactic, so a player is never measured against a role the formation
# doesn't field.

import statistics

from constants import TACTICAL_SLOT_TO_GAME_POSITIONS, get_position_to_role_mapping, get_valid_roles
from utils import parse_position_string


def slot_side(slot):
    """Return 'Left' / 'Right' / 'Center' for a tactical slot key.
    Uses the same L/R suffix convention as MASTER_POSITION_MAP (e.g. 'AMR'->Right)."""
    if slot.endswith('L'):
        return 'Left'
    if slot.endswith('R'):
        return 'Right'
    return 'Center'


def _player_can_play_slot(player, slot):
    """True if the player's game positions are valid for this tactical slot,
    mirroring the eligibility check used in squad_logic.select_team."""
    allowed = TACTICAL_SLOT_TO_GAME_POSITIONS.get(slot, [])
    player_positions = parse_position_string(player.get('Position', ''))
    return any(p in allowed for p in player_positions)


def _parse_pct(value):
    """Convert a stored rating ('85%' or 85 or 85.0) to a float; None on failure."""
    if value is None:
        return None
    try:
        return float(str(value).rstrip('%'))
    except (ValueError, TypeError):
        return None


def _effective_side(player):
    """A player's side preference for gap analysis.

    Only the MANUALLY set 'preferred_side' counts. We deliberately do NOT fall
    back to 'Preferred Foot': a strong right foot does not mean a player is out
    of position on the left. A side preference is only meaningful if the user
    explicitly set it in the player's edit screen.
    """
    side = player.get('preferred_side')
    return side if side in ('Left', 'Right') else None


def _best_tactic_slot_for_player(player, positions, master_ratings, current_slot=None):
    """
    Find the player's best slot among the slots that exist in THIS tactic
    and that he is eligible to play. Returns (best_slot, best_dwrs) or
    (None, None) if he fits nowhere in the formation.

    On a DWRS tie, the player's CURRENT slot is preferred. This matters because
    side-mirrored slots (e.g. DCL/DCR, AML/AMR) usually share the same role and
    therefore the same DWRS — without this, a player sitting on his best slot
    could be reported as "displaced" to an identical-rated mirror slot.
    """
    uid = player['Unique ID']
    best_slot, best_dwrs = None, None

    for slot, role in positions.items():
        if not _player_can_play_slot(player, slot):
            continue
        dwrs = master_ratings.get(role, {}).get(uid)
        dwrs = _parse_pct(dwrs)
        if dwrs is None:
            continue
        if best_dwrs is None or dwrs > best_dwrs:
            best_slot, best_dwrs = slot, dwrs
        elif dwrs == best_dwrs and slot == current_slot:
            # Tie: keep the player where he already is rather than a mirror slot.
            best_slot = slot

    return best_slot, best_dwrs


def analyze_team_gaps(team, positions, players_by_id, master_ratings,
                      displacement_threshold, dropoff_threshold, wrong_side_penalty):
    """
    Analyze a single team (starting XI or B-team) for gaps.

    Args:
        team: dict {slot: {"player_id","name","rating",...}} from squad_logic.
        positions: dict {slot: role} for the tactic.
        players_by_id: {uid: full player record} (needs Position and, optionally, preferred_side).
        master_ratings: {role: {uid: dwrs}} numeric or '%'-string.
        displacement_threshold, dropoff_threshold, wrong_side_penalty: floats.

    Returns a list of gap dicts, one per flagged slot, each:
        {
          "slot", "role", "player_name", "assigned_dwrs",
          "is_displacement", "is_dropoff",
          "displacement_score", "dropoff", "gap_score",
          "best_slot", "best_dwrs", "wrong_side", "reason"
        }
    sorted by gap_score descending.
    """
    # 1. Collect assigned DWRS for every filled slot to compute the median.
    assigned = {}  # slot -> (player record, role, assigned_dwrs)
    dwrs_values = []
    for slot, role in positions.items():
        cell = team.get(slot, {})
        uid = cell.get('player_id')
        if not uid:
            continue  # empty slot -> handled separately below
        dwrs = _parse_pct(cell.get('rating'))
        if dwrs is None:
            dwrs = _parse_pct(master_ratings.get(role, {}).get(uid))
        if dwrs is None:
            continue
        player = players_by_id.get(uid)
        if not player:
            continue
        assigned[slot] = (player, role, dwrs)
        dwrs_values.append(dwrs)

    median_dwrs = statistics.median(dwrs_values) if dwrs_values else 0.0

    gaps = []

    # 2. Per filled slot, compute displacement + drop-off.
    for slot, (player, role, assigned_dwrs) in assigned.items():
        # --- Displacement ---
        best_slot, best_dwrs = _best_tactic_slot_for_player(player, positions, master_ratings, current_slot=slot)
        dwrs_gap = (best_dwrs - assigned_dwrs) if best_dwrs is not None else 0.0
        if dwrs_gap < 0:
            dwrs_gap = 0.0

        # --- Wrong-side penalty ---
        # Only meaningful when the player has a MANUAL preferred_side, is on the
        # opposite side, AND actually has a slot on his preferred side available
        # in this tactic that he is eligible for. Otherwise the preference can't
        # be satisfied anyway, so it isn't a gap.
        wrong_side = False
        side = _effective_side(player)
        this_side = slot_side(slot)
        if side in ('Left', 'Right') and this_side in ('Left', 'Right') and side != this_side:
            preferred_side_available = any(
                slot_side(s) == side and _player_can_play_slot(player, s)
                for s in positions
            )
            if preferred_side_available:
                wrong_side = True
        side_penalty = wrong_side_penalty if wrong_side else 0.0

        displacement_score = dwrs_gap + side_penalty
        is_displacement = displacement_score >= displacement_threshold

        # --- Drop-off ---
        dropoff = median_dwrs - assigned_dwrs
        if dropoff < 0:
            dropoff = 0.0
        is_dropoff = dropoff >= dropoff_threshold

        if not (is_displacement or is_dropoff):
            continue

        gap_score = max(
            displacement_score if is_displacement else 0.0,
            dropoff if is_dropoff else 0.0,
        )

        # --- Human-readable reason ---
        reasons = []
        if is_displacement:
            if wrong_side and best_slot == slot:
                reasons.append(
                    f"{player['Name']} is {side}-sided but plays the {this_side} slot."
                )
            elif best_slot and best_slot != slot:
                reasons.append(
                    f"{player['Name']} is pulled here from {best_slot} "
                    f"({int(round(best_dwrs))}% vs {int(round(assigned_dwrs))}%) — "
                    f"the real gap is at {best_slot}."
                )
                if wrong_side:
                    reasons.append(f"Also on the wrong side ({side}-sided on a {this_side} slot).")
            elif wrong_side:
                reasons.append(
                    f"{player['Name']} is {side}-sided but plays the {this_side} slot."
                )
        if is_dropoff:
            reasons.append(
                f"{int(round(assigned_dwrs))}% is {int(round(dropoff))} below the team median "
                f"({int(round(median_dwrs))}%)."
            )

        gaps.append({
            "slot": slot,
            "role": role,
            "player_name": player.get('Name', '-'),
            "assigned_dwrs": assigned_dwrs,
            "is_displacement": is_displacement,
            "is_dropoff": is_dropoff,
            "displacement_score": displacement_score,
            "dropoff": dropoff,
            "gap_score": gap_score,
            "best_slot": best_slot,
            "best_dwrs": best_dwrs,
            "wrong_side": wrong_side,
            "reason": " ".join(reasons),
        })

    # 3. Empty slots are always the most severe gap.
    for slot, role in positions.items():
        if slot not in assigned:
            gaps.append({
                "slot": slot,
                "role": role,
                "player_name": "—",
                "assigned_dwrs": 0.0,
                "is_displacement": False,
                "is_dropoff": True,
                "displacement_score": 0.0,
                "dropoff": median_dwrs,
                "gap_score": max(median_dwrs, dropoff_threshold) + 100.0,  # force to top
                "best_slot": None,
                "best_dwrs": None,
                "wrong_side": False,
                "reason": "No eligible player for this slot — position is unfilled.",
            })

    gaps.sort(key=lambda g: g["gap_score"], reverse=True)
    return gaps