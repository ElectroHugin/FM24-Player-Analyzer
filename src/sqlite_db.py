# src/sqlite_db.py

import sqlite3
from datetime import datetime
import pandas as pd
import ast
import streamlit as st
import shutil
import os

from definitions_loader import PROJECT_ROOT
from constants import attribute_mapping, get_valid_roles
from config_handler import get_db_file

def connect_db():
    return sqlite3.connect(get_db_file())



def init_db():
    conn = connect_db()
    cursor = conn.cursor()
    
    # --- MIGRATION BLOCK 1: Players Table - Creation & Migration ---
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='players'")
    table_exists = cursor.fetchone()

    if not table_exists:
        # SCENARIO A (First Run): Table doesn't exist, create it perfectly.
        cursor.execute("CREATE TABLE players (\"Unique ID\" TEXT PRIMARY KEY, \"Assigned Roles\" TEXT)")
    else:
        # SCENARIO B (Upgrade Run): Table exists, check if it needs fixing.
        cursor.execute("PRAGMA table_info(players)")
        columns_info = cursor.fetchall()
        pk_info = next((col for col in columns_info if col[1] == 'Unique ID'), None)
        if not pk_info or pk_info[5] != 1: # If 'Unique ID' is not the primary key
            st.warning("Outdated player table detected. Performing a safe, one-time cleanup...")
            with st.spinner("Deduplicating players and rebuilding table structure..."):
                try:
                    cursor.execute("ALTER TABLE players RENAME TO players_old")
                    cursor.execute("CREATE TABLE players (\"Unique ID\" TEXT PRIMARY KEY, \"Assigned Roles\" TEXT)")
                    cursor.execute("PRAGMA table_info(players_old)")
                    old_columns = [col[1] for col in cursor.fetchall()]
                    col_str = ", ".join([f'"{c}"' for c in old_columns])
                    cursor.execute(f"INSERT INTO players ({col_str}) SELECT {col_str} FROM players_old WHERE ROWID IN (SELECT ROWID FROM (SELECT ROWID, ROW_NUMBER() OVER(PARTITION BY \"Unique ID\" ORDER BY CASE WHEN \"Assigned Roles\" IS NOT NULL AND \"Assigned Roles\" != '[]' THEN 0 ELSE 1 END, ROWID DESC) as rn FROM players_old) WHERE rn = 1)")
                    cursor.execute("DROP TABLE players_old")
                    conn.commit()
                    st.toast("Player table rebuilt successfully.", icon="✅")
                except Exception as e:
                    cursor.execute("DROP TABLE IF EXISTS players")
                    cursor.execute("ALTER TABLE players_old RENAME TO players")
                    st.error(f"Database cleanup failed: {e}. Your original data has been restored.")
                    st.stop()

    # --- MIGRATION BLOCK 2: Add All Missing Columns to Players Table ---
    # This block is now safe because the players table is guaranteed to exist correctly.
    cursor.execute("PRAGMA table_info(players)")
    existing_columns = {col[1] for col in cursor.fetchall()}
    all_player_columns = set(attribute_mapping.values()) | {"primary_role", "natural_positions", "transfer_status", "loan_status", "Nationality", "Second Nationality", "preferred_side"}
    for col_name in all_player_columns:
        if col_name not in existing_columns:
            try:
                cursor.execute(f'ALTER TABLE players ADD COLUMN "{col_name}" TEXT')
            except sqlite3.OperationalError: pass
    if "agreed_playing_time" in existing_columns:
        try:
            cursor.execute(f'UPDATE players SET "Agreed Playing Time" = agreed_playing_time WHERE "Agreed Playing Time" IS NULL')
            cursor.execute('ALTER TABLE players DROP COLUMN "agreed_playing_time"')
        except sqlite3.OperationalError: pass

    # --- MIGRATION BLOCK 3: Create Settings Table ---
    cursor.execute("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)")

    # Create the table to store the IDs of players called up to the national squad.
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS national_squad (
            player_unique_id TEXT PRIMARY KEY
        )
    """)

    # Create the table to store the user's shortlist of players.
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS shortlist (
        player_unique_id TEXT PRIMARY KEY
    )
""")

    # --- MIGRATION BLOCK 4: Merge Newgen Duplicates ---
    # This logic is safe and unchanged.
    cursor.execute("SELECT value FROM settings WHERE key = 'newgen_merge_v1_complete'")
    if not cursor.fetchone():
        st.info("Performing a one-time merge of duplicate newgen players...")
        with st.spinner("Checking for and merging records..."):
            cursor.execute("SELECT \"Unique ID\", Name, \"Assigned Roles\", \"Agreed Playing Time\", primary_role, natural_positions FROM players WHERE \"Unique ID\" NOT LIKE 'r-%'")
            numeric_players = {row[0]: {'Name': row[1], 'Assigned Roles': row[2], 'APT': row[3], 'Primary Role': row[4], 'Natural Pos': row[5]} for row in cursor.fetchall()}
            cursor.execute("SELECT \"Unique ID\", Name FROM players WHERE \"Unique ID\" LIKE 'r-%'")
            r_players = {row[0]: row[1] for row in cursor.fetchall()}
            
            ids_to_delete, updates_to_make = [], []
            for r_id, r_name in r_players.items():
                numeric_part = r_id[2:]
                if numeric_part in numeric_players and numeric_players[numeric_part]['Name'] == r_name:
                    ids_to_delete.append(numeric_part)
                    valuable_data = numeric_players[numeric_part]
                    updates_to_make.append((valuable_data['Assigned Roles'], valuable_data['APT'], valuable_data['Primary Role'], valuable_data['Natural Pos'], r_id))
            
            if updates_to_make:
                cursor.executemany('UPDATE players SET "Assigned Roles"=?, "Agreed Playing Time"=?, primary_role=?, natural_positions=? WHERE "Unique ID"=?', updates_to_make)
            if ids_to_delete:
                placeholders = ', '.join('?' for _ in ids_to_delete)
                cursor.execute(f'DELETE FROM players WHERE "Unique ID" IN ({placeholders})', ids_to_delete)
                cursor.execute(f'DELETE FROM dwrs_ratings WHERE unique_id IN ({placeholders})', ids_to_delete)
                st.toast(f"Merged {len(ids_to_delete)} duplicate newgen records.", icon="✨")

            cursor.execute("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)", ('newgen_merge_v1_complete', 'true'))
            conn.commit()

    # --- MIGRATION BLOCK 5: DWRS Ratings Table - Creation & Migration ---
    correct_dwrs_schema = "CREATE TABLE dwrs_ratings (unique_id TEXT, role TEXT, dwrs_absolute REAL, dwrs_normalized TEXT, timestamp TEXT, PRIMARY KEY (unique_id, role, timestamp))"
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='dwrs_ratings'")
    table_exists = cursor.fetchone()
    
    if not table_exists:
        # SCENARIO A (First Run): Table doesn't exist, create it perfectly.
        cursor.execute(correct_dwrs_schema)
    else:
        # SCENARIO B (Upgrade Run): Table exists, check if it needs fixing.
        cursor.execute("PRAGMA table_info(dwrs_ratings)")
        columns_info = cursor.fetchall()
        pk_count = sum(1 for col in columns_info if col[5] > 0)
        column_names = {col[1] for col in columns_info}
        if pk_count != 3 or "dwrs_absolute" not in column_names:
            st.warning("Outdated ratings table detected. Performing safe upgrade...")
            with st.spinner("Preserving historical data and rebuilding ratings table..."):
                try:
                    cursor.execute("ALTER TABLE dwrs_ratings RENAME TO dwrs_ratings_old")
                    cursor.execute(correct_dwrs_schema)
                    cursor.execute("PRAGMA table_info(dwrs_ratings_old)")
                    old_columns = [col[1] for col in cursor.fetchall()]
                    if "dwrs_absolute" in old_columns:
                        cursor.execute("INSERT INTO dwrs_ratings SELECT * FROM dwrs_ratings_old")
                    else:
                        cursor.execute("INSERT INTO dwrs_ratings (unique_id, role, dwrs_absolute, dwrs_normalized, timestamp) SELECT unique_id, role, 0.0, dwrs_normalized, timestamp FROM dwrs_ratings_old")
                    cursor.execute("DROP TABLE dwrs_ratings_old")
                    conn.commit()
                    df = pd.DataFrame(get_all_players())
                    if not df.empty:
                        update_dwrs_ratings(df, get_valid_roles())
                    st.cache_data.clear()
                    st.success("Database upgrade successful! The app will now reload.")
                    st.rerun()
                except Exception as e:
                    cursor.execute("DROP TABLE IF EXISTS dwrs_ratings")
                    cursor.execute("ALTER TABLE dwrs_ratings_old RENAME TO dwrs_ratings")
                    st.error(f"Database upgrade failed: {e}. Your original data has been restored.")
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
    if not uid:
        conn.close()
        return

    cursor.execute('SELECT "Unique ID" FROM players WHERE "Unique ID" = ?', (uid,))
    result = cursor.fetchone()

    # --- START OF NEW, ROBUST LOGIC ---
    
    # These are columns managed *within the app* and should NOT be overwritten by an HTML import.
    APP_MANAGED_COLUMNS = [
        "Assigned Roles", 
        "primary_role", 
        "natural_positions", 
        "transfer_status", 
        "loan_status"
    ]

    # Create a clean dictionary of only the columns to update from the HTML file.
    # We explicitly exclude the app-managed columns.
    data_to_update = {
        k: v for k, v in mapped_player_data.items() 
        if k != 'Unique ID' and k not in APP_MANAGED_COLUMNS
    }

    if result:
        # PLAYER EXISTS: Build a dynamic UPDATE statement.
        if data_to_update:
            set_clauses = [f'"{col}" = ?' for col in data_to_update.keys()]
            values = list(data_to_update.values())
            values.append(uid)

            query = f'UPDATE players SET {", ".join(set_clauses)} WHERE "Unique ID" = ?'
            cursor.execute(query, values)
    else:
        # PLAYER IS NEW: Build a dynamic INSERT statement.
        columns = ['"Unique ID"']
        values = [uid]

        columns.extend([f'"{col}"' for col in data_to_update.keys()])
        values.extend(data_to_update.values())

        # Explicitly set "Assigned Roles" to an empty list for brand new players.
        columns.append('"Assigned Roles"')
        values.append('[]')

        placeholders = ", ".join(["?" for _ in columns])
        query = f'INSERT INTO players ({", ".join(columns)}) VALUES ({placeholders})'
        cursor.execute(query, values)

    # --- END OF NEW, ROBUST LOGIC ---

    conn.commit()
    conn.close()

