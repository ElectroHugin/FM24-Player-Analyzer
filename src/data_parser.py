# data_parser.py

import pandas as pd
from bs4 import BeautifulSoup
from constants import (attribute_mapping, get_valid_roles, ROLE_ANALYSIS_COLUMNS, 
                     PLAYER_ROLE_MATRIX_COLUMNS, WEIGHT_DEFAULTS, GK_WEIGHT_DEFAULTS)
from sqlite_db import init_db, update_player, get_all_players
from analytics import calculate_dwrs
from config_handler import get_weight
import streamlit as st

REQUIRED_COLUMN = "UID"

def parse_html_table(file):
    try:
        content = file.read().decode('utf-8')
        soup = BeautifulSoup(content, 'html.parser')
        table = soup.find('table')
        if not table: return None
        # Find the first row of the table specifically, and get headers from it.
        # This is more robust than searching the whole table for <th> tags.
        header_row = table.find('tr')
        if not header_row: return None
        headers = [th.text.strip() for th in header_row.find_all('th')]
        if not headers or REQUIRED_COLUMN not in headers or len(headers) < 10: return None
        
        rows = [[td.text.strip() for td in tr.find_all('td')] for tr in table.find_all('tr')[1:]]
        valid_rows = [row for row in rows if len(row) == len(headers)]
        if not valid_rows: return None
        
        return pd.DataFrame(valid_rows, columns=headers)
    except Exception as e:
        print(f"Error reading HTML file: {e}")
        return None

def load_data():
    init_db()
    players = get_all_players()
    return pd.DataFrame(players) if players else None

def parse_and_update_data(file):
    html_df = parse_html_table(file)
    if html_df is None: return None
    init_db()
    for _, row in html_df.iterrows():
        player_data = row.to_dict()
        mapped_player_data = {attribute_mapping.get(k, k): v for k, v in player_data.items()}
        mapped_player_data['Unique ID'] = player_data['UID']
        update_player(mapped_player_data)
    return load_data()

def get_filtered_players(filter_option="Unassigned Players", club_filter="All", position_filter="All", sort_column="Name", sort_ascending=True, user_club=None):
    players = get_all_players()
    if not players: return pd.DataFrame()
    df = pd.DataFrame(players)
    if filter_option == "Unassigned Players":
        df = df[df['Assigned Roles'].apply(lambda x: not x)]
    elif filter_option == "Players Not From My Club" and user_club:
        df = df[df['Club'] != user_club]
    elif filter_option == "Unassigned Players Not From My Club" and user_club:
        df = df[(df['Club'] != user_club) & (df['Assigned Roles'].apply(lambda x: not x))]
    if club_filter != "All": df = df[df['Club'] == club_filter]
    if position_filter != "All": df = df[df['Position'] == position_filter]
    return df.sort_values(by=sort_column, ascending=sort_ascending)

@st.cache_data
def get_players_by_role(role, user_club, second_team_club=None):
    players = get_all_players()
    empty_df = pd.DataFrame(columns=ROLE_ANALYSIS_COLUMNS)
    if not players: return empty_df, empty_df, empty_df
    players_with_role = [p for p in players if role in p.get('Assigned Roles', [])]
    if not players_with_role: return empty_df, empty_df, empty_df
    
    df = pd.DataFrame(players_with_role)
    weights = {cat: get_weight(cat.lower().replace(" ", "_"), default) for cat, default in WEIGHT_DEFAULTS.items()}
    gk_weights = {cat: get_weight("gk_" + cat.lower().replace(" ", "_"), default) for cat, default in GK_WEIGHT_DEFAULTS.items()}
    all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]
    weights_to_use = gk_weights if role in all_gk_roles else weights
    ratings = [calculate_dwrs(player, role, weights_to_use) for _, player in df.iterrows()]
    df['DWRS Rating (Absolute)'] = [r[0] for r in ratings]
    df['DWRS Rating (Normalized)'] = [r[1].rstrip('%') for r in ratings]
    df['DWRS Rating (Normalized)'] = pd.to_numeric(df['DWRS Rating (Normalized)'])
    
    my_club_df = df[df['Club'] == user_club]
    second_team_df = df[df['Club'] == second_team_club] if second_team_club else pd.DataFrame()
    exclude_clubs = [user_club, second_team_club] if second_team_club else [user_club]
    scouted_df = df[~df['Club'].isin(exclude_clubs)]

    final_dfs = []
    for temp_df in [my_club_df, second_team_df, scouted_df]:
        if not temp_df.empty:
            temp_df = temp_df.sort_values(by='DWRS Rating (Normalized)', ascending=False)
            temp_df['DWRS Rating (Normalized)'] = temp_df['DWRS Rating (Normalized)'].apply(lambda x: f"{int(x)}%")
            final_dfs.append(temp_df[ROLE_ANALYSIS_COLUMNS])
        else:
            final_dfs.append(empty_df)
    return final_dfs

@st.cache_data
def get_player_role_matrix(user_club, second_team_club=None):
    players = get_all_players()
    if not players: return pd.DataFrame()
    df_players = pd.DataFrame(players)
    if df_players.empty: return pd.DataFrame()
    weights = {cat: get_weight(cat.lower().replace(" ", "_"), default) for cat, default in WEIGHT_DEFAULTS.items()}
    gk_weights = {cat: get_weight("gk_" + cat.lower().replace(" ", "_"), default) for cat, default in GK_WEIGHT_DEFAULTS.items()}
    all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]
    matrix_data = []
    for _, player in df_players.iterrows():
        player_dict = player.to_dict()
        row = {col: player_dict.get(col, '') for col in PLAYER_ROLE_MATRIX_COLUMNS}
        for role in get_valid_roles():
            if role in player_dict.get('Assigned Roles', []):
                weights_to_use = gk_weights if role in all_gk_roles else weights
                _, normalized = calculate_dwrs(player_dict, role, weights_to_use)
                row[role] = normalized
            else:
                row[role] = ''
        matrix_data.append(row)
    return pd.DataFrame(matrix_data)