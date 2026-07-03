# tactic_explorer_logic.py
#
# Business logic for the "Tactic Explorer": evaluate a squad across ALL tactics
# to find which formations fit best. UI-free so it can be tested independently.
#
# For each tactic it reports:
#   * Coverage  – how many of the XI slots the best XI can actually fill, which
#     slots are left empty, which have no eligible player at all (structural
#     gaps, e.g. "no right winger"), and which have only a single option.
#   * Strength  – median / mean normalized DWRS of the best XI (computed over
#     FILLED slots only, so an empty slot never fakes a low score), plus the
#     same per stratum (Defense / DM / Midfield / AM / Strikers, GK separate).
#
# Ranking is coverage-first (more fillable slots), then strength (median) — the
# right priority when taking over an unknown/amateur squad.

import statistics

from constants import TACTICAL_SLOT_TO_GAME_POSITIONS, get_tactic_roles, get_tactic_layouts
from utils import parse_position_string
from squad_logic import calculate_squad_and_surplus

# Stratum order from back to front; GK is its own line.
STRATUM_ORDER = ["Goalkeeper", "Defense", "Defensive Midfield",
                 "Midfield", "Attacking Midfield", "Strikers"]


def _parse_pct(value):
    """Convert a stored rating ('85%' / 85 / 85.0) to float; None on failure."""
    if value is None:
        return None
    try:
        return float(str(value).rstrip('%'))
    except (ValueError, TypeError):
        return None


def _slot_to_stratum(slot, layout):
    """Which stratum a tactical slot belongs to (GK handled explicitly)."""
    if slot == 'GK':
        return 'Goalkeeper'
    for stratum, slots in layout.items():
        if slot in slots:
            return stratum
    return 'Other'


def _eligible_count_for_slot(slot, role, pool, master_ratings):
    """Number of pool players who could play this slot in isolation: their
    listed position matches the slot AND they have a positive DWRS for the
    slot's role. This is the depth/coverage signal (independent of who the
    best-XI algorithm actually assigns)."""
    allowed = TACTICAL_SLOT_TO_GAME_POSITIONS.get(slot, [])
    role_ratings = master_ratings.get(role, {})
    count = 0
    for p in pool:
        player_positions = parse_position_string(p.get('Position', ''))
        if not any(gp in allowed for gp in player_positions):
            continue
        if role_ratings.get(p.get('Unique ID'), 0) > 0:
            count += 1
    return count


def analyze_tactic(pool, tactic, positions, layout, master_ratings, apply_apt_weight=True):
    """Analyze a single tactic for the given player pool. Returns a metrics dict."""
    squad = calculate_squad_and_surplus(pool, positions, master_ratings, apply_apt_weight=apply_apt_weight)
    xi = squad.get("starting_xi", {})

    total_slots = len(positions)
    filled = 0
    empty_slots = []          # left empty by the best XI
    ratings_overall = []
    per_stratum = {}          # stratum -> [ratings]

    for slot, role in positions.items():
        stratum = _slot_to_stratum(slot, layout)
        cell = xi.get(slot, {})
        rating = _parse_pct(cell.get('rating')) if cell.get('player_id') else None
        if cell.get('player_id') and rating and rating > 0:
            filled += 1
            ratings_overall.append(rating)
            per_stratum.setdefault(stratum, []).append(rating)
        else:
            empty_slots.append(slot)

    # Depth / coverage in isolation
    eligible_counts = {
        slot: _eligible_count_for_slot(slot, role, pool, master_ratings)
        for slot, role in positions.items()
    }
    uncoverable_slots = [s for s, c in eligible_counts.items() if c == 0]
    thin_slots = [s for s, c in eligible_counts.items() if c == 1]
    avg_depth = statistics.mean(eligible_counts.values()) if eligible_counts else 0.0

    def med(xs):
        return round(statistics.median(xs), 1) if xs else None

    def avg(xs):
        return round(statistics.mean(xs), 1) if xs else None

    stratum_stats = {
        stratum: {"median": med(xs), "mean": avg(xs), "n": len(xs)}
        for stratum, xs in per_stratum.items()
    }

    return {
        "tactic": tactic,
        "total_slots": total_slots,
        "filled_slots": filled,
        "empty_slots": empty_slots,
        "uncoverable_slots": uncoverable_slots,
        "thin_slots": thin_slots,
        "avg_depth": round(avg_depth, 2),
        "overall_median": med(ratings_overall),
        "overall_mean": avg(ratings_overall),
        "per_stratum": stratum_stats,
        "eligible_counts": eligible_counts,
        "positions": positions,
    }


def analyze_all_tactics(pool, master_ratings, tactics=None, apply_apt_weight=True):
    """Analyze the pool across all (or a chosen subset of) tactics.

    apply_apt_weight=False is used for national squads, where a player's club
    Agreed Playing Time must not influence selection.
    Returns a list of metrics dicts sorted coverage-first, then by median DWRS.
    """
    tactic_roles = get_tactic_roles()
    layouts = get_tactic_layouts()
    names = tactics if tactics is not None else list(tactic_roles.keys())

    results = []
    for t in names:
        positions = tactic_roles.get(t, {})
        if not positions:
            continue
        layout = layouts.get(t, {})
        results.append(analyze_tactic(pool, t, positions, layout, master_ratings, apply_apt_weight=apply_apt_weight))

    # Coverage first (more filled slots better), then strength (higher median).
    results.sort(
        key=lambda r: (r["filled_slots"], r["overall_median"] or 0.0),
        reverse=True,
    )
    return results