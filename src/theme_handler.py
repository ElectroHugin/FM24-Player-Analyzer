# src/theme_handler.py

import os
import toml

# --- CORRECTED IMPORT ---
# This correctly imports the helper functions from your utils file.
from utils import hex_to_rgb, get_luminance, calculate_contrast_ratio

# --- Path Definitions ---
_CURRENT_FILE_DIR = os.path.dirname(os.path.abspath(__file__))
_PROJECT_ROOT = os.path.dirname(_CURRENT_FILE_DIR)
CONFIG_TOML_FILE = os.path.join(_PROJECT_ROOT, '.streamlit', 'config.toml')

# --- TOML File Management Functions ---

def get_theme_from_toml():
    """Reads the current theme colors from .streamlit/config.toml."""
    try:
        with open(CONFIG_TOML_FILE, 'r') as f:
            config = toml.load(f)
        
        primary = config.get('theme', {}).get('primaryColor', '#0055a4')
        secondary = config.get('theme', {}).get('textColor', '#FFFFFF') # For consistency, this key must be textColor
        return primary, secondary
    except (FileNotFoundError, toml.TomlDecodeError):
        return '#0055a4', '#FFFFFF'

def set_theme_toml(primary_color, text_color): # Renamed for clarity
    """Safely reads, updates, and writes theme colors to .streamlit/config.toml."""
    os.makedirs(os.path.dirname(CONFIG_TOML_FILE), exist_ok=True)
    
    try:
        with open(CONFIG_TOML_FILE, 'r') as f:
            config = toml.load(f)
    except (FileNotFoundError, toml.TomlDecodeError):
        config = {}

    if 'theme' not in config:
        config['theme'] = {}

    # Streamlit requires these specific camelCase keys
    config['theme']['primaryColor'] = primary_color
    config['theme']['textColor'] = text_color
    config['theme']['backgroundColor'] = '#0E1117'
    config['theme']['secondaryBackgroundColor'] = '#262730' 
    config['theme']['font'] = 'sans serif'
    
    with open(CONFIG_TOML_FILE, 'w') as f:
        toml.dump(config, f)