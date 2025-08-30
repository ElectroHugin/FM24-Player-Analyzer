# utils.py

import os
import base64
import streamlit as st
import re
from collections import defaultdict
import matplotlib.colors as mcolors
import matplotlib.cm as cm

from constants import get_player_roles, get_valid_roles, get_position_to_role_mapping, MASTER_POSITION_MAP
from definitions_loader import PROJECT_ROOT


def get_last_name(full_name):
    """Extracts the last name from a full name string."""
    if isinstance(full_name, str) and full_name:
        return full_name.split(' ')[-1]
    return ""

def get_role_display_map():
    player_roles = get_player_roles()
    return {role: name for category in player_roles.values() for role, name in category.items()}

def format_role_display(role_abbr):
    return get_role_display_map().get(role_abbr, role_abbr)

def format_role_display_with_all(role_abbr):
    return "All Roles" if role_abbr == "All Roles" else get_role_display_map().get(role_abbr, role_abbr)

def hex_to_rgb(hex_color: str) -> tuple[int, int, int]:
    """Converts a hex color string to an (R, G, B) tuple."""
    hex_color = hex_color.lstrip('#')
    return tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))

def get_luminance(rgb: tuple[int, int, int]) -> float:
    """Calculates the relative luminance of an RGB color."""
    vals = []
    for val in rgb:
        s = val / 255.0
        if s <= 0.03928:
            vals.append(s / 12.92)
        else:
            vals.append(((s + 0.055) / 1.055) ** 2.4)
    return vals[0] * 0.2126 + vals[1] * 0.7152 + vals[2] * 0.0722

def calculate_contrast_ratio(hex1: str, hex2: str) -> float:
    """Calculates the contrast ratio between two hex colors."""
    lum1 = get_luminance(hex_to_rgb(hex1))
    lum2 = get_luminance(hex_to_rgb(hex2))
    if lum1 > lum2:
        return (lum1 + 0.05) / (lum2 + 0.05)
    else:
        return (lum2 + 0.05) / (lum1 + 0.05)

@st.cache_data
def get_image_as_base64(path):
    """Encodes an image file into base64 string for embedding in CSS."""
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        data = f.read()
    return base64.b64encode(data).decode()

def get_available_databases():
    """Scans the databases directory and returns a list of DB names without the .db extension."""
    db_folder = os.path.join(PROJECT_ROOT, 'databases')
    # Ensure the databases directory exists
    os.makedirs(db_folder, exist_ok=True)
    
    db_files = [f for f in os.listdir(db_folder) if f.endswith('.db') and os.path.isfile(os.path.join(db_folder, f))]
    return sorted([os.path.splitext(f)[0] for f in db_files])

def parse_position_string(pos_str):
    """
    Parses a complex position string like 'AM (RL), ST (C)' into a clean set of individual positions.
    Returns a set to automatically handle duplicates.
    """
    if not isinstance(pos_str, str):
        return set()
    
    final_pos = set()
    # Split by comma for multiple positions like "D (C), DM"
    for part in [p.strip() for p in pos_str.split(',')]:
        # Use regex to find the base position(s) and the sides (R, L, C)
        match = re.match(r'([A-Z/]+) *(?:\(([RLC]+)\))?$', part.strip())
        if match:
            bases, sides = match.groups()
            # Split bases like "D/WB"
            for base in bases.split('/'):
                if sides:
                    # For "AM (RL)", create "AM (R)" and "AM (L)"
                    for side in list(sides):
                        final_pos.add(f"{base} ({side})")
                else:
                    # For "ST", which implies "ST (C)"
                    final_pos.add(f"{base} (C)" if base == "ST" else base)
    return final_pos

