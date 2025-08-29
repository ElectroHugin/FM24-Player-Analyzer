# src/sqlite_db.py

import sqlite3
from datetime import datetime
import pandas as pd
from constants import attribute_mapping, get_valid_roles
import ast
from config_handler import get_db_file
import streamlit as st

def connect_db():
    return sqlite3.connect(get_db_file())


def init_db():
    conn = connect_db()
    cursor = conn.cursor()
    
    # --- 1. Player Table Migration (existing logic, unchanged) ---
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS players (
            "Unique ID" TEXT PRIMARY KEY, 
            "Assigned Roles" TEXT
        )
    """)
    cursor.execute("PRAGMA table_info(players)")
    existing_columns = [col[1] for col in cursor.fetchall()]
    for col_name in attribute_mapping.values():
        if col_name not in existing_columns:
            cursor.execute(f'ALTER TABLE players ADD COLUMN "{col_name}" TEXT')
    
    if "primary_role" not in existing_columns:
        cursor.execute('ALTER TABLE players ADD COLUMN "primary_role" TEXT')
    if "natural_positions" not in existing_columns:
        cursor.execute('ALTER TABLE players ADD COLUMN "natural_positions" TEXT')
    if "transfer_status" not in existing_columns:
        cursor.execute('ALTER TABLE players ADD COLUMN "transfer_status" INTEGER DEFAULT 0')
    if "loan_status" not in existing_columns:
        cursor.execute('ALTER TABLE players ADD COLUMN "loan_status" INTEGER DEFAULT 0')
    OLD_APT_COL, NEW_APT_COL = "agreed_playing_time", "Agreed Playing Time"
    if OLD_APT_COL in existing_columns:
        if NEW_APT_COL not in existing_columns:
            cursor.execute(f'ALTER TABLE players RENAME COLUMN "{OLD_APT_COL}" TO "{NEW_APT_COL}"')
        else:
            cursor.execute(f'UPDATE players SET "{NEW_APT_COL}" = "{OLD_APT_COL}" WHERE "{NEW_APT_COL}" IS NULL OR "{NEW_APT_COL}" = ""')
            cursor.execute(f'ALTER TABLE players DROP COLUMN "{OLD_APT_COL}"')

    # --- 2. Settings Table (unchanged) ---
    # --- DWRS RATINGS TABLE - FINAL, NON-DESTRUCTIVE MIGRATION ---
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS dwrs_ratings (
            unique_id TEXT, role TEXT, dwrs_absolute REAL, dwrs_normalized TEXT, timestamp TEXT,
            PRIMARY KEY (unique_id, role, timestamp)
        )
    """)

    cursor.execute("PRAGMA table_info(dwrs_ratings)")
    dwrs_columns = [col[1] for col in cursor.fetchall()]

    if "dwrs_absolute" not in dwrs_columns:
        st.warning("Older database version detected. Performing a safe, one-time upgrade...")
        with st.spinner("Adding new columns and backfilling missing rating data..."):
            try:
                # Step A: Add the missing column without deleting any data.
                cursor.execute('ALTER TABLE dwrs_ratings ADD COLUMN "dwrs_absolute" REAL DEFAULT 0.0')
                conn.commit()

                # Step B: Force a full recalculation to backfill the new column.

                df = pd.DataFrame(get_all_players())
                if not df.empty:
                    # This will now calculate and save the absolute value for all players.
                    # It will only add new rows if ratings have changed, but it will
                    # ensure future writes are in the correct format.
                    update_dwrs_ratings(df, get_valid_roles())
                
                st.cache_data.clear()
                st.success("Database upgrade successful! The app will now reload.")
                st.rerun()

            except Exception as e:
                st.error(f"Database upgrade failed: {e}. Please report this issue.")
                st.stop()

    conn.commit()
    conn.close()

