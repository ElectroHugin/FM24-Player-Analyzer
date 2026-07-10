#!/usr/bin/env python3
"""Golden-master exporter: dumps the legacy app's DWRS and talent results as
CSV so the C++ port can be verified against them bit-for-bit.

Usage (from the legacy/ directory):
    python tools/export_golden.py <db_name> <output_dir>

<db_name> is the database name without .db (as in config.ini); the script
reads legacy/databases/<db_name>.db read-only and writes:
    <output_dir>/dwrs_<db_name>.csv    uid,role,absolute,normalized
    <output_dir>/talent_<db_name>.csv  uid,age_cap,best_dwrs,talent

DWRS is recomputed from current player attributes with the weights from
config.ini (NOT read from the dwrs_ratings history), for every role each
player has assigned — exactly what update_dwrs_ratings would compute.
Talent uses the best freshly-computed DWRS across assigned roles.
"""

import csv
import os
import sqlite3
import sys

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
_LEGACY_DIR = os.path.dirname(_TOOLS_DIR)
sys.path.insert(0, os.path.join(_LEGACY_DIR, "src"))

import numpy as np
import pandas as pd


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    db_name, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    db_path = os.path.join(_LEGACY_DIR, "databases", db_name + ".db")
    if not os.path.exists(db_path):
        print(f"FEHLER: {db_path} nicht gefunden")
        return 1

    # Import after sys.path setup. These modules pull in streamlit, which is
    # fine in bare mode (cache decorators become no-ops with warnings).
    from analytics import build_attribute_matrix, calculate_dwrs_role_batch
    from config_handler import get_weight, get_role_multiplier, get_age_threshold
    from constants import (WEIGHT_DEFAULTS, GK_WEIGHT_DEFAULTS, get_gk_roles,
                           get_valid_roles, get_personality_category)
    from talent_logic import PERSONALITY_BONUS

    conn = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
    df = pd.read_sql_query("SELECT * FROM players", conn)
    conn.close()

    import ast

    def parse_list(v):
        if not v:
            return []
        try:
            parsed = ast.literal_eval(v)
            return parsed if isinstance(parsed, list) else []
        except (ValueError, SyntaxError):
            return []

    df["Assigned Roles"] = df["Assigned Roles"].map(parse_list)
    df = df.reset_index(drop=True)

    weights = {cat: get_weight(cat.lower().replace(" ", "_"), default)
               for cat, default in WEIGHT_DEFAULTS.items()}
    gk_weights = {cat: get_weight("gk_" + cat.lower().replace(" ", "_"), default)
                  for cat, default in GK_WEIGHT_DEFAULTS.items()}
    all_gk_roles = set(get_gk_roles())
    valid_roles = set(get_valid_roles())

    attr_matrix = build_attribute_matrix(df)
    uids = df["Unique ID"].tolist()

    role_to_rows = {}
    for i, roles in enumerate(df["Assigned Roles"]):
        for role in roles:
            if role in valid_roles:
                role_to_rows.setdefault(role, []).append(i)

    best_dwrs = {}  # uid -> best normalized value across assigned roles
    dwrs_path = os.path.join(out_dir, f"dwrs_{db_name}.csv")
    n_rows = 0
    with open(dwrs_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["uid", "role", "absolute", "normalized"])
        for role in sorted(role_to_rows):
            row_list = role_to_rows[role]
            rows = np.asarray(row_list, dtype=np.intp)
            w = gk_weights if role in all_gk_roles else weights
            absolute_arr, normalized_arr = calculate_dwrs_role_batch(attr_matrix, role, w, rows)
            for j, i in enumerate(row_list):
                uid = uids[i]
                writer.writerow([uid, role, repr(float(absolute_arr[j])),
                                 repr(float(normalized_arr[j]))])
                n_rows += 1
                if normalized_arr[j] > best_dwrs.get(uid, 0.0):
                    best_dwrs[uid] = float(normalized_arr[j])
    print(f"{dwrs_path}: {n_rows} Zeilen")

    # Talent: scalar formula on every player, age cap by GK position.
    outfielder_cap = get_age_threshold("outfielder")
    gk_cap = get_age_threshold("goalkeeper")

    def to_float(v, default=0.0):
        try:
            return float(v)
        except (TypeError, ValueError):
            return default

    talent_path = os.path.join(out_dir, f"talent_{db_name}.csv")
    with open(talent_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["uid", "age_cap", "best_dwrs", "talent"])
        for _, p in df.iterrows():
            uid = p["Unique ID"]
            cap = gk_cap if "GK" in (p.get("Position") or "") else outfielder_cap
            best = best_dwrs.get(uid, 0.0)
            age = to_float(p.get("Age"), cap)
            det = to_float(p.get("Determination"), 0.0)
            wor = to_float(p.get("Work Rate"), 0.0)
            bonus = PERSONALITY_BONUS.get(get_personality_category(p.get("Personality")), 0.0)
            talent = best + 2 * (cap - age) + (det + wor - 20) / 4 + bonus
            writer.writerow([uid, cap, repr(best), repr(talent)])
    print(f"{talent_path}: {len(df)} Zeilen")
    return 0


if __name__ == "__main__":
    sys.exit(main())