@st.cache_data
def get_natural_role_sorter():
    """
    Creates a dictionary mapping each role to a sortable tuple.
    The tuple represents (stratum, column_index) for natural on-pitch sorting.
    e.g., Goalkeepers first, then Defenders L-R, then DMs L-R, etc.
    """
    pos_to_role_map = get_position_to_role_mapping()
    
    # Create a reverse mapping: role -> list of game positions (e.g., 'BPD-D' -> ['D (C)'])
    role_to_positions = defaultdict(list)
    for pos, roles in pos_to_role_map.items():
        for role in roles:
            role_to_positions[role].append(pos)

    # Define the numerical order for strata (vertical pitch sections)
    stratum_order = {
        # Lower numbers are further back on the pitch
        "GK": 0,
        "Defense": 1,
        "Defensive Midfield": 2,
        "Midfield": 3,
        "Attacking Midfield": 4,
        "Strikers": 5
    }

    # Define a mapping from the game position (e.g., 'D (C)') to a tactical slot (e.g., 'DC')
    # We take the first match we find, which is sufficient for sorting purposes.
    game_pos_to_slot = {}
    for slot, game_positions in MASTER_POSITION_MAP.items():
        # The key in MASTER_POSITION_MAP is the slot, but the data is a tuple. Let's use a different constant.
        # Let's rebuild this logic slightly to be more direct.
        pass # We'll build this map on the fly.

    role_sorter = {}
    
    for role in get_valid_roles():
        if "GK" in role or "SK" in role:
            role_sorter[role] = (0, 0) # Goalkeepers are always first
            continue

        associated_positions = role_to_positions.get(role, [])
        if not associated_positions:
            role_sorter[role] = (99, 99) # Unassigned roles go to the end
            continue
            
        best_score = (-1, -1)

        # Find the "highest" position this role can play.
        # e.g., if a Winger can be 'M (L)' and 'AM (L)', we use 'AM (L)' for sorting.
        for pos_key, (stratum, col_index) in MASTER_POSITION_MAP.items():
            # TACTICAL_SLOT_TO_GAME_POSITIONS maps a slot to its valid game positions
            from constants import TACTICAL_SLOT_TO_GAME_POSITIONS
            valid_game_positions = TACTICAL_SLOT_TO_GAME_POSITIONS.get(pos_key, [])
            
            # Check if any of the role's assigned positions match the valid positions for this slot
            if any(p in valid_game_positions for p in associated_positions):
                stratum_score = stratum_order.get(stratum, 99)
                current_score = (stratum_score, col_index)
                
                # We want the highest stratum (most attacking), and for ties, the lowest column (most left)
                if current_score[0] > best_score[0]:
                    best_score = current_score
                elif current_score[0] == best_score[0] and current_score[1] < best_score[1]:
                    best_score = current_score

        role_sorter[role] = best_score if best_score != (-1, -1) else (99, 99)

    return role_sorter

def _get_smart_style(val, cmap, norm):
    """
    A helper function that calculates background color from a colormap
    and dynamically sets text color to white or black for readability.
    """
    try:
        val_float = float(str(val).replace('%',''))
        # Get the color from the colormap as an RGBA tuple (0-1 range)
        rgba = cmap(norm(val_float))
        
        # Convert to a hex color for the background
        bg_color_hex = mcolors.to_hex(rgba)
        
        # Convert the RGB part to a 0-255 range for luminance calculation
        rgb_0_255 = tuple(int(c * 255) for c in rgba[:3])
        
        # Calculate perceived brightness
        luminance = get_luminance(rgb_0_255)
        
        # Set text color to black on light backgrounds, white on dark backgrounds
        text_color = 'black' if luminance > 0.5 else 'white'
        
        return f'background-color: {bg_color_hex}; color: {text_color};'
        
    except (ValueError, TypeError, AttributeError):
        return '' # Return empty string for non-numeric or None values

# --- Styler for DWRS (0-100) ---
_dwrs_cmap = cm.get_cmap('gist_rainbow')
_dwrs_norm = mcolors.Normalize(vmin=30, vmax=95)

def color_dwrs_by_value(val):
    """Applies a smart gist_rainbow gradient to a DWRS value."""
    return _get_smart_style(val, _dwrs_cmap, _dwrs_norm)

# --- Styler for Attributes (1-20) using CUSTOM if/elif logic ---
def color_attribute_by_value(val):
    """Applies a custom, high-performance 5-step gradient to an attribute value."""
    try:
        val = int(val)
        # Tiers: 1-7 (Red), 8-11 (Orange), 12-14 (Yellow), 15-17 (Light Green), 18-20 (Full Green)
        if val >= 18:
            # Full Green (dark background, white text)
            bg_color = "#0da025"
            text_color = 'white'
        elif val >= 15:
            # Light Green (light background, black text)
            bg_color = "#a2d31a"
            text_color = 'black'
        elif val >= 12:
            # Yellow (light background, black text)
            bg_color = "#d8d21e"
            text_color = 'black'
        elif val >= 8:
            # Orange (light background, black text)
            bg_color = "#ca7b3a"
            text_color = 'black'
        else:
            # Red (light background, black text)
            bg_color = "#cf1e1e"
            text_color = 'black'
            
        return f'background-color: {bg_color}; color: {text_color};'

    except (ValueError, TypeError, AttributeError):
        return ''