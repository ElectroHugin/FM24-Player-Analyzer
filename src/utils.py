# utils.py

import os
import base64
import streamlit as st
import re

from constants import get_player_roles
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