@st.cache_data
def get_latest_dwrs_ratings():
    """
    Fetches the MOST RECENT DWRS rating for every player-role combination from the historical table.
    Returns a nested dictionary: {role: {unique_id: (absolute_val, normalized_str)}}
    """
    conn = connect_db()
    cursor = conn.cursor()

    # --- THIS IS THE CORRECT QUERY FOR A HISTORICAL TABLE ---
    # It finds the latest timestamp for each player-role pair first,
    # then joins back to get the full data for only those latest entries.
    query = """
        SELECT t1.unique_id, t1.role, t1.dwrs_absolute, t1.dwrs_normalized
        FROM dwrs_ratings t1
        INNER JOIN (
            SELECT unique_id, role, MAX(timestamp) as max_timestamp
            FROM dwrs_ratings
            GROUP BY unique_id, role
        ) t2 ON t1.unique_id = t2.unique_id AND t1.role = t2.role AND t1.timestamp = t2.max_timestamp
    """
    cursor.execute(query)
    rows = cursor.fetchall()
    conn.close()

    master_ratings = {}
    # This loop is now safe, because the query guarantees every row has 4 columns.
    for uid, role, dwrs_abs, dwrs_norm in rows:
        if role not in master_ratings:
            master_ratings[role] = {}
        master_ratings[role][uid] = (dwrs_abs, dwrs_norm)
        
    return master_ratings

def update_player_apt(unique_id, playing_time):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('UPDATE players SET "Agreed Playing Time" = ? WHERE "Unique ID" = ?', (playing_time, unique_id))
    conn.commit()
    conn.close()

def update_player(mapped_player_data):
    conn = connect_db()
    cursor = conn.cursor()

    uid = mapped_player_data.get('Unique ID')
    # If there's no Unique ID, we cannot process the row.
    if not uid:
        conn.close()
        return

    # Check if the player already exists in the database
    cursor.execute('SELECT "Unique ID" FROM players WHERE "Unique ID" = ?', (uid,))
    result = cursor.fetchone()

    # --- START OF NEW, SAFER LOGIC ---

    # Create a clean dictionary of only the columns we want to insert or update.
    # Exclude the Unique ID itself, as it's used to identify the row, not to be set.
    data_to_update = {k: v for k, v in mapped_player_data.items() if k != 'Unique ID'}

    if result:
        # PLAYER EXISTS: Build a dynamic UPDATE statement.
        # This ensures we only update the columns present in the uploaded file.
        if data_to_update: # Proceed only if there's actually data to update
            set_clauses = [f'"{col}" = ?' for col in data_to_update.keys()]
            values = list(data_to_update.values())
            values.append(uid)  # Add the UID for the WHERE clause at the end

            query = f'UPDATE players SET {", ".join(set_clauses)} WHERE "Unique ID" = ?'
            cursor.execute(query, values)
    else:
        # PLAYER IS NEW: Build a dynamic INSERT statement.
        # Start with the Unique ID
        columns = ['"Unique ID"']
        values = [uid]

        # Add the rest of the data from the file
        columns.extend([f'"{col}"' for col in data_to_update.keys()])
        values.extend(data_to_update.values())

        # Explicitly set "Assigned Roles" to an empty list for new players
        columns.append('"Assigned Roles"')
        values.append('[]')

        placeholders = ", ".join(["?" for _ in columns])
        query = f'INSERT INTO players ({", ".join(columns)}) VALUES ({placeholders})'
        cursor.execute(query, values)

    # --- END OF NEW LOGIC ---

    conn.commit()
    conn.close()

def update_player_roles(role_changes):
    conn = connect_db()
    cursor = conn.cursor()
    for unique_id, roles in role_changes.items():
        cursor.execute('UPDATE players SET "Assigned Roles" = ? WHERE "Unique ID" = ?', (str(roles), unique_id))
    conn.commit()
    conn.close()

def update_player_club(unique_id, new_club):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('UPDATE players SET "Club" = ? WHERE "Unique ID" = ?', (new_club, unique_id))
    conn.commit()
    conn.close()

def set_primary_role(unique_id, role):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('UPDATE players SET primary_role = ? WHERE "Unique ID" = ?', (role, unique_id))
    conn.commit()
    conn.close()