def bulk_upsert_players(players_data):
    """
    Performs a high-performance bulk "upsert" (update or insert) of player data.
    This is dramatically faster than single-row operations.
    """
    if not players_data:
        return

    conn = connect_db()
    cursor = conn.cursor()

    # 1. Get all existing player IDs from the DB in one query for fast checking
    cursor.execute('SELECT "Unique ID" FROM players')
    existing_ids = {row[0] for row in cursor.fetchall()}

    # 2. Separate players into two lists: those to update and those to insert
    players_to_update = []
    players_to_insert = []

    for player in players_data:
        if player.get('Unique ID') in existing_ids:
            players_to_update.append(player)
        else:
            players_to_insert.append(player)

    # 3. Perform bulk UPDATE for existing players
    if players_to_update:
        # Get the columns to update from the first player dict (assumes they are all the same)
        update_cols = [col for col in players_to_update[0].keys() if col != 'Unique ID']
        set_clauses = ", ".join([f'"{col}" = ?' for col in update_cols])
        
        # Prepare data as a list of tuples for executemany
        update_data = [
            tuple(p[col] for col in update_cols) + (p['Unique ID'],)
            for p in players_to_update
        ]
        
        query = f'UPDATE players SET {set_clauses} WHERE "Unique ID" = ?'
        cursor.executemany(query, update_data)

    # 4. Perform bulk INSERT for new players
    if players_to_insert:
        # Get columns from the first player and ensure 'Assigned Roles' is included
        insert_cols = list(players_to_insert[0].keys())
        if 'Assigned Roles' not in insert_cols:
            insert_cols.append('Assigned Roles')

        col_names_str = ", ".join([f'"{col}"' for col in insert_cols])
        placeholders_str = ", ".join(["?" for _ in insert_cols])
        
        # Prepare data, adding an empty list for 'Assigned Roles' for new players
        insert_data = [
            tuple(p.get(col, '[]') for col in insert_cols)
            for p in players_to_insert
        ]

        query = f'INSERT INTO players ({col_names_str}) VALUES ({placeholders_str})'
        cursor.executemany(query, insert_data)

    # 5. Commit the entire transaction ONCE and close
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

