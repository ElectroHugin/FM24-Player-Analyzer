#!/usr/bin/env python3
"""Golden-master exporter part 2: squad building, gap analysis and tactic
explorer results of the legacy app, dumped as JSON for the C++ golden gate.

Usage (from the legacy/ directory):
    python tools/export_golden_squad.py <db_name> <output_dir>

Writes <output_dir>/squad_<db_name>.json with, PER TACTIC:
    starting_xi / b_team:   {slot: uid or null}
    depth:                  {role: uid}
    youth_xi / second_xi:   {slot: uid or null}
    loan / sell:            [uid, ...] (ordered)
    gaps_xi / gaps_b:       [[slot, gap_score_rounded], ...] (ordered)
plus "explorer": [[tactic, filled_slots, overall_median], ...] (ranked).

Ratings come from the dwrs_ratings table (latest per player/role), exactly
like the app's get_master_role_ratings.
"""

import json
import os
import sqlite3
import sys

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
_LEGACY_DIR = os.path.dirname(_TOOLS_DIR)
sys.path.insert(0, os.path.join(_LEGACY_DIR, "src"))


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    db_name, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    # Point the app's config at this DB WITHOUT writing anything: monkeypatch
    # get_db_file before the sqlite_db module connects.
    import config_handler
    db_path = os.path.join(_LEGACY_DIR, "databases", db_name + ".db")
    if not os.path.exists(db_path):
        print(f"FEHLER: {db_path} nicht gefunden")
        return 1
    config_handler.get_db_file = lambda: f"file:{db_path}?mode=ro"

    import sqlite_db
    _orig_connect = sqlite3.connect
    sqlite_db.connect_db = lambda: _orig_connect(f"file:{db_path}?mode=ro", uri=True)

    from sqlite_db import (get_all_players, get_latest_dwrs_ratings, get_user_club,
                           get_second_team_club)
    from squad_logic import (calculate_squad_and_surplus, calculate_development_squads)
    from gap_analysis_logic import analyze_team_gaps
    from tactic_explorer_logic import analyze_all_tactics
    from constants import get_tactic_roles
    from config_handler import get_gap_analysis_setting

    players = get_all_players()
    user_club = get_user_club()
    second_club = get_second_team_club()
    print(f"{len(players)} Spieler, Club: {user_club}, Second: {second_club}")

    # Numeric master ratings, same as squad_logic.get_master_role_ratings.
    raw = get_latest_dwrs_ratings()
    master = {}
    for role, per_player in raw.items():
        master[role] = {}
        for uid, (absolute, normalized_str) in per_player.items():
            try:
                master[role][uid] = float(str(normalized_str).rstrip('%'))
            except (ValueError, TypeError):
                master[role][uid] = 0.0

    my_club = [p for p in players if p.get('Club') == user_club]
    second_team = [p for p in players if p.get('Club') == second_club] if second_club else []
    players_by_id = {p['Unique ID']: p for p in players}

    disp_thr = get_gap_analysis_setting('displacement_threshold')
    drop_thr = get_gap_analysis_setting('dropoff_threshold')
    side_pen = get_gap_analysis_setting('wrong_side_penalty')

    def xi_uids(team):
        return {slot: cell.get('player_id') for slot, cell in team.items()}

    result = {"user_club": user_club, "second_club": second_club, "tactics": {}}

    for tactic, positions in get_tactic_roles().items():
        squad = calculate_squad_and_surplus(my_club, positions, master)
        dev = calculate_development_squads(
            second_team, squad["depth_pool"], positions, master,
            depth_player_ids=squad.get("depth_player_ids", set()))

        gaps_xi = analyze_team_gaps(squad["starting_xi"], positions, players_by_id,
                                    master, disp_thr, drop_thr, side_pen)
        gaps_b = analyze_team_gaps(squad["b_team"], positions, players_by_id,
                                   master, disp_thr, drop_thr, side_pen)

        result["tactics"][tactic] = {
            "starting_xi": xi_uids(squad["starting_xi"]),
            "b_team": xi_uids(squad["b_team"]),
            "depth": {role: opts[0]["name"] for role, opts in squad["best_depth_options"].items()},
            "depth_uids": sorted(squad.get("depth_player_ids", set())),
            "youth_xi": xi_uids(dev["youth_xi"]),
            "second_xi": xi_uids(dev["second_team_xi"]),
            "loan": [p['Unique ID'] for p in dev["loan_candidates"]],
            "sell": [p['Unique ID'] for p in dev["sell_candidates"]],
            "gaps_xi": [[g["slot"], round(g["gap_score"], 4)] for g in gaps_xi],
            "gaps_b": [[g["slot"], round(g["gap_score"], 4)] for g in gaps_b],
        }

    explorer = analyze_all_tactics(my_club, master)
    result["explorer"] = [[r["tactic"], r["filled_slots"], r["overall_median"]]
                          for r in explorer]

    out_path = os.path.join(out_dir, f"squad_{db_name}.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=1, ensure_ascii=False)
    print(f"{out_path} geschrieben ({len(result['tactics'])} Taktiken)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
