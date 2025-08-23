# ui_components.py

import streamlit as st
from constants import MASTER_POSITION_MAP, APT_ABBREVIATIONS


def display_tactic_grid(team, title, positions, layout):
    """
    Renders a visually appealing, consistent, and hierarchical tactical layout.
    Uses a two-line name container to handle all name lengths gracefully.
    """
    st.subheader(title)
    default_player = {"name": "-", "rating": "0%", "apt": ""}

    if isinstance(layout, list):
        # ... (Backward compatibility code remains unchanged) ...
        return

    # --- Dynamic Pitch Grid Logic ---
    occupied_columns = set()
    for stratum, player_keys in layout.items():
        for pos_key in player_keys:
            _, col_index = MASTER_POSITION_MAP.get(pos_key, (None, None))
            if col_index is not None:
                main_grid_col = col_index + 1 if stratum == "Strikers" else col_index
                occupied_columns.add(main_grid_col)

    column_widths = ["1fr" if i in occupied_columns else ("0.02fr" if i == 2 else "0.1fr") for i in range(5)]
    grid_template_columns = " ".join(column_widths)

    # --- FINAL CSS with a dedicated two-line name container ---
    grid_css = f"""
    <style>
        .pitch-grid {{ display: grid; grid-template-columns: {grid_template_columns}; grid-template-rows: repeat(5, 1fr) auto; gap: 5px; background-color: #2a5d34; border: 2px solid #ccc; border-radius: 10px; padding: 10px; margin: 0 auto; min-height: 550px; }}
        .player-box, .placeholder {{ border-radius: 5px; padding: 5px 8px; min-height: 85px; text-align: center; display: flex; flex-direction: column; justify-content: center; line-height: 1.2; }}
        .player-box {{ border: 1px solid #555; background-color: rgba(0, 0, 0, 0.3); }}
        .gk-box {{ grid-row: 6; grid-column: 1 / 6; margin-top: 15px; }}
        .placeholder {{ border: none; }}
        /* --- NEW: CSS for the consistent two-line name --- */
        .player-name {{
            height: 2.5em; /* Reserve space for two lines of text */
            display: flex;
            align-items: center; /* Vertically center the name(s) */
            justify-content: center;
            overflow-wrap: break-word; /* Force long words like 'Kanellopoulos' to wrap */
        }}
    </style>
    """
    st.markdown(grid_css, unsafe_allow_html=True)

    html_out = '<div class="pitch-grid">'
    stratum_to_row = {"Strikers": 1, "Attacking Midfield": 2, "Midfield": 3, "Defensive Midfield": 4, "Defense": 5}
    all_player_positions = [pos for stratum in layout.values() for pos in stratum]

    for r in range(1, 6):
        for c in range(1, 6):
            cell_content = ""
            is_placeholder = True
            for pos_key in all_player_positions:
                stratum, col_index = MASTER_POSITION_MAP.get(pos_key, (None, None))
                if stratum is None or stratum_to_row.get(stratum) != r: continue
                
                main_grid_col = col_index + 2 if stratum == "Strikers" else col_index + 1
                
                if main_grid_col == c:
                    player_info = team.get(pos_key, default_player)
                    role = positions.get(pos_key, "")
                    
                    full_apt = player_info.get('apt', '')
                    display_apt = APT_ABBREVIATIONS.get(full_apt, full_apt)
                    apt_html = f"<small style='color: #bbb;'><i>{display_apt}</i></small>" if display_apt else ""
                    
                    # --- FINAL HTML structure ---
                    cell_content = (
                        f'<div class="player-box">'
                        f'<div class="player-name">{player_info["name"]}</div>' # Use the full name in the new container
                        f"<b style='font-size: 1.1em;'>{player_info['rating']}</b>"
                        f"<small style='color: #ccc;'><i>({role})</i></small>"
                        f"{apt_html}</div>"
                    )
                    is_placeholder = False
                    break
            
            if is_placeholder:
                cell_content = '<div class="placeholder"></div>'
            html_out += cell_content

    # Handle the Goalkeeper with the same consistent structure
    gk_pos_key = 'GK'
    gk_role = positions.get(gk_pos_key, "GK")
    player_info = team.get(gk_pos_key, default_player)
    
    full_apt_gk = player_info.get('apt', '')
    apt_html_gk = f"<small style='color: #bbb;'><i>{full_apt_gk}</i></small>" if full_apt_gk else ""

    # --- FINAL GK HTML structure ---
    gk_display = (
        f'<div class="player-box gk-box">'
        f'<div class="player-name">{player_info["name"]}</div>' # Use the full name in the new container
        f"<b style='font-size: 1.1em;'>{player_info['rating']}</b>"
        f"<small style='color: #ccc;'><i>({gk_role})</i></small>"
        f"{apt_html_gk}</div>"
    )
    html_out += gk_display

    html_out += '</div>'
    st.markdown(html_out, unsafe_allow_html=True)