def update_dwrs_ratings(df, valid_roles, player_ids_to_update=None):
    from analytics import build_attribute_matrix, calculate_dwrs_role_batch
    from config_handler import get_weight
    from constants import WEIGHT_DEFAULTS, GK_WEIGHT_DEFAULTS, get_gk_roles
    import numpy as np

    conn = connect_db()
    cursor = conn.cursor()

    if player_ids_to_update:
        # We only work with the subset of players who were in the last import
        df_to_process = df[df['Unique ID'].isin(player_ids_to_update)]
    else:
        # If no IDs are provided, fall back to the old behavior (process everyone)
        df_to_process = df

    if df_to_process.empty:
        conn.close()
        return

    weights = {cat: get_weight(cat.lower().replace(" ", "_"), default) for cat, default in WEIGHT_DEFAULTS.items()}
    gk_weights = {cat: get_weight("gk_" + cat.lower().replace(" ", "_"), default) for cat, default in GK_WEIGHT_DEFAULTS.items()}
    all_gk_roles = set(get_gk_roles())
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # Get the latest existing rating for every player/role to compare against
    cursor.execute("""
        SELECT unique_id, role, dwrs_normalized FROM dwrs_ratings
        WHERE (unique_id, role, timestamp) IN (
            SELECT unique_id, role, MAX(timestamp) FROM dwrs_ratings GROUP BY unique_id, role
        )
    """)
    latest_ratings_dict = {(row[0], row[1]): row[2] for row in cursor.fetchall()}

    # --- Vectorized recalculation ---
    # Parse every attribute column once, group player rows by assigned role,
    # then compute each role's ratings for all its players in one numpy pass.
    # (The old per-player loop took ~12 minutes on an 80k-player database.)
    df_to_process = df_to_process.reset_index(drop=True)
    attr_matrix = build_attribute_matrix(df_to_process)
    uids = df_to_process['Unique ID'].tolist()

    valid_roles_set = set(valid_roles)
    role_to_rows = {}
    for i, roles in enumerate(df_to_process['Assigned Roles']):
        if not isinstance(roles, list):
            continue
        for role in roles:
            if role in valid_roles_set:
                role_to_rows.setdefault(role, []).append(i)

    ratings_to_insert = []
    for role, row_list in role_to_rows.items():
        rows = np.asarray(row_list, dtype=np.intp)
        weights_to_use = gk_weights if role in all_gk_roles else weights
        absolute_arr, normalized_arr = calculate_dwrs_role_batch(attr_matrix, role, weights_to_use, rows)

        for j, i in enumerate(row_list):
            uid = uids[i]
            new_value = normalized_arr[j]
            # Insert only if it's a new entry or changed by at least 1%
            old_normalized = latest_ratings_dict.get((uid, role))
            if old_normalized is None or abs(new_value - float(old_normalized.strip('%'))) >= 1.0:
                ratings_to_insert.append(
                    (uid, role, float(absolute_arr[j]), f"{new_value:.0f}%", timestamp)
                )

    if ratings_to_insert:
        # We use a simple INSERT here to add a new historical record.
        cursor.executemany(
            "INSERT INTO dwrs_ratings (unique_id, role, dwrs_absolute, dwrs_normalized, timestamp) VALUES (?, ?, ?, ?, ?)",
            ratings_to_insert
        )

    conn.commit()
    conn.close()

