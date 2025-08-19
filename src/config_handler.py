# config_handler.py

import configparser
import os
import streamlit as st
from constants import WEIGHT_DEFAULTS, GK_WEIGHT_DEFAULTS

# Get the absolute path to the directory this file is in (i.e., .../project/src)
_CURRENT_FILE_DIR = os.path.dirname(os.path.abspath(__file__))
# Get the project root directory by going one level up from 'src'
_PROJECT_ROOT = os.path.dirname(_CURRENT_FILE_DIR)

# Build the absolute path to the config file
CONFIG_FILE = os.path.join(_PROJECT_ROOT, 'config', 'config.ini')

@st.cache_data
def load_config():
    config = configparser.ConfigParser()
    # Create the config directory if it doesn't exist, to prevent errors on first run
    os.makedirs(os.path.dirname(CONFIG_FILE), exist_ok=True)
    if not os.path.exists(CONFIG_FILE):
        config['Database'] = {'db_name': 'default'}
        config['Weights'] = {k.lower().replace(' ', '_'): str(v) for k, v in WEIGHT_DEFAULTS.items()}
        config['GKWeights'] = {k.lower().replace(' ', '_'): str(v) for k, v in GK_WEIGHT_DEFAULTS.items()}
        config['RoleMultipliers'] = {'key_multiplier': '1.5', 'preferable_multiplier': '1.2'}
        with open(CONFIG_FILE, 'w') as f:
            config.write(f)
    config.read(CONFIG_FILE)
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