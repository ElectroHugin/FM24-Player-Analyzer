# src/ui_components.py

import streamlit as st

from constants import MASTER_POSITION_MAP, APT_ABBREVIATIONS, get_tactic_layouts
from sqlite_db import get_club_identity, get_user_club
from config_handler import get_db_name, get_theme_settings

def clear_all_caches():
    st.cache_data.clear()

def display_tactic_grid(team, title, positions, layout, mode='night'):
    """
    Renders a visually appealing, consistent, and hierarchical tactical layout.
    Now accepts a 'mode' argument to switch between 'day' and 'night' themes.
    """
    st.subheader(title)
    default_player = {"name": "-", "rating": "0%", "apt": ""}

    # --- START: THEME-AWARE COLOR DEFINITIONS ---
    if mode == 'day':
        # Day Mode: Light pitch, dark text, light player boxes
        pitch_bg = "#D3EED8"              # Light pastel green
        pitch_border = "#B0C4B3"           # Muted green-gray border
        markings_color = "rgba(0, 0, 0, 0.2)" # Dark, semi-transparent markings
        player_box_bg = "rgba(255, 255, 255, 0.7)" # Light semi-transparent box
        player_box_border = "#B0B0B0"
        main_text_color = "#31333F"        # Dark text (from your day theme)
        secondary_text_color = "#555"      # Darker gray for role/apt
        rating_text_color = "#000"         # Black for the rating
    else: # Night Mode (default)
        # Night Mode: Dark pitch, light text, dark player boxes
        pitch_bg = "#2a5d34"              # Original dark green
        pitch_border = "#ccc"
        markings_color = "rgba(255, 255, 255, 0.4)" # Light, semi-transparent markings
        player_box_bg = "rgba(0, 0, 0, 0.4)"   # Dark semi-transparent box
        player_box_border = "#555"
        main_text_color = "#FFFFFF"        # White text
        secondary_text_color = "#ccc"      # Light gray for role/apt
        rating_text_color = "#FFF"         # White for the rating
    # --- END: THEME-AWARE COLOR DEFINITIONS ---

        injury_highlight_color = "#F80B1E"

    # --- Dynamic Pitch Grid Logic (Unchanged) ---
    occupied_columns = set()
    for stratum, player_keys in layout.items():
        for pos_key in player_keys:
            _, col_index = MASTER_POSITION_MAP.get(pos_key, (None, None))
            if col_index is not None:
                main_grid_col = col_index + 1 if stratum == "Strikers" else col_index
                occupied_columns.add(main_grid_col)

    column_widths = ["1fr" if i in occupied_columns else ("0.02fr" if i == 2 else "0.1fr") for i in range(5)]
    grid_template_columns = " ".join(column_widths)

    # --- THEME-AWARE CSS ---
    grid_css = f"""
    <style>
        .pitch-grid {{
            position: relative;
            display: grid;
            grid-template-columns: {grid_template_columns};
            grid-template-rows: repeat(6, 1fr);
            gap: 5px;
            background-color: {pitch_bg};
            border: 2px solid {pitch_border};
            border-radius: 10px;
            padding: 10px;
            margin: 0 auto;
            min-height: 550px;
            overflow: hidden;
            color: {main_text_color}; /* Set default text color for the grid */
        }}
        .player-box, .placeholder {{
            position: relative;
            z-index: 2;
            border-radius: 5px; padding: 5px 8px; min-height: 80px; text-align: center;
            display: flex; flex-direction: column; justify-content: center; line-height: 1.2;
        }}
        .player-box {{ 
            border: 1px solid {player_box_border}; 
            background-color: {player_box_bg}; 
        }}
        .gk-box {{ grid-row: 6; grid-column: 1 / 6; }}
        .placeholder {{ border: none; }}
        .player-name {{ 
            height: 2.5em; display: flex; align-items: center; justify-content: center; 
            overflow-wrap: break-word; font-weight: bold;
        }}
        .player-rating b {{ color: {rating_text_color}; font-size: 1.1em; }}

        /* --- Pitch Markings with Dynamic Color --- */
        .pitch-markings {{
            position: absolute; top: 0; left: 0; right: 0; bottom: 0; z-index: 1;
        }}
        .line {{ position: absolute; background-color: {markings_color}; }}
        .marking-box {{ position: absolute; border: 2px solid {markings_color}; }}
        
        /* ... All other marking definitions remain the same ... */
        .center-line {{ width: 100%; height: 2px; top: 50%; transform: translateY(-50%); }}
        .center-circle {{ width: 90px; height: 90px; border-radius: 50%; top: 50%; left: 50%; transform: translate(-50%, -50%); }}
        .penalty-box {{ width: 40%; height: 18%; left: 50%; transform: translateX(-50%); }}
        .penalty-box.top {{ top: -2px; border-top: none; }}
        .penalty-box.bottom {{ bottom: -2px; border-bottom: none; }}
        .goal-area {{ width: 20%; height: 7%; left: 50%; transform: translateX(-50%); }}
        .goal-area.top {{ top: -2px; border-top: none; }}
        .goal-area.bottom {{ bottom: -2px; border-bottom: none; }}
        .penalty-spot {{ width: 4px; height: 4px; border-radius: 50%; left: 50%; transform: translateX(-50%); }}
        .penalty-spot.top {{ top: 11%; }}
        .penalty-spot.bottom {{ bottom: 11%; }}
        .penalty-arc {{ width: 25%; height: 8%; left: 50%; transform: translateX(-50%); }}
        .penalty-arc.top {{ top: 12.5%; border-radius: 0 0 50% 50% / 0 0 100% 100%; border-color: transparent transparent {markings_color} transparent; }}
        .penalty-arc.bottom {{ bottom: 12.5%; border-radius: 50% 50% 0 0 / 100% 100% 0 0; border-color: {markings_color} transparent transparent transparent; }}
    </style>
    """
    st.markdown(grid_css, unsafe_allow_html=True)

    html_out = '<div class="pitch-grid">'
    # ... (The markings HTML structure remains the same) ...
    html_out += """
        <div class="pitch-markings">
            <div class="line center-line"></div><div class="marking-box center-circle"></div><div class="marking-box penalty-box top"></div><div class="marking-box goal-area top"></div><div class="line penalty-spot top"></div><div class="marking-box penalty-arc top"></div><div class="marking-box penalty-box bottom"></div><div class="marking-box goal-area bottom"></div><div class="line penalty-spot bottom"></div><div class="marking-box penalty-arc bottom"></div>
        </div>
    """
    
    # --- Layer 2: The Players (with dynamic styling) ---
    stratum_to_row = {"Strikers": 1, "Attacking Midfield": 2, "Midfield": 3, "Defensive Midfield": 4, "Defense": 5}
    all_player_positions = [pos for stratum in layout.values() for pos in stratum]

    for r in range(1, 7):
        if r == 6: continue
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
                    is_promoted = player_info.get('promoted_due_to_injury', False)
                    name_style = f"color: {injury_highlight_color};" if is_promoted else ""
                    full_apt = player_info.get('apt', '')
                    display_apt = APT_ABBREVIATIONS.get(full_apt, full_apt)
                    apt_html = f"<small style='color: {secondary_text_color};'><i>{display_apt}</i></small>" if display_apt else ""
                    
                    # --- UPDATED HTML with dynamic colors ---
                    cell_content = (f'<div class="player-box">'
                                    f'<div class="player-name" style="{name_style}">{player_info["name"]}</div>'
                                    f'<div class="player-rating"><b>{player_info["rating"]}</b></div>'
                                    f"<small style='color: {secondary_text_color};'><i>({role})</i></small>"
                                    f"{apt_html}"
                                    f'</div>')
                    is_placeholder = False
                    break
            if is_placeholder:
                cell_content = '<div class="placeholder"></div>'
            html_out += cell_content

    gk_pos_key = 'GK'
    gk_role = positions.get(gk_pos_key, "GK")
    player_info = team.get(gk_pos_key, default_player)
    is_gk_promoted = player_info.get('promoted_due_to_injury', False)
    gk_name_style = f"color: {injury_highlight_color};" if is_gk_promoted else ""
    full_apt_gk = player_info.get('apt', '')
    apt_html_gk = f"<small style='color: {secondary_text_color};'><i>{full_apt_gk}</i></small>" if full_apt_gk else ""
    
    # --- UPDATED GK HTML with dynamic colors ---
    gk_display = (f'<div class="player-box gk-box">'
                  f'<div class="player-name" style="{gk_name_style}">{player_info["name"]}</div>'
                  f'<div class="player-rating"><b>{player_info["rating"]}</b></div>'
                  f"<small style='color: {secondary_text_color};'><i>({gk_role})</i></small>"
                  f"{apt_html_gk}"
                  f'</div>')
    html_out += gk_display

    html_out += '</div>'
    st.markdown(html_out, unsafe_allow_html=True)