def _parse_list_column(value, cache):
    """Parse a stored list string (e.g. \"['CD-D', 'DM-S']\") into a list.
    The same strings repeat thousands of times across a big database, so
    results are memoized; each caller gets its own shallow copy."""
    if not value:
        return []
    cached = cache.get(value)
    if cached is None:
        try:
            parsed = ast.literal_eval(value)
            cached = parsed if isinstance(parsed, list) else []
        except (ValueError, SyntaxError):
            cached = []
        cache[value] = cached
    return list(cached)

@st.cache_data
def get_all_players():
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM players')
    rows = cursor.fetchall()

    # Get column names directly from the cursor description.
    # This guarantees the names are in the same order as the data in each row.
    columns = [description[0] for description in cursor.description]

    eval_cache = {}
    players = []
    for row in rows:
        player = dict(zip(columns, row))
        player['Assigned Roles'] = _parse_list_column(player.get('Assigned Roles'), eval_cache)
        player['natural_positions'] = _parse_list_column(player.get('natural_positions'), eval_cache)
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


# Shared subquery: each player's best CURRENT rating — the latest rating per
# (player, role), then the max over those roles. This matches what the matrix
# pages display. The old version took the max over the ENTIRE history, so a
# player who once peaked above the threshold could never be pruned again.
_LATEST_BEST_DWRS_SUBQUERY = """
    SELECT t1.unique_id, MAX(CAST(REPLACE(t1.dwrs_normalized, '%', '') AS REAL)) as max_dwrs
    FROM dwrs_ratings t1
    INNER JOIN (
        SELECT unique_id, role, MAX(timestamp) as max_ts
        FROM dwrs_ratings
        GROUP BY unique_id, role
    ) t2 ON t1.unique_id = t2.unique_id AND t1.role = t2.role AND t1.timestamp = t2.max_ts
    GROUP BY t1.unique_id
"""

