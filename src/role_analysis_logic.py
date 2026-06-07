# role_analysis_logic.py
#
# Business logic for the "Pros & Cons" feature: given a player and a role,
# translate raw attributes into a human-readable list of strengths and
# weaknesses for that specific role.
#
# This module is intentionally UI-free so it can be reused by the Role
# Analysis page, a future Player Profile view, or anywhere else.

from constants import get_role_specific_weights, get_player_roles

# Roles that use goalkeeper attributes / are evaluated as keepers.
ALL_GK_ROLES = ["GK-D", "SK-D", "SK-S", "SK-A"]

# --- Thresholds ---
# Key attributes are central to the role, so we judge them on a tighter scale.
KEY_PRO_THRESHOLD = 15      # >= 15 on a key attribute is a genuine strength
KEY_CON_THRESHOLD = 9       # <= 9 on a key attribute is a real weakness

# Preferable attributes matter less, so we only flag the extremes.
PREF_PRO_THRESHOLD = 16     # must be excellent to count as a "pro"
PREF_CON_THRESHOLD = 7      # must be quite poor to count as a "con"

# An attribute this high is highlighted as "elite" regardless of key/pref.
ELITE_THRESHOLD = 18


def parse_attribute_value(raw_value):
    """
    Convert a stored attribute into a float, using the same convention as
    analytics.py: a plain number, or a range like '12-15' -> mean (13.5).
    Missing / non-numeric values become 0.0.
    """
    if raw_value is None:
        return 0.0
    if isinstance(raw_value, str) and '-' in raw_value:
        try:
            return sum(map(float, raw_value.split('-'))) / 2
        except (ValueError, TypeError):
            return 0.0
    try:
        return float(raw_value)
    except (ValueError, TypeError):
        return 0.0


def _role_display_name(role_abbr):
    """Return the human-readable role name (e.g. 'Poacher (Attack)')."""
    for category in get_player_roles().values():
        if role_abbr in category:
            return category[role_abbr]
    return role_abbr


def analyze_player_for_role(player, role):
    """
    Compare a player's attributes against a role's key/preferable attributes
    and return a structured dict of pros and cons.

    Returns:
        {
            "role": role_abbr,
            "role_name": human-readable role name,
            "pros": [ {"attr": str, "value": float, "tier": "key"|"preferable", "elite": bool}, ... ],
            "cons": [ {"attr": str, "value": float, "tier": "key"|"preferable"}, ... ],
        }
    Pros are sorted strongest-first, cons weakest-first. Key attributes are
    prioritized over preferable ones when ordering.
    """
    role_weights = get_role_specific_weights().get(role, {"key": [], "preferable": []})
    key_attrs = role_weights.get("key", [])
    pref_attrs = role_weights.get("preferable", [])

    pros, cons = [], []

    for attr in key_attrs:
        value = parse_attribute_value(player.get(attr, 0))
        if value <= 0:
            continue  # attribute not present in this export — skip silently
        if value >= KEY_PRO_THRESHOLD:
            pros.append({
                "attr": attr, "value": value, "tier": "key",
                "elite": value >= ELITE_THRESHOLD,
            })
        elif value <= KEY_CON_THRESHOLD:
            cons.append({"attr": attr, "value": value, "tier": "key"})

    for attr in pref_attrs:
        value = parse_attribute_value(player.get(attr, 0))
        if value <= 0:
            continue
        if value >= PREF_PRO_THRESHOLD:
            pros.append({
                "attr": attr, "value": value, "tier": "preferable",
                "elite": value >= ELITE_THRESHOLD,
            })
        elif value <= PREF_CON_THRESHOLD:
            cons.append({"attr": attr, "value": value, "tier": "preferable"})

    # Ordering: key tier first, then by value (pros high->low, cons low->high)
    tier_rank = {"key": 0, "preferable": 1}
    pros.sort(key=lambda p: (tier_rank[p["tier"]], -p["value"]))
    cons.sort(key=lambda c: (tier_rank[c["tier"]], c["value"]))

    return {
        "role": role,
        "role_name": _role_display_name(role),
        "pros": pros,
        "cons": cons,
    }


def _article(word):
    """Return 'an' if the word starts with a vowel sound, else 'a'."""
    return "an" if word[:1].lower() in "aeiou" else "a"


def format_pro_line(pro, role_name):
    """Render a single pro as a readable string, e.g.
    'Elite Finishing for a Poacher (Attack)' or 'Strong Tackling'."""
    qualifier = "Elite" if pro.get("elite") else "Strong"
    base = f"{qualifier} {pro['attr']} ({int(round(pro['value']))})"
    if pro["tier"] == "key":
        return f"{base} for {_article(role_name)} {role_name}"
    return base


def format_con_line(con, role_name):
    """Render a single con as a readable string, e.g.
    'Low Work Rate for a Pressing Forward (Support)'."""
    base = f"Low {con['attr']} ({int(round(con['value']))})"
    if con["tier"] == "key":
        return f"{base} for {_article(role_name)} {role_name}"
    return base


def get_top_roles_for_player(player, latest_ratings, limit=5):
    """
    Return the player's best roles by normalized DWRS, limited to roles they
    actually have assigned.

    Args:
        player: the player record dict (must contain 'Unique ID' and 'Assigned Roles').
        latest_ratings: the nested dict from get_latest_dwrs_ratings()
                        -> {role: {uid: (absolute, 'NN%')}}.
        limit: max number of roles to return.

    Returns a list of dicts: [{"role", "role_name", "normalized", "absolute"}, ...]
    sorted by normalized DWRS, highest first.
    """
    uid = player.get('Unique ID')
    assigned = player.get('Assigned Roles', []) or []
    results = []

    for role in assigned:
        rating = latest_ratings.get(role, {}).get(uid)
        if not rating:
            continue
        absolute, normalized_str = rating
        try:
            normalized = int(float(str(normalized_str).rstrip('%')))
        except (ValueError, TypeError):
            continue
        results.append({
            "role": role,
            "role_name": _role_display_name(role),
            "normalized": normalized,
            "absolute": absolute,
        })

    results.sort(key=lambda r: r["normalized"], reverse=True)
    return results[:limit]