# config_handler.py

import configparser
import os
import streamlit as st
from constants import WEIGHT_DEFAULTS, GK_WEIGHT_DEFAULTS, FIELD_PLAYER_APT_OPTIONS, GK_APT_OPTIONS

# Get the absolute path to the directory this file is in (i.e., .../project/src)
_CURRENT_FILE_DIR = os.path.dirname(os.path.abspath(__file__))
# Get the project root directory by going one level up from 'src'
_PROJECT_ROOT = os.path.dirname(_CURRENT_FILE_DIR)

# Build the absolute path to the config file
CONFIG_FILE = os.path.join(_PROJECT_ROOT, 'config', 'config.ini')

@st.cache_data
def load_config():
    config = configparser.ConfigParser()
    config_was_modified = False

    # Ensure the config directory exists before trying to read from it
    os.makedirs(os.path.dirname(CONFIG_FILE), exist_ok=True)
    
    # Read the existing config file. If it doesn't exist, this does nothing.
    config.read(CONFIG_FILE)

    # --- NEW: Section-by-section validation and creation ---

    # 1. Ensure [Database] section exists
    if not config.has_section('Database'):
        config['Database'] = {'db_name': 'default'}
        config_was_modified = True

    # 2. Ensure [Weights] section exists
    if not config.has_section('Weights'):
        config['Weights'] = {k.lower().replace(' ', '_'): str(v) for k, v in WEIGHT_DEFAULTS.items()}
        config_was_modified = True

    # 3. Ensure [GKWeights] section exists
    if not config.has_section('GKWeights'):
        config['GKWeights'] = {k.lower().replace(' ', '_'): str(v) for k, v in GK_WEIGHT_DEFAULTS.items()}
        config_was_modified = True

    # 4. Ensure [RoleMultipliers] section exists
    if not config.has_section('RoleMultipliers'):
        config['RoleMultipliers'] = {'key_multiplier': '1.5', 'preferable_multiplier': '1.2'}
        config_was_modified = True

    # 5. Ensure [APTWeights] section exists (This is the critical fix)
    if not config.has_section('APTWeights'):
        config['APTWeights'] = {}
        all_apt = set(FIELD_PLAYER_APT_OPTIONS + GK_APT_OPTIONS)
        for apt in all_apt:
            if apt != "None":
                key = apt.lower().replace(' ', '_')
                config['APTWeights'][key] = '1.0'
        config_was_modified = True

    # --- End of validation ---

    # If we had to add any missing sections, write the complete config back to the file
    if config_was_modified or not os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, 'w') as f:
            config.write(f)

    return config

def get_db_file():
    # Construct the full, absolute path to the database file
    db_name = load_config()['Database']['db_name']
    db_folder = os.path.join(_PROJECT_ROOT, 'databases')
    # Create the databases directory if it doesn't exist
    os.makedirs(db_folder, exist_ok=True)
    return os.path.join(db_folder, db_name + '.db')

def get_db_name():
    return load_config()['Database']['db_name']

def set_db_name(name):
    config = load_config()
    config['Database']['db_name'] = name
    with open(CONFIG_FILE, 'w') as f:
        config.write(f)
    load_config.clear()

def get_weight(key, default):
    config = load_config()
    section = 'GKWeights' if key.startswith('gk_') else 'Weights'
    config_key = key[3:] if key.startswith('gk_') else key
    return float(config[section].get(config_key, str(default)))

def set_weight(key, value):
    config = load_config()
    section = 'GKWeights' if key.startswith('gk_') else 'Weights'
    config_key = key[3:] if key.startswith('gk_') else key
    if section not in config: config[section] = {}
    config[section][config_key] = str(value)
    with open(CONFIG_FILE, 'w') as f:
        config.write(f)
    load_config.clear()

def get_apt_weight(key, default=1.0):
    """Gets the weight for a given Agreed Playing Time status."""
    if not key or key == "None":
        return default
    config = load_config()
    # Standardize the key to match how it's stored in the config file
    config_key = key.lower().replace(' ', '_')
    # Use .get() for safe access, falling back to the default value
    return float(config['APTWeights'].get(config_key, str(default)))

def set_apt_weight(key, value):
    """Sets the weight for a given Agreed Playing Time status."""
    if not key or key == "None":
        return
    config = load_config()
    config_key = key.lower().replace(' ', '_')
    if 'APTWeights' not in config:
        config['APTWeights'] = {}
    config['APTWeights'][config_key] = str(value)
    with open(CONFIG_FILE, 'w') as f:
        config.write(f)
    # Clear the cache to ensure the new value is loaded on the next call
    load_config.clear()

def get_role_multiplier(type_):
    config = load_config()
    defaults = {'key': 1.5, 'preferable': 1.2}
    return float(config['RoleMultipliers'].get(type_ + '_multiplier', str(defaults[type_])))

def set_role_multiplier(type_, value):
    config = load_config()
    if 'RoleMultipliers' not in config: config['RoleMultipliers'] = {}
    config['RoleMultipliers'][type_ + '_multiplier'] = str(value)
    with open(CONFIG_FILE, 'w') as f:
        config.write(f)
    load_config.clear()