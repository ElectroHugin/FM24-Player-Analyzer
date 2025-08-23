# ui_components.py

import streamlit as st
from constants import MASTER_POSITION_MAP

def display_tactic_grid(team, title, positions, layout):
    """
    Renders a tactical layout grid using a dynamic CSS Grid.
    This provides fixed slots and intelligently shrinks empty columns.
    It remains fully backward compatible with the old list-based layout format.
    """
    st.subheader(title)
    default_player = {"name": "-", "rating": "0%", "apt": ""}

    # --- BACKWARD COMPATIBILITY CHECK ---
    if isinstance(layout, list):
        st.warning("Note: This tactic is using the old layout format. For a more accurate display, please update it in definitions.json.")
        # (The old rendering code remains here for compatibility)
        gk_pos_key = 'GK'
        gk_role = positions.get(gk_pos_key, "GK")
        player_info = team.get(gk_pos_key, default_player)
        apt_html = f"<br><small><i>{player_info.get('apt', '')}</i></small>" if player_info.get('apt') else ""
        gk_display = f"<div style='text-align: center;'><b>{gk_pos_key}</b> ({gk_role})<br>{player_info['name']}<br><i>{player_info['rating']}</i>{apt_html}</div>"
        for row in reversed(layout):
            cols = st.columns(len(row))
            for i, pos_key in enumerate(row):
                player_info = team.get(pos_key, default_player)
                role = positions.get(pos_key, "")
                apt_html = f"<br><small><i>{player_info.get('apt', '')}</i></small>" if player_info.get('apt') else ""
                cols[i].markdown(f"<div style='text-align: center; border: 1px solid #444; border-radius: 5px; padding: 10px; height: 100%;'><b>{pos_key}</b> ({role})<br>{player_info['name']}<br><i>{player_info['rating']}</i>{apt_html}</div>", unsafe_allow_html=True)
            st.write("")
        st.markdown(gk_display, unsafe_allow_html=True)
        return

    # --- NEW DYNAMIC CSS GRID LOGIC (CORRECTED) ---

    # 1. Pre-computation: Analyze which of the 5 main columns are occupied.
    occupied_columns = set()
    for stratum, player_keys in layout.items():
        for pos_key in player_keys:
            _, col_index = MASTER_POSITION_MAP.get(pos_key, (None, None))
            if col_index is not None:
                # For strikers (indices 0,1,2), map them to the main grid columns (1,2,3)
                main_grid_col = col_index + 1 if stratum == "Strikers" else col_index
                occupied_columns.add(main_grid_col)

    # 2. Dynamic Style Generation for column widths.
    column_widths = []
    for i in range(5): # Iterate through the 5 main grid columns (0-4)
        if i in occupied_columns:
            column_widths.append("1fr")  # Full width
        else:
            if i == 2: # Empty central column
                column_widths.append("0.02fr") # Minimal width
            else: # Empty side columns
                column_widths.append("0.1fr") # Reduced width
    
    grid_template_columns = " ".join(column_widths)

    # 3. Define the CSS styles.
    grid_css = f"""
    <style>
        .pitch-grid {{
            display: grid;
            grid-template-columns: {grid_template_columns};
            gap: 8px;
            padding: 10px;
            max-width: 50%;
            margin: 0 auto;
        }}
        .player-box, .placeholder {{
            border-radius: 5px;
            padding: 10px;
            min-height: 90px;
            text-align: center;
        }}
        .player-box {{
            border: 1px solid #555;
            background-color: transparent; /* TRANSPARENT BACKGROUND */
        }}
        .placeholder {{
            border: none;
            opacity: 0.5;
        }}
    </style>
    """
    st.markdown(grid_css, unsafe_allow_html=True)

    # 4. Build the HTML string for the pitch grid.
    html_out = '<div class="pitch-grid">'
    
    stratum_to_row = {"Strikers": 1, "Attacking Midfield": 2, "Midfield": 3, "Defensive Midfield": 4, "Defense": 5}
    
    # Create a simple list of all player positions to place on the grid
    all_player_positions = []
    for stratum_name, player_keys in layout.items():
        for pos_key in player_keys:
            all_player_positions.append(pos_key)

    # Iterate through every cell of a 5x5 grid and decide what to put there
    for r in range(1, 6): # Rows 1 to 5
        for c in range(1, 6): # Columns 1 to 5
            
            cell_content = ""
            is_placeholder = True

            # Find if a player belongs in this specific cell (r, c)
            for pos_key in all_player_positions:
                stratum, col_index = MASTER_POSITION_MAP.get(pos_key, (None, None))
                if stratum is None: continue

                # Check if the player's stratum matches the current row
                if stratum_to_row.get(stratum) == r:
                    # Check if the player's column matches the current column
                    # This includes the special centering logic for strikers
                    main_grid_col = col_index + 1
                    if stratum == "Strikers":
                        main_grid_col = col_index + 2 # Center strikers in columns 2,3,4
                    
                    if main_grid_col == c:
                        player_info = team.get(pos_key, default_player)
                        role = positions.get(pos_key, "")
                        apt_html = f"<br><small><i>{player_info.get('apt', '')}</i></small>" if player_info.get('apt') else ""
                        
                        cell_content = (
                            f'<div class="player-box">'
                            f"<b>{pos_key}</b> ({role})<br>{player_info['name']}<br>"
                            f"<i>{player_info['rating']}</i>{apt_html}</div>"
                        )
                        is_placeholder = False
                        break # Found the player for this cell, stop searching

            if is_placeholder:
                cell_content = '<div class="placeholder"></div>'

            html_out += cell_content

    html_out += '</div>'
    st.markdown(html_out, unsafe_allow_html=True)
    
    # 5. Handle the Goalkeeper separately
    st.divider()
    gk_pos_key = 'GK'
    gk_role = positions.get(gk_pos_key, "GK")
    player_info = team.get(gk_pos_key, default_player)
    apt_html = f"<br><small><i>{player_info.get('apt', '')}</i></small>" if player_info.get('apt') else ""
    gk_display = (f"<div style='text-align: center;'><b>{gk_pos_key}</b> ({gk_role})<br>{player_info['name']}<br>"
                  f"<i>{player_info['rating']}</i>{apt_html}</div>")
    st.markdown(gk_display, unsafe_allow_html=True)