def update_dwrs_ratings(df, valid_roles):
    from analytics import calculate_dwrs
    from config_handler import get_weight
    from constants import WEIGHT_DEFAULTS, GK_WEIGHT_DEFAULTS
    
    conn = connect_db()
    cursor = conn.cursor()
    
    weights = {cat: get_weight(cat.lower().replace(" ", "_"), default) for cat, default in WEIGHT_DEFAULTS.items()}
    gk_weights = {cat: get_weight("gk_" + cat.lower().replace(" ", "_"), default) for cat, default in GK_WEIGHT_DEFAULTS.items()}
    all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # Get the latest existing rating for every player/role to compare against
    cursor.execute("""
        SELECT unique_id, role, dwrs_normalized FROM dwrs_ratings
        WHERE (unique_id, role, timestamp) IN (
            SELECT unique_id, role, MAX(timestamp) FROM dwrs_ratings GROUP BY unique_id, role
        )
    """)
    latest_ratings_dict = {(row[0], row[1]): row[2] for row in cursor.fetchall()}
    
    ratings_to_insert = []
    for _, player in df.iterrows():
        player_dict = player.to_dict()
        roles = player_dict.get('Assigned Roles', [])
        if not isinstance(roles, list): roles = []
        
        for role in roles:
            if role in valid_roles:
                weights_to_use = gk_weights if role in all_gk_roles else weights
                absolute, normalized = calculate_dwrs(player_dict, role, weights_to_use)
                
                # Check if the rating has changed by at least 1% or if it's a new entry
                old_normalized = latest_ratings_dict.get((player['Unique ID'], role), '0%')
                old_value = float(old_normalized.strip('%'))
                new_value = float(normalized.strip('%'))

                if abs(new_value - old_value) >= 1.0 or (player['Unique ID'], role) not in latest_ratings_dict:
                    ratings_to_insert.append(
                        (player['Unique ID'], role, absolute, normalized, timestamp)
                    )

    if ratings_to_insert:
        # We use a simple INSERT here to add a new historical record.
        cursor.executemany(
            "INSERT INTO dwrs_ratings (unique_id, role, dwrs_absolute, dwrs_normalized, timestamp) VALUES (?, ?, ?, ?, ?)",
            ratings_to_insert
        )

    conn.commit()
    conn.close()

@st.cache_data
def get_all_players():
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM players')
    rows = cursor.fetchall()
    
    # Get column names directly from the cursor description.
    # This guarantees the names are in the same order as the data in each row.
    columns = [description[0] for description in cursor.description]

    players = []
    for row in rows:
        player = dict(zip(columns, row))
        try:
            player['Assigned Roles'] = ast.literal_eval(player['Assigned Roles']) if player['Assigned Roles'] else []
        except (ValueError, SyntaxError):
            player['Assigned Roles'] = []

        try:
            # Safely parse the 'natural_positions' string back into a list
            player['natural_positions'] = ast.literal_eval(player.get('natural_positions', '[]'))
            if not isinstance(player['natural_positions'], list):
                player['natural_positions'] = []
        except (ValueError, SyntaxError):
            player['natural_positions'] = []

        players.append(player)
    conn.close()
    return players if players else []

def get_user_club():
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT value FROM settings WHERE key = "user_club"')
    result = cursor.fetchone()
    conn.close()
    return result[0] if result else None

def set_user_club(club):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ("user_club", club))
    conn.commit()
    conn.close()

def get_second_team_club():
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT value FROM settings WHERE key = "second_team_club"')
    result = cursor.fetchone()
    conn.close()
    return result[0] if result else None

def set_second_team_club(club):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ("second_team_club", club))
    conn.commit()
    conn.close()

