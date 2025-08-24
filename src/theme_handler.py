# src/theme_handler.py

import os
import toml

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
        
        theme = config.get('theme', {})
        return {
            'primaryColor': theme.get('primaryColor', '#0055a4'),
            'textColor': theme.get('textColor', '#FFFFFF'),
            'backgroundColor': theme.get('backgroundColor', '#0E1117'),
            'secondaryBackgroundColor': theme.get('secondaryBackgroundColor', '#262730')
        }
    except (FileNotFoundError, toml.TomlDecodeError):
        return {
            'primaryColor': '#0055a4',
            'textColor': '#FFFFFF',
            'backgroundColor': '#0E1117',
            'secondaryBackgroundColor': '#262730'
        }

def set_theme_toml(primary_color, text_color, background_color, secondary_background_color):
    """Safely reads, updates, and writes all theme colors to .streamlit/config.toml."""
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
    config['theme']['backgroundColor'] = background_color
    config['theme']['secondaryBackgroundColor'] = secondary_background_color
    config['theme']['font'] = 'sans serif'
    
    with open(CONFIG_TOML_FILE, 'w') as f:
        toml.dump(config, f)