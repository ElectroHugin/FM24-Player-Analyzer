# talent_logic.py
#
# Single source of truth for the "Talent Score" used across the app (Squad
# Matrix domestic/foreign talent filter, Player Profile, and the Best XI
# development/loan recommendations).
#
# The score answers "how much upside is hidden in this young player?":
#
#   Talent = best DWRS of the relevant roles          (what he can do now)
#          + 2 per year he is under the age cap        (development runway)
#          + (Determination + Work Rate - 20) / 4      (visible dev drivers)
#          + 3 for a good / - 5 for a bad personality  (hidden dev drivers)
#
# calculate_talent_score() is the canonical scalar implementation;
# add_talent_column() is its vectorized mirror for whole matrices. Keep the
# two in sync — the parity test compares them on the real database.

import pandas as pd

from constants import get_personality_category

PERSONALITY_BONUS = {'good': 3.0, 'bad': -5.0}


def _to_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def calculate_talent_score(best_dwrs, age, determination, work_rate, personality, age_cap):
    """Scalar talent score for a single player. `age_cap` is the reference age
    for the development-runway bonus (a player at the cap gets no bonus)."""
    best = _to_float(best_dwrs, 0.0)
    age_val = _to_float(age, age_cap)
    det = _to_float(determination, 0.0)
    wor = _to_float(work_rate, 0.0)
    bonus = PERSONALITY_BONUS.get(get_personality_category(personality), 0.0)
    return best + 2 * (age_cap - age_val) + (det + wor - 20) / 4 + bonus


def add_talent_column(df, role_cols, age_cap):
    """Vectorized talent score for a matrix DataFrame. Returns a float Series
    aligned to df.index. Mirrors calculate_talent_score() exactly.

    `role_cols` are the role columns whose best value counts as "current
    ability"; if none are present the ability term is 0."""
    age = pd.to_numeric(df['Age'], errors='coerce').fillna(age_cap)
    det = pd.to_numeric(df.get('Determination'), errors='coerce').fillna(0)
    wor = pd.to_numeric(df.get('Work Rate'), errors='coerce').fillna(0)

    present = [r for r in role_cols if r in df.columns]
    if present:
        best = df[present].max(axis=1).fillna(0)
    else:
        best = pd.Series(0.0, index=df.index)

    bonus = df['Personality'].map(get_personality_category).map(PERSONALITY_BONUS).fillna(0)
    return best + 2 * (age_cap - age) + (det + wor - 20) / 4 + bonus


def best_dwrs_for_player(player, master_role_ratings, roles=None):
    """Highest DWRS (numeric) a player has across `roles` (default: his
    assigned roles), read from a master_role_ratings dict {role: {uid: value}}."""
    uid = player.get('Unique ID')
    if roles is None:
        roles = player.get('Assigned Roles', []) or []
    best = 0.0
    for role in roles:
        rating = master_role_ratings.get(role, {}).get(uid, 0)
        if rating and rating > best:
            best = rating
    return best


def talent_age_cap_for_player(player, outfielder_cap, goalkeeper_cap):
    """Goalkeepers develop later, so they use a higher age cap."""
    is_gk = 'GK' in (player.get('Position') or '')
    return goalkeeper_cap if is_gk else outfielder_cap