def _build_prune_query(select_clause, dwrs_threshold, national_code_to_protect):
    """
    One query builder for both the preview and the actual deletion, so the two
    can never disagree about who is prunable.

    NULL-safety matters here: `p."Second Nationality" != 'GER'` evaluates to
    NULL (not TRUE) for players without a second nationality, which silently
    protected almost every player from pruning. IFNULL(...) fixes that.
    Players in the national squad or on the shortlist are always protected —
    they were hand-picked by the user.
    """
    user_club = get_user_club()
    second_team_club = get_second_team_club()

    query = f"""
        SELECT {select_clause}
        FROM players p
        LEFT JOIN ({_LATEST_BEST_DWRS_SUBQUERY}) dr ON p."Unique ID" = dr.unique_id
        WHERE IFNULL(p.Club, '') NOT IN (?, ?)
        AND p."Unique ID" NOT IN (SELECT player_unique_id FROM national_squad)
        AND p."Unique ID" NOT IN (SELECT player_unique_id FROM shortlist)
    """
    params = [user_club or '', second_team_club or '']

    if national_code_to_protect:
        query += ' AND IFNULL(p.Nationality, \'\') != ? AND IFNULL(p."Second Nationality", \'\') != ?'
        params.extend([national_code_to_protect, national_code_to_protect])

    query += " AND IFNULL(dr.max_dwrs, 0) < ?"
    params.append(dwrs_threshold)
    return query, params

