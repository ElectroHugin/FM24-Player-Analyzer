import json
import os
import streamlit as st

# Get the absolute path to the directory this file is in (i.e., .../project/src)
_CURRENT_FILE_DIR = os.path.dirname(os.path.abspath(__file__))
# Get the project root directory by going one level up from 'src'
_PROJECT_ROOT = os.path.dirname(_CURRENT_FILE_DIR)

# Build the absolute path to the definitions file
DEFINITIONS_FILE = os.path.join(_PROJECT_ROOT, 'config', 'definitions.json')

@st.cache_data
def load_definitions():
    """
    Loads roles, weights, and tactics from the definitions.json file.
    Raises an error if the file is missing.
    """
    if not os.path.exists(DEFINITIONS_FILE):
        st.error(f"FATAL ERROR: The definitions file '{DEFINITIONS_FILE}' was not found. The application cannot start.")
        st.stop()
    
    try:
        with open(DEFINITIONS_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        st.error(f"FATAL ERROR: Could not parse '{DEFINITIONS_FILE}'. Please ensure it is valid JSON. Details: {e}")
        st.stop()
    except Exception as e:
        st.error(f"An unexpected error occurred while reading '{DEFINITIONS_FILE}': {e}")
        st.stop()