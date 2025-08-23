# constants.py
from definitions_loader import load_definitions


# --- DYNAMIC DEFINITION FUNCTIONS ---
# By wrapping these in functions, we ensure the app can get the latest
# definitions after they have been modified by the user.

def get_player_roles():
    return load_definitions().get('player_roles', {})

def get_role_specific_weights():
    return load_definitions().get('role_specific_weights', {})

def get_position_to_role_mapping():
    return load_definitions().get('position_to_role_mapping', {})

def get_tactic_roles():
    return load_definitions().get('tactic_roles', {})

def get_tactic_layouts():
    return load_definitions().get('tactic_layouts', {})

def get_valid_roles():
    """Generates and returns a sorted list of all valid role abbreviations."""
    player_roles = get_player_roles()
    return sorted([role for category in player_roles.values() for role in category.keys()])
# -----------------------------------------

# Attribute mapping (HTML abbreviations to full names)
attribute_mapping = {
    "Reg": "Registration",
    "Inf": "Information",
    "Name": "Name",
    "Age": "Age",
    "Wage": "Wage",
    "Transfer Value": "Transfer Value",
    "Nat": "Nationality",
    "2nd Nat": "Second Nationality",
    "Position": "Position",
    "Personality": "Personality",
    "Media Handling": "Media Handling",
    "Av Rat": "Average Rating",
    "Left Foot": "Left Foot",
    "Right Foot": "Right Foot",
    "Height": "Height",
    "1v1": "One vs One",
    "Acc": "Acceleration",
    "Aer": "Aerial Reach",
    "Agg": "Aggression",
    "Agi": "Agility",
    "Ant": "Anticipation",
    "Bal": "Balance",
    "Bra": "Bravery",
    "Cmd": "Command of Area",
    "Cnt": "Concentration",
    "Cmp": "Composure",
    "Cro": "Crossing",
    "Dec": "Decisions",
    "Det": "Determination",
    "Dri": "Dribbling",
    "Fin": "Finishing",
    "Fir": "First Touch",
    "Fla": "Flair",
    "Han": "Handling",
    "Hea": "Heading",
    "Jum": "Jumping Reach",
    "Kic": "Kicking",
    "Ldr": "Leadership",
    "Lon": "Long Shots",
    "Mar": "Marking",
    "OtB": "Off the Ball",
    "Pac": "Pace",
    "Pas": "Passing",
    "Pos": "Positioning",
    "Ref": "Reflexes",
    "Sta": "Stamina",
    "Str": "Strength",
    "Tck": "Tackling",
    "Tea": "Teamwork",
    "Tec": "Technique",
    "Thr": "Throwing",
    "TRO": "Rushing Out (Tendency)",
    "Vis": "Vision",
    "Wor": "Work Rate",
    "UID": "Unique ID",
    "Cor": "Corners",
    "Club": "Club",
    "Agreed Playing Time": "Agreed Playing Time"
}

# Columns available for sorting in the Assign Roles page
SORTABLE_COLUMNS = [
    "Name",
    "Age",
    "Average Rating",
    "Position",
    "Club",
    "Unique ID"
]

# Filter options for the Assign Roles page
FILTER_OPTIONS = [
    "All Players",
    "Unassigned Players",
    "Players Not From My Club",
    "Unassigned Players Not From My Club"
]

# Columns for Role Analysis page
ROLE_ANALYSIS_COLUMNS = [
    "Unique ID",
    "Name",
    "Age",
    "Position",
    "Left Foot",
    "Right Foot",
    "Height",
    "Club",
    "Transfer Value",
    "DWRS Rating (Absolute)",
    "DWRS Rating (Normalized)"
]

# Columns for Player-Role Matrix page (base columns + roles)
PLAYER_ROLE_MATRIX_COLUMNS = [
    "Name",
    "Age",
    "Position",
    "Left Foot",
    "Right Foot",
    "Height",
    "Club",
    "Transfer Value"
]