def get_prunable_player_info(dwrs_threshold, national_code_to_protect=None):
    """
    Calculates how many scouted players are eligible for deletion and the best
    current rating among them. Uses exactly the same query as the deletion.
    """
    query, params = _build_prune_query(
        'p."Unique ID", IFNULL(dr.max_dwrs, 0) as max_dwrs',
        dwrs_threshold, national_code_to_protect
    )
    conn = connect_db()
    df = pd.read_sql_query(query, conn, params=params)
    conn.close()

    count = len(df)
    max_rating_prunable = df['max_dwrs'].max() if count > 0 else 0
    return count, max_rating_prunable

def prune_scouted_players(dwrs_threshold, national_code_to_protect=None):
    """
    Finds and permanently deletes scouted players whose best CURRENT role
    rating is below the threshold. Shares its query with
    get_prunable_player_info, so the preview always matches the deletion.
    """
    conn = connect_db()
    cursor = conn.cursor()

    query, params = _build_prune_query('p."Unique ID"', dwrs_threshold, national_code_to_protect)
    cursor.execute(query, params)
    ids_to_delete = [row[0] for row in cursor.fetchall()]
    
    if not ids_to_delete:
        conn.close()
        return 0

    # Batch deletion logic remains the same
    BATCH_SIZE = 900
    try:
        for i in range(0, len(ids_to_delete), BATCH_SIZE):
            batch_ids = ids_to_delete[i:i + BATCH_SIZE]
            placeholders = ', '.join('?' for _ in batch_ids)
            cursor.execute(f'DELETE FROM players WHERE "Unique ID" IN ({placeholders})', batch_ids)
            cursor.execute(f'DELETE FROM dwrs_ratings WHERE unique_id IN ({placeholders})', batch_ids)
        conn.commit()
    except Exception as e:
        conn.rollback()
        st.error(f"An error occurred during deletion: {e}")
        return -1
    finally:
        conn.close()
        
    return len(ids_to_delete)

def create_database_backup():
    """
    Creates a timestamped backup of the current database file.
    Manages a rotation to keep only the 3 most recent backups.
    """
    db_file = get_db_file()
    if not os.path.exists(db_file):
        return # Nothing to back up

    # 1. Define paths and create the backup folder
    db_name_part = os.path.splitext(os.path.basename(db_file))[0]
    backup_folder = os.path.join(PROJECT_ROOT, 'backups')
    os.makedirs(backup_folder, exist_ok=True)

    # 2. Create a timestamped backup filename
    timestamp = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
    backup_filename = f"{db_name_part}_backup_{timestamp}.db"
    backup_filepath = os.path.join(backup_folder, backup_filename)

    # 3. Copy the current database to the backup location
    try:
        shutil.copy2(db_file, backup_filepath)
        st.toast(f"Database backup created: {backup_filename}", icon="💾")
    except Exception as e:
        st.warning(f"Could not create database backup. Error: {e}")
        return # Stop if backup fails

    # 4. Clean up old backups (rotation)
    try:
        # Get all backup files for the current database, sorted oldest to newest
        all_backups = sorted([
            f for f in os.listdir(backup_folder) 
            if f.startswith(f"{db_name_part}_backup_") and f.endswith(".db")
        ])
        
        # If we have more than 3 backups, delete the oldest ones
        while len(all_backups) > 3:
            oldest_backup = all_backups.pop(0) # Get the first (oldest) file
            os.remove(os.path.join(backup_folder, oldest_backup))
    except Exception as e:
        st.warning(f"Could not clean up old backups. Error: {e}")

