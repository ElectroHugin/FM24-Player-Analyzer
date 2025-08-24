# src/definitions_handler.py

import json
import os
import streamlit as st
import shutil # For file backups

from definitions_loader import PROJECT_ROOT

# Build the absolute path to the definitions file
DEFINITIONS_FILE = os.path.join(PROJECT_ROOT, 'config', 'definitions.json')

def get_definitions():
    """Loads the raw definitions file without caching."""
    try:
        with open(DEFINITIONS_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        # Fallback to the cached loader if direct reading fails during an operation
        from definitions_loader import load_definitions
        return load_definitions()

def save_definitions(data):
    """
    Safely writes the updated definitions data to the JSON file.
    Creates a backup before writing.
    """
    backup_file = DEFINITIONS_FILE + '.bak'
    try:
        # 1. Create a backup
        shutil.copy(DEFINITIONS_FILE, backup_file)

        # 2. Write the new data
        with open(DEFINITIONS_FILE, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=4)
        
        # 3. If write is successful, remove the backup
        if os.path.exists(backup_file):
            os.remove(backup_file)
            
        return True, "Successfully saved definitions."
    except Exception as e:
        # If something goes wrong, restore from backup
        if os.path.exists(backup_file):
            shutil.copy(backup_file, DEFINITIONS_FILE)
        return False, f"An error occurred: {e}. Restored from backup."