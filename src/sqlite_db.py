# sqlite_db.py

import sqlite3
from datetime import datetime
import pandas as pd
from constants import attribute_mapping
import ast
from config_handler import get_db_file
import streamlit as st

def connect_db():
    return sqlite3.connect(get_db_file())

def init_db():
    conn = connect_db()
    cursor = conn.cursor()
    
    columns = ", ".join([f'"{col}" TEXT' for col in attribute_mapping.values()])
    cursor.execute(f"""
        CREATE TABLE IF NOT EXISTS players (
            "Unique ID" TEXT PRIMARY KEY, {columns}, "Assigned Roles" TEXT
        )
    """)
    
    cursor.execute("PRAGMA table_info(players)")
    existing_columns = [col[1] for col in cursor.fetchall()]

    if "Team Roles" in existing_columns and "Rushing Out (Tendency)" not in existing_columns:
        cursor.execute('ALTER TABLE players RENAME COLUMN "Team Roles" TO "Rushing Out (Tendency)"')
    
    if "primary_role" not in existing_columns:
        cursor.execute('ALTER TABLE players ADD COLUMN "primary_role" TEXT')

    if "agreed_playing_time" not in existing_columns:
        cursor.execute('ALTER TABLE players ADD COLUMN "agreed_playing_time" TEXT')
    
    cursor.execute("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)")
    cursor.execute("UPDATE settings SET key = 'second_team_club' WHERE key = 'youth_club'")
    
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS dwrs_ratings (
            unique_id TEXT, role TEXT, dwrs_absolute REAL, dwrs_normalized TEXT, timestamp TEXT,
            PRIMARY KEY (unique_id, role, timestamp)
        )
    """)
    
    conn.commit()
    conn.close()

def update_player_apt(unique_id, playing_time):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('UPDATE players SET agreed_playing_time = ? WHERE "Unique ID" = ?', (playing_time, unique_id))
    conn.commit()
    conn.close()

def update_player(mapped_player_data):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT "Assigned Roles" FROM players WHERE "Unique ID" = ?', (mapped_player_data['Unique ID'],))
    result = cursor.fetchone()
    columns = list(attribute_mapping.values()) + ['Assigned Roles']
    values = [mapped_player_data.get(col, '') for col in attribute_mapping.values()] + ['']
    if result:
        values[-1] = result[0] or '[]'
        set_clause = ", ".join([f'"{col}" = ?' for col in columns])
        cursor.execute(f'UPDATE players SET {set_clause} WHERE "Unique ID" = ?', values + [mapped_player_data['Unique ID']])
    else:
        columns.insert(0, "Unique ID")
        values.insert(0, mapped_player_data['Unique ID'])
        placeholders = ", ".join(["?" for _ in columns])
        cursor.execute(f'INSERT INTO players ({", ".join([f'"{col}"' for col in columns])}) VALUES ({placeholders})', values)
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
    cursor.execute("""
        SELECT unique_id, role, dwrs_normalized FROM dwrs_ratings
        WHERE (unique_id, role, timestamp) IN (
            SELECT unique_id, role, MAX(timestamp) FROM dwrs_ratings GROUP BY unique_id, role
        )
    """)
    dwrs_dict = {(row[0], row[1]): row[2] for row in cursor.fetchall()}
    for _, player in df.iterrows():
        player_dict = player.to_dict()
        roles = player_dict.get('Assigned Roles', [])
        if not isinstance(roles, list): roles = []
        for role in roles:
            if role in valid_roles:
                weights_to_use = gk_weights if role in all_gk_roles else weights
                absolute, normalized = calculate_dwrs(player_dict, role, weights_to_use)
                old_normalized = dwrs_dict.get((player['Unique ID'], role), '0%')
                old_value = float(old_normalized.strip('%')) if isinstance(old_normalized, str) and old_normalized.endswith('%') else 0.0
                new_value = float(normalized.strip('%'))
                if abs(new_value - old_value) >= 1.0 or (player['Unique ID'], role) not in dwrs_dict:
                    cursor.execute("INSERT OR REPLACE INTO dwrs_ratings (unique_id, role, dwrs_absolute, dwrs_normalized, timestamp) VALUES (?, ?, ?, ?, ?)",
                                   (player['Unique ID'], role, absolute, normalized, timestamp))
    conn.commit()
    conn.close()

@st.cache_data
def get_all_players():
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM players')
    rows = cursor.fetchall()
    columns = ["Unique ID"] + list(attribute_mapping.values()) + ["Assigned Roles", "primary_role", "agreed_playing_time"]
    players = []
    for row in rows:
        player = dict(zip(columns, row))
        try:
            player['Assigned Roles'] = ast.literal_eval(player['Assigned Roles']) if player['Assigned Roles'] else []
        except (ValueError, SyntaxError):
            player['Assigned Roles'] = []
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

def get_dwrs_history(unique_ids, role=None):
    if not unique_ids: return pd.DataFrame()
    conn = connect_db()
    placeholders = ','.join(['?'] * len(unique_ids))
    if role and role != "All Roles":
        query = f"SELECT * FROM dwrs_ratings WHERE unique_id IN ({placeholders}) AND role = ? ORDER BY unique_id, timestamp"
        params = unique_ids + [role]
    else:
        query = f"SELECT * FROM dwrs_ratings WHERE unique_id IN ({placeholders}) ORDER BY unique_id, role, timestamp"
        params = unique_ids
    df = pd.read_sql_query(query, conn, params=params)
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    conn.close()
    return df