def display_strength_grid(positional_strengths, tactic, mode='night'):
    """
    Renders a miniaturized, color-coded tactical grid showing the average, min, and max
    DWRS for each position. The color is relative to the squad's own strength.
    """
    st.markdown("<h3 style='text-align: center;'>Positional Strength</h3>", unsafe_allow_html=True)

    # --- 1. Relative Color Calculation Logic ---
    avg_ratings = [data['avg'] for data in positional_strengths.values() if data['avg'] > 0]
    if not avg_ratings:
        st.info("No rating data available to display the strength grid.")
        return
        
    min_avg, max_avg = min(avg_ratings), max(avg_ratings)
    delta = max_avg - min_avg

    def _get_relative_color(value):
        """Calculates a color on a red-yellow-green gradient based on the squad's min/max avg."""
        if delta == 0:
            return "100, 166, 100" # A neutral green if all values are the same

        # Normalize the value from 0 (worst) to 1 (best)
        normalized_val = (value - min_avg) / delta

        if normalized_val < 0.5:
            # Red to Yellow gradient
            red = 221
            green = 43 + int((240 - 43) * (normalized_val * 2))
            blue = 43
        else:
            # Yellow to Green gradient
            red = 240 - int((240 - 98) * ((normalized_val - 0.5) * 2))
            green = 240 - int((240 - 186) * ((normalized_val - 0.5) * 2))
            blue = 43 + int((98 - 43) * ((normalized_val - 0.5) * 2))
            
        return f"{red}, {green}, {blue}"

    # --- 2. Miniaturized CSS ---
    # Theme-aware colors for text and pitch markings
    text_color = "#FFFFFF" if mode == 'night' else "#31333F"
    markings_color = "rgba(255, 255, 255, 0.3)" if mode == 'night' else "rgba(0, 0, 0, 0.2)"
    pitch_bg = "#2a5d34" if mode == 'night' else "#D3EED8"

    grid_css = f"""
    <style>
        .strength-pitch-grid {{
            display: grid;
            grid-template-columns: repeat(5, 1fr);
            grid-template-rows: repeat(6, 1fr);
            gap: 4px;
            background-color: {pitch_bg};
            border: 1px solid {markings_color};
            border-radius: 8px;
            padding: 8px;
            position: relative;
            overflow: hidden;
            aspect-ratio: 68 / 105;
            
            /* --- NEW: Add a max-width and center the pitch --- */
            max-width: 280px;
            margin: 0 auto;
        }}
        .strength-player-box, .strength-placeholder {{
            border-radius: 4px; padding: 4px; min-height: 55px; /* Reduced height & padding */
            text-align: center; color: {text_color};
            display: flex; flex-direction: column; justify-content: center;
            line-height: 1.1; font-size: 0.8em; /* Reduced font size */
            border: 1px solid rgba(0,0,0,0.2);
        }}
        .strength-player-box b {{ font-size: 1.4em; }} /* Make average stand out */
        .strength-gk-box {{ grid-row: 6; grid-column: 1 / 6; }}
        /* Pitch markings for the mini-pitch */
        .mini-marking {{ position: absolute; border: 1px solid {markings_color}; }}
        .mini-center-line {{ position: absolute; width: 100%; height: 1px; top: 50%; background-color: {markings_color}; }}
        .mini-center-circle {{ top: 50%; left: 50%; width: 60px; height: 60px; border-radius: 50%; transform: translate(-50%, -50%); }}
        .mini-penalty-box {{ width: 40%; height: 16%; left: 50%; transform: translateX(-50%); border-top: none; }}
        .mini-penalty-box.bottom {{ bottom: -1px; }}
    </style>
    """
    st.markdown(grid_css, unsafe_allow_html=True)

    # --- 3. HTML Rendering ---
    html_out = '<div class="strength-pitch-grid">'
    # Add markings
    html_out += '<div class="mini-center-line"></div><div class="mini-marking mini-center-circle"></div>'
    html_out += '<div class="mini-marking mini-penalty-box"></div><div class="mini-marking mini-penalty-box bottom"></div>'

    layout = get_tactic_layouts().get(tactic, {})
    stratum_to_row = {"Strikers": 1, "Attacking Midfield": 2, "Midfield": 3, "Defensive Midfield": 4, "Defense": 5}
    
    occupied_cells = set()

    for stratum, positions in layout.items():
        for pos_key in positions:
            stratum_val, col_index = MASTER_POSITION_MAP.get(pos_key, (None, None))
            if stratum_val is None: continue
            
            row = stratum_to_row.get(stratum)
            col = col_index + 2 if stratum == "Strikers" else col_index + 1
            occupied_cells.add((row, col))
            
            stats = positional_strengths.get(pos_key)
            if stats and stats['avg'] > 0:
                color_rgb = _get_relative_color(stats['avg'])
                box_html = (
                    f"<div class='strength-player-box' style='background-color: rgba({color_rgb}, 0.8); grid-row:{row}; grid-column:{col};'>"
                    f"<b>{stats['avg']:.0f}%</b>"
                    f"<small>({stats['min']:.0f}-{stats['max']:.0f})</small>"
                    "</div>"
                )
            else: # Placeholder for empty positions
                box_html = f"<div class='strength-placeholder' style='grid-row:{row}; grid-column:{col};'>-</div>"
            html_out += box_html

    # Handle GK separately
    gk_stats = positional_strengths.get('GK')
    if gk_stats and gk_stats['avg'] > 0:
        color_rgb_gk = _get_relative_color(gk_stats['avg'])
        gk_html = (
            f"<div class='strength-player-box strength-gk-box' style='background-color: rgba({color_rgb_gk}, 0.8);'>"
            f"<b>{gk_stats['avg']:.0f}%</b>"
            f"<small>({gk_stats['min']:.0f}-{gk_stats['max']:.0f})</small>"
            "</div>"
        )
    else:
        gk_html = "<div class='strength-player-box strength-gk-box'>-</div>"
    html_out += gk_html

    html_out += '</div>'
    st.markdown(html_out, unsafe_allow_html=True)