# Global stat categories for DWRS rating
GLOBAL_STAT_CATEGORIES = {
    "Pace": "Extremely Important",
    "Acceleration": "Extremely Important",
    "Jumping Reach": "Important",
    "Anticipation": "Important",
    "Balance": "Important",
    "Agility": "Important",
    "Concentration": "Important",
    "Finishing": "Important",
    "Work Rate": "Good",
    "Dribbling": "Good",
    "Stamina": "Good",
    "Strength": "Good",
    "Passing": "Good",
    "Determination": "Good",
    "Vision": "Good",
    "Long Shots": "Decent",
    "Marking": "Decent",
    "Decisions": "Decent",
    "First Touch": "Decent",
    "Off the Ball": "Almost Irrelevant",
    "Tackling": "Almost Irrelevant",
    "Teamwork": "Almost Irrelevant",
    "Composure": "Almost Irrelevant",
    "Technique": "Almost Irrelevant",
    "Positioning": "Almost Irrelevant"
}

# GK stat categories for DWRS rating
GK_STAT_CATEGORIES = {
    "Agility": "Top Importance",
    "Aerial Reach": "High Importance",
    "Reflexes": "High Importance",
    "Command of Area": "Medium Importance",
    "Handling": "Medium Importance",
    "One vs One": "Medium Importance",
}

# Default global weights for DWRS rating (field players)
WEIGHT_DEFAULTS = {
    "Extremely Important": 8.0,
    "Important": 4.0,
    "Good": 2.0,
    "Decent": 1.0,
    "Almost Irrelevant": 0.2
}

# Default GK weights for DWRS rating
GK_WEIGHT_DEFAULTS = {
    "Top Importance": 10.0,
    "High Importance": 8.0,
    "Medium Importance": 6.0,
    "Key": 4.0,
    "Preferable": 2.0,
    "Other": 0.5
}

# Default role-specific multipliers
ROLE_MULTIPLIERS_DEFAULTS = {
    "key": 1.5,
    "preferable": 1.2
}

# Agreed Playing Time options
FIELD_PLAYER_APT_OPTIONS = [
    "None", "Star Player", "Important Player", "Regular Starter", "Squad Player",
    "Impact Sub", "Fringe Player", "Breakthrough Prospect", "Future Prospect", "Youngster", 
    "Emergency Backup", "2nd Team Regular", "Surplus to Requirements"
]

GK_APT_OPTIONS = [
    "None", "Star Player", "Important Player", "First-Choice Goalkeeper", "Cup Goalkeeper",
    "Backup", "Breakthrough Prospect", "Future Prospect", "Youngster",
    "Emergency Backup", "Surplus to Requirements"
]

APT_ABBREVIATIONS = {
    "Important Player": "Important",
    "Breakthrough Prospect": "Prospect",
    "Future Prospect": "Future",
    "Emergency Backup": "Backup",
    "2nd Team Regular": "2nd Team",
    "Surplus to Requirements": "Surplus",
    # Note: GK-specific long names are not needed here as per the design.
}

# NEW: Master map for tactical grid display
# Defines the stratum (vertical level) and column index (horizontal slot) for each position key.
# Defensive/Midfield strata use a 5-column grid (indices 0-4).
# Striker stratum uses a 3-column grid (indices 0-2).
MASTER_POSITION_MAP = {
    # Defense (5 slots, indices 0-4)
    "DL": ("Defense", 0), "WBL": ("Defense", 0),
    "DCL": ("Defense", 1),
    "DC": ("Defense", 2),
    "DCR": ("Defense", 3),
    "DR": ("Defense", 4), "WBR": ("Defense", 4),
    # Defensive Midfield (5 slots, indices 0-4)
    "DML": ("Defensive Midfield", 0),
    "DMCL": ("Defensive Midfield", 1),
    "DMC": ("Defensive Midfield", 2),
    "DMCR": ("Defensive Midfield", 3),
    "DMR": ("Defensive Midfield", 4),
    # Midfield (5 slots, indices 0-4)
    "ML": ("Midfield", 0),
    "MCL": ("Midfield", 1),
    "MC": ("Midfield", 2),
    "MCR": ("Midfield", 3),
    "MR": ("Midfield", 4),
    # Attacking Midfield (5 slots, indices 0-4)
    "AML": ("Attacking Midfield", 0),
    "AMCL": ("Attacking Midfield", 1),
    "AMC": ("Attacking Midfield", 2),
    "AMCR": ("Attacking Midfield", 3),
    "AMR": ("Attacking Midfield", 4),
    # Strikers (3 slots, indices 0-2)
    "STL": ("Strikers", 0),
    "STC": ("Strikers", 1), "ST": ("Strikers", 1), # Both ST and STC map to the central slot
    "STR": ("Strikers", 2)
}