def get_national_team_settings():
    """Fetches the national team's details from the settings table."""
    conn = connect_db()
    cursor = conn.cursor()
    settings = {}
    keys = ['national_team_name', 'national_team_country_code', 'national_team_age_limit']
    for key in keys:
        cursor.execute('SELECT value FROM settings WHERE key = ?', (key,))
        result = cursor.fetchone()
        settings[key] = result[0] if result else None
    conn.close()
    return settings['national_team_name'], settings['national_team_country_code'], settings['national_team_age_limit']

def set_national_team_settings(name, country_code, age_limit):
    """Saves the national team's details to the settings table."""
    conn = connect_db()
    cursor = conn.cursor()
    settings = {
        "national_team_name": name,
        "national_team_country_code": country_code,
        "national_team_age_limit": age_limit
    }
    for key, value in settings.items():
        if value:
            cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', (key, str(value)))
        else:
            cursor.execute('DELETE FROM settings WHERE key = ?', (key,))
    conn.commit()
    conn.close()

def get_national_squad_ids():
    """Fetches a set of all player IDs currently in the national squad."""
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT player_unique_id FROM national_squad')
    # Return as a set for very fast 'in' checks
    ids = {row[0] for row in cursor.fetchall()}
    conn.close()
    return ids

def set_national_squad_ids(player_ids):
    """
    Clears the national squad and saves a new list of player IDs.
    'player_ids' should be a list or set of strings.
    """
    conn = connect_db()
    cursor = conn.cursor()
    # Perform in a transaction for safety
    try:
        cursor.execute("DELETE FROM national_squad")
        if player_ids:
            # executemany expects a list of tuples
            data_to_insert = [(pid,) for pid in player_ids]
            cursor.executemany("INSERT INTO national_squad (player_unique_id) VALUES (?)", data_to_insert)
        conn.commit()
    except Exception as e:
        conn.rollback()
        st.error(f"Failed to update national squad: {e}")
    finally:
        conn.close()

def get_national_mode_enabled():
    """Checks if the national team management mode is enabled."""
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute("SELECT value FROM settings WHERE key = 'national_mode_enabled'")
    result = cursor.fetchone()
    conn.close()
    # Default to False if the setting doesn't exist
    return result[0] == 'true' if result else False

def set_national_mode_enabled(is_enabled):
    """Saves the state of the national team management mode."""
    conn = connect_db()
    cursor = conn.cursor()
    # Store the boolean as a string 'true' or 'false'
    value_to_save = 'true' if is_enabled else 'false'
    cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ('national_mode_enabled', value_to_save))
    conn.commit()
    conn.close()

def get_national_favorite_tactics():
    """Fetches the user's primary and secondary favorite NATIONAL tactics."""
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT value FROM settings WHERE key = "national_fav_tactic_1"')
    tactic1_result = cursor.fetchone()
    cursor.execute('SELECT value FROM settings WHERE key = "national_fav_tactic_2"')
    tactic2_result = cursor.fetchone()
    conn.close()
    
    tactic1 = tactic1_result[0] if tactic1_result else None
    tactic2 = tactic2_result[0] if tactic2_result else None
    return tactic1, tactic2

def set_national_favorite_tactics(tactic1, tactic2):
    """Saves the user's favorite NATIONAL tactics to the database."""
    conn = connect_db()
    cursor = conn.cursor()
    
    # Handle Tactic 1
    if tactic1 and tactic1 != "None":
        cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ("national_fav_tactic_1", tactic1))
    else:
        cursor.execute('DELETE FROM settings WHERE key = "national_fav_tactic_1"')
        
    # Handle Tactic 2
    if tactic2 and tactic2 != "None":
        cursor.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)', ("national_fav_tactic_2", tactic2))
    else:
        cursor.execute('DELETE FROM settings WHERE key = "national_fav_tactic_2"')
        
    conn.commit()
    conn.close()