def display_custom_header(page_title=""):
    """
    Displays a dynamic, personalized header using a three-column layout and theme colors.
    """
    # --- 1. Get all the necessary data and theme settings ---
    full_name, stadium_name = get_club_identity()
    user_club = get_user_club()
    db_name = get_db_name()
    display_name = full_name or user_club or "FM Dashboard"

    theme_settings = get_theme_settings()
    current_mode = theme_settings.get('current_mode', 'night')
    primary_color = theme_settings.get(f"{current_mode}_primary_color")
    text_color = theme_settings.get(f"{current_mode}_text_color")

    # --- 2. Create the three-column layout ---
    with st.container():
        left, center, right = st.columns([2, 3, 1.5])
        
        # --- Left Column: Club Identity ---
        with left:
            st.markdown(f"<h2 style='color: {primary_color}; margin-bottom: -10px;'>{display_name}</h2>", unsafe_allow_html=True)
            if stadium_name:
                st.caption(f"🏟️ {stadium_name}")
        
        # --- Center Column: Page Title ---
        with center:
            if page_title:
                # Add a top margin to help with vertical alignment
                st.markdown(f"<h3 style='text-align: center; color: {text_color}; margin-top: 10px;'>{page_title}</h3>", unsafe_allow_html=True)

        # --- Right Column: Save File Context ---
        with right:
            st.markdown("<p style='text-align: right; margin-bottom: -5px;'><small>Active Save File</small></p>", unsafe_allow_html=True)
            st.markdown(f"<p style='text-align: right;'><strong>{db_name}.db</strong></p>", unsafe_allow_html=True)
            
    st.divider()