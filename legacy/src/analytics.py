# analytics.py

import numpy as np
import pandas as pd

from constants import GLOBAL_STAT_CATEGORIES, GK_STAT_CATEGORIES, get_role_specific_weights, get_gk_roles
from config_handler import get_role_multiplier

def calculate_dwrs(player, role, weights):
    stat_categories = GK_STAT_CATEGORIES if role in get_gk_roles() else GLOBAL_STAT_CATEGORIES
    components = {cat: [] for cat in weights}
    role_weights = get_role_specific_weights().get(role, {"key": [], "preferable": []})
    key_attrs, pref_attrs = role_weights["key"], role_weights["preferable"]
    key_mult, pref_mult = get_role_multiplier('key'), get_role_multiplier('preferable')
    
    for attr, category in stat_categories.items():
        raw_value = player.get(attr, 0) or 0
        if isinstance(raw_value, str) and '-' in raw_value:
            try: value = sum(map(float, raw_value.split('-'))) / 2
            except (ValueError, TypeError): value = 0.0
        else:
            try: value = float(raw_value)
            except (ValueError, TypeError): value = 0.0
        
        role_weight = key_mult if attr in key_attrs else pref_mult if attr in pref_attrs else 1.0
        if category in components:
            components[category].append(value * role_weight)
    
    means = {cat: sum(values) / len(values) if values else 0 for cat, values in components.items()}
    absolute = sum(weights.get(cat, 0) * means.get(cat, 0) for cat in weights)
    
    worst_means, best_means = {cat: 0 for cat in components}, {cat: 0 for cat in components}
    for attr, category in stat_categories.items():
        role_weight = key_mult if attr in key_attrs else pref_mult if attr in pref_attrs else 1.0
        if category in worst_means:
            worst_means[category] += 1 * role_weight
            best_means[category] += 20 * role_weight
    
    for cat in worst_means:
        count = len(components.get(cat, []))
        if count > 0:
            worst_means[cat] /= count
            best_means[cat] /= count
            
    worst_possible = sum(weights.get(cat, 0) * worst_means.get(cat, 0) for cat in weights)
    best_possible = sum(weights.get(cat, 0) * best_means.get(cat, 0) for cat in weights)
    
    normalized = round((absolute - worst_possible) / (best_possible - worst_possible) * 100, 0) if best_possible != worst_possible else 0
    return absolute, f"{normalized:.0f}%"


# --- Vectorized batch versions -------------------------------------------
# calculate_dwrs() above is the readable reference implementation for a single
# player. Recalculating a full 80k-player scouting database with it takes ~12
# minutes of pure Python; the batch path below produces identical numbers via
# numpy in a few seconds. update_dwrs_ratings() uses the batch path.

def _parse_attr_column(series):
    """Vectorized equivalent of the per-value parsing in calculate_dwrs:
    numbers pass through, masked ranges like '12-15' become their mean,
    everything unparsable becomes 0.0."""
    values = pd.to_numeric(series, errors='coerce')
    unparsed = values.isna()
    if unparsed.any():
        # Only the non-numeric leftovers can be masked ranges ('12-15');
        # restricting the (expensive) string work to them keeps this fast.
        s = series[unparsed].astype(str).str.strip()
        is_range = s.str.match(r'^\d+(\.\d+)?\s*-\s*\d+(\.\d+)?$')
        if is_range.any():
            parts = s[is_range].str.split('-', expand=True)
            values.loc[s[is_range].index] = (
                pd.to_numeric(parts[0], errors='coerce')
                + pd.to_numeric(parts[1], errors='coerce')
            ) / 2
    return values.fillna(0.0).to_numpy(dtype=np.float64)


def build_attribute_matrix(df):
    """Parse every rating-relevant attribute column of `df` ONCE into float
    arrays. Returns {attribute_name: np.ndarray} aligned with df's rows;
    attributes missing from df map to zeros (same as player.get(attr, 0))."""
    needed = set(GLOBAL_STAT_CATEGORIES) | set(GK_STAT_CATEGORIES)
    n = len(df)
    zeros = np.zeros(n, dtype=np.float64)
    matrix = {}
    for attr in needed:
        matrix[attr] = _parse_attr_column(df[attr]) if attr in df.columns else zeros
    return matrix


def calculate_dwrs_role_batch(attr_matrix, role, weights, rows):
    """
    DWRS for ALL players in `rows` (index array into the attribute matrix) for
    one role. Mirrors calculate_dwrs exactly; returns (absolute, normalized)
    as float arrays, normalized already rounded to whole percent.
    """
    stat_categories = GK_STAT_CATEGORIES if role in get_gk_roles() else GLOBAL_STAT_CATEGORIES
    role_weights = get_role_specific_weights().get(role, {"key": [], "preferable": []})
    key_attrs, pref_attrs = set(role_weights["key"]), set(role_weights["preferable"])
    key_mult, pref_mult = get_role_multiplier('key'), get_role_multiplier('preferable')

    n = len(rows)
    cat_sums = {}      # category -> np.ndarray (sum of value*role_weight)
    cat_counts = {}    # category -> number of attributes
    worst_sums = {}    # category -> scalar sum of 1*role_weight
    best_sums = {}     # category -> scalar sum of 20*role_weight

    for attr, category in stat_categories.items():
        if category not in weights:
            continue
        role_weight = key_mult if attr in key_attrs else pref_mult if attr in pref_attrs else 1.0
        values = attr_matrix[attr][rows] * role_weight
        if category in cat_sums:
            cat_sums[category] += values
            cat_counts[category] += 1
            worst_sums[category] += 1 * role_weight
            best_sums[category] += 20 * role_weight
        else:
            cat_sums[category] = values.copy()
            cat_counts[category] = 1
            worst_sums[category] = 1 * role_weight
            best_sums[category] = 20 * role_weight

    absolute = np.zeros(n, dtype=np.float64)
    worst_possible = 0.0
    best_possible = 0.0
    for cat, weight in weights.items():
        count = cat_counts.get(cat, 0)
        if count:
            absolute += weight * (cat_sums[cat] / count)
            worst_possible += weight * (worst_sums[cat] / count)
            best_possible += weight * (best_sums[cat] / count)

    denom = best_possible - worst_possible
    if denom != 0:
        normalized = np.round((absolute - worst_possible) / denom * 100, 0)
    else:
        normalized = np.zeros(n, dtype=np.float64)
    return absolute, normalized