def merge_player_records(bad_id, good_id):
    """
    Safely merges a player record with a bad (numeric) ID into a good (r-) ID.

    Historical DWRS ratings are always migrated. App-managed data (assigned
    roles, primary role, natural positions, transfer/loan status, preferred
    side, APT) is preserved: if the good record does not exist yet, the bad
    record is simply renamed; if both exist, empty fields on the good record
    are filled from the bad one before it is deleted.
    """
    APP_MANAGED_MERGE_COLUMNS = [
        "Assigned Roles", "primary_role", "natural_positions",
        "transfer_status", "loan_status", "preferred_side", "Agreed Playing Time"
    ]
    conn = connect_db()
    cursor = conn.cursor()
    try:
        # Step 1: Migrate historical DWRS records. OR IGNORE avoids primary
        # key collisions when both IDs already have a rating for the same
        # role+timestamp; leftovers under the bad ID are then removed.
        cursor.execute(
            "UPDATE OR IGNORE dwrs_ratings SET unique_id = ? WHERE unique_id = ?",
            (good_id, bad_id)
        )
        cursor.execute("DELETE FROM dwrs_ratings WHERE unique_id = ?", (bad_id,))

        # Step 2: Merge the player rows without losing app-managed data.
        cursor.execute('SELECT 1 FROM players WHERE "Unique ID" = ?', (good_id,))
        good_exists = cursor.fetchone() is not None

        if not good_exists:
            # Rename keeps every column (roles, APT, statuses) intact.
            cursor.execute(
                'UPDATE players SET "Unique ID" = ? WHERE "Unique ID" = ?',
                (good_id, bad_id)
            )
        else:
            col_list = ", ".join(f'"{c}"' for c in APP_MANAGED_MERGE_COLUMNS)
            cursor.execute(f'SELECT {col_list} FROM players WHERE "Unique ID" = ?', (bad_id,))
            bad_row = cursor.fetchone()
            cursor.execute(f'SELECT {col_list} FROM players WHERE "Unique ID" = ?', (good_id,))
            good_row = cursor.fetchone()

            if bad_row and good_row:
                for col, bad_val, good_val in zip(APP_MANAGED_MERGE_COLUMNS, bad_row, good_row):
                    good_is_empty = good_val is None or good_val in ('', '[]')
                    bad_has_value = bad_val is not None and bad_val not in ('', '[]')
                    if good_is_empty and bad_has_value:
                        cursor.execute(
                            f'UPDATE players SET "{col}" = ? WHERE "Unique ID" = ?',
                            (bad_val, good_id)
                        )

            cursor.execute('DELETE FROM players WHERE "Unique ID" = ?', (bad_id,))

        conn.commit()
        st.toast(f"Unified records for ID {good_id}", icon="🔗")
    except sqlite3.Error as e:
        conn.rollback()
        st.error(f"Database error during player merge: {e}")
    finally:
        conn.close()

def get_shortlist_ids():
    """Fetches a set of all player IDs currently on the shortlist."""
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('SELECT player_unique_id FROM shortlist')
    ids = {row[0] for row in cursor.fetchall()}
    conn.close()
    return ids

def set_shortlist_ids(player_ids):
    """Clears the shortlist and saves a new list of player IDs."""
    conn = connect_db()
    cursor = conn.cursor()
    try:
        cursor.execute("DELETE FROM shortlist")
        if player_ids:
            data_to_insert = [(pid,) for pid in player_ids]
            cursor.executemany("INSERT INTO shortlist (player_unique_id) VALUES (?)", data_to_insert)
        conn.commit()
    except Exception as e:
        conn.rollback()
        st.error(f"Failed to update shortlist: {e}")
    finally:
        conn.close()

def update_player_preferred_side(unique_id, side):
    """Saves the preferred side ('Left', 'Right', or None) for a player."""
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute('UPDATE players SET preferred_side = ? WHERE "Unique ID" = ?', (side, unique_id))
    conn.commit()
    conn.close()