def get_dwrs_history(unique_ids, role=None):
    if not unique_ids: return pd.DataFrame()
    conn = connect_db()
    placeholders = ','.join(['?'] * len(unique_ids))

    # --- NEW SQL LOGIC ---
    # We use DENSE_RANK() as a window function. It assigns a rank (which we'll call a "Snapshot Number")
    # to each unique timestamp, partitioned by player and role. This correctly numbers the progression
    # for each player's specific role history.
    base_query = f"""
        SELECT 
            unique_id, 
            role, 
            dwrs_normalized, 
            timestamp,
            DENSE_RANK() OVER (PARTITION BY unique_id, role ORDER BY timestamp) as snapshot
        FROM dwrs_ratings 
        WHERE unique_id IN ({placeholders})
    """
    
    if role and role != "All Roles":
        query = base_query + " AND role = ? ORDER BY unique_id, role, timestamp"
        params = unique_ids + [role]
    else:
        query = base_query + " ORDER BY unique_id, role, timestamp"
        params = unique_ids
    # --- END NEW SQL LOGIC ---

    df = pd.read_sql_query(query, conn, params=params)
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    conn.close()
    return df

def get_favorite_tactics():
    """Fetches the user's primary and secondary favorite tactics."""
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT value FROM settings WHERE key = "favorite_tactic_1"')
    tactic1_result = cursor.fetchone()
    cursor.execute('SELECT value FROM settings WHERE key = "favorite_tactic_2"')
    tactic2_result = cursor.fetchone()
    conn.close()
    
    tactic1 = tactic1_result[0] if tactic1_result else None
    tactic2 = tactic2_result[0] if tactic2_result else None
    return tactic1, tactic2

def set_favorite_tactics(tactic1, tactic2):
    """Saves the user's favorite tactics to the database."""
    conn = connect_db()
    cursor = conn.cursor()
    
    # Handle Tactic 1
    if tactic1 and tactic1 != "None":
        cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ("favorite_tactic_1", tactic1))
    else:
        cursor.execute('DELETE FROM settings WHERE key = "favorite_tactic_1"')
        
    # Handle Tactic 2
    if tactic2 and tactic2 != "None":
        cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ("favorite_tactic_2", tactic2))
    else:
        cursor.execute('DELETE FROM settings WHERE key = "favorite_tactic_2"')
        
    conn.commit()
    conn.close()

def update_player_transfer_status(unique_id, status):
    conn = connect_db()
    cursor = conn.cursor()
    # status is a boolean (True/False), so we store it as 1 or 0
    cursor.execute('UPDATE players SET transfer_status = ? WHERE "Unique ID" = ?', (1 if status else 0, unique_id))
    conn.commit()
    conn.close()

def update_player_loan_status(unique_id, status):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('UPDATE players SET loan_status = ? WHERE "Unique ID" = ?', (1 if status else 0, unique_id))
    conn.commit()
    conn.close()

def update_player_natural_positions(unique_id, positions):
    """Saves the list of natural positions for a player."""
    conn = connect_db()
    cursor = conn.cursor()
    # Convert the list to a string representation to store in the TEXT column
    positions_str = str(positions)
    cursor.execute('UPDATE players SET natural_positions = ? WHERE "Unique ID" = ?', (positions_str, unique_id))
    conn.commit()
    conn.close()

def get_club_identity():
    """Fetches the full club name and stadium name from the settings."""
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT value FROM settings WHERE key = "full_club_name"')
    full_name_result = cursor.fetchone()
    cursor.execute('SELECT value FROM settings WHERE key = "stadium_name"')
    stadium_name_result = cursor.fetchone()
    conn.close()
    
    full_name = full_name_result[0] if full_name_result else None
    stadium_name = stadium_name_result[0] if stadium_name_result else None
    return full_name, stadium_name

def set_club_identity(full_name, stadium_name):
    """Saves the full club name and stadium name to the settings."""
    conn = connect_db()
    cursor = conn.cursor()
    
    if full_name:
        cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ("full_club_name", full_name))
    else:
        cursor.execute('DELETE FROM settings WHERE key = "full_club_name"')
        
    if stadium_name:
        cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ("stadium_name", stadium_name))
    else:
        cursor.execute('DELETE FROM settings WHERE key = "stadium_name"')
        
    conn.commit()
    conn.close()