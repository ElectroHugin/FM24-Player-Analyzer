# analytics.py

from constants import GLOBAL_STAT_CATEGORIES, GK_STAT_CATEGORIES, get_role_specific_weights
from config_handler import get_role_multiplier

def calculate_dwrs(player, role, weights):
    all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]
    stat_categories = GK_STAT_CATEGORIES if role in all_gk_roles else GLOBAL_STAT_CATEGORIES
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