# player_comparison.py

import streamlit as st
import pandas as pd
import plotly.graph_objects as go

from config_handler import get_theme_settings
from sqlite_db import get_user_club, get_favorite_tactics
from constants import get_valid_roles, get_tactic_roles, GLOBAL_STAT_CATEGORIES, GK_STAT_CATEGORIES, get_role_specific_weights
from utils import format_role_display, hex_to_rgb, color_attribute_by_value
from ui_components import display_custom_header

def player_comparison_page(players):
    #st.title("Player Comparison")
    display_custom_header("Player Comparison")

    # --- START: THEME-AWARE SETUP ---
    theme_settings = get_theme_settings()
    current_mode = theme_settings.get('current_mode', 'night')

    # Define chart colors and palettes based on the current mode
    if current_mode == 'day':
        # Day Mode: Dark text, light backgrounds, saturated trace colors
        chart_bg_color = 'rgba(230, 230, 230, 0.5)' # Light gray background
        font_color = theme_settings.get('day_text_color', '#31333F')
        grid_color = 'rgba(0, 0, 0, 0.2)'
        # Palette designed for high contrast on light backgrounds
        primary_color = theme_settings.get('day_primary_color', '#0055a4')
        trace_palette = [primary_color, '#D32F2F', '#7B1FA2', '#0288D1', '#FFA000']
    else:
        # Night Mode: Light text, dark backgrounds, bright trace colors
        chart_bg_color = 'rgba(46, 46, 46, 0.8)' # Original dark gray
        font_color = theme_settings.get('night_text_color', '#FFFFFF')
        grid_color = 'rgba(255, 255, 255, 0.4)'
        # Palette designed for high contrast on dark backgrounds
        primary_color = theme_settings.get('night_primary_color', '#0055a4')
        trace_palette = [primary_color, '#F50057', '#00E5FF', '#FFDE03', '#76FF03']
    # --- END: THEME-AWARE SETUP ---
    
    df = pd.DataFrame(players)
    if df.empty:
        st.info("No players available.")
        return

    # --- 1. ADVANCED FILTERING SECTION ---
    st.subheader("Filter Player Selection")
    user_club = get_user_club()
    f_col1, f_col2, f_col3 = st.columns(3)
    
    with f_col1:
        fav_tactic1, _ = get_favorite_tactics()
        all_tactics = ["All Roles"] + sorted(list(get_tactic_roles().keys()))
        tactic_index = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
        selected_tactic = st.selectbox("Filter by Tactic", options=all_tactics, index=tactic_index)

    with f_col2:
        if selected_tactic == "All Roles":
            role_options = get_valid_roles()
        else:
            role_options = sorted(list(set(get_tactic_roles()[selected_tactic].values())))
        selected_role = st.selectbox("Filter by Role", options=role_options, format_func=format_role_display)

    with f_col3:
        club_filter = st.selectbox("Filter by Club", options=["My Club", "All Players"])

    player_pool = df.copy()
    if club_filter == "My Club":
        player_pool = player_pool[player_pool['Club'] == user_club]
    
    player_pool = player_pool[player_pool['Assigned Roles'].apply(lambda roles: selected_role in roles if isinstance(roles, list) else False)]

    # Create a mapping from Unique ID to a descriptive, unique display name
    player_map = {
        p['Unique ID']: f"{p['Name']} ({p['Club']})"
        for _, p in player_pool.iterrows()
    }
    
    if not player_map:
        st.warning(f"No players found with the role '{format_role_display(selected_role)}' in the selected club filter.")
        return
        
    # The multiselect options are now the Unique IDs, but it displays the descriptive names
    selected_ids = st.multiselect(
        f"Select players to compare (up to 5 for optimal viewing)",
        options=list(player_map.keys()),
        format_func=lambda uid: player_map[uid],
        help="Only players matching the filters above are shown."
    )

    if selected_ids:
        # Filter the main DataFrame using the list of selected Unique IDs
        comparison_df = df[df['Unique ID'].isin(selected_ids)].copy()
        
        
        all_gk_roles = ["GK-D", "SK-D", "SK-S", "SK-A"]
        is_gk_role = selected_role in all_gk_roles

        role_weights = get_role_specific_weights().get(selected_role, {"key": [], "preferable": []})
        key_attrs = role_weights["key"]
        pref_attrs = role_weights["preferable"]

        if is_gk_role:
            gameplay_attrs = { 'Shot Stopping': ['Reflexes', 'One vs One', 'Handling', 'Agility'], 'Aerial Control': ['Aerial Reach', 'Command of Area', 'Jumping Reach'], 'Distribution': ['Kicking', 'Throwing', 'Passing', 'Vision'], 'Sweeping': ['Rushing Out (Tendency)', 'Acceleration', 'Pace'], 'Mental': ['Composure', 'Concentration', 'Decisions', 'Anticipation']}
            meta_categories = { "Top Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "Top Importance"], "High Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "High Importance"], "Medium Importance": [attr for attr, cat in GK_STAT_CATEGORIES.items() if cat == "Medium Importance"], "Key": key_attrs, "Preferable": pref_attrs}
            meta_chart_title = "GK Meta-Attribute Profile"
        else:
            gameplay_attrs = { 'Pace': ['Acceleration', 'Pace'], 'Shooting': ['Finishing', 'Long Shots'], 'Passing': ['Passing', 'Crossing', 'Vision'], 'Dribbling': ['Dribbling', 'First Touch', 'Flair'], 'Defending': ['Tackling', 'Marking', 'Positioning'], 'Physical': ['Strength', 'Stamina', 'Balance'], 'Mental': ['Work Rate', 'Determination', 'Teamwork', 'Decisions']}
            meta_categories = { "Extremely Important": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Extremely Important"], "Important": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Important"], "Good": [attr for attr, cat in GLOBAL_STAT_CATEGORIES.items() if cat == "Good"], "Key": key_attrs, "Preferable": pref_attrs}
            meta_chart_title = "Outfield Meta-Attribute Profile"

        with st.expander("What do these charts show?"):
            st.info(f"The charts below visualize player attributes grouped into different categories. The Meta-Attribute Profile is based on the **{format_role_display(selected_role)}** role.")
            info_col1, info_col2 = st.columns(2)
            with info_col1:
                st.markdown("**Chart 1: Gameplay Areas**")
                md_string = "".join([f"- **{cat}**: `{', '.join(attrs)}`\n" for cat, attrs in gameplay_attrs.items()])
                st.markdown(md_string)
            with info_col2:
                st.markdown(f"**Chart 2: {meta_chart_title}**")
                meta_string = "".join([f"- **{cat}**: `{', '.join(attrs) or 'None'}`\n" for cat, attrs in meta_categories.items()])
                st.markdown(meta_string)
        
        def parse_attribute_value(raw_value):
            if isinstance(raw_value, str) and '-' in raw_value:
                try: return sum(map(float, raw_value.split('-'))) / 2
                except (ValueError, TypeError): return 0.0
            else:
                try: return float(raw_value)
                except (ValueError, TypeError): return 0.0

        chart_col1, chart_col2 = st.columns(2)
        with chart_col1:
            st.subheader("Gameplay Areas")
            fig1 = go.Figure()

            # --- FIX: Define the theta values for the loop ONCE ---
            gameplay_theta = list(gameplay_attrs.keys())

            # --- UPDATED: Loop to build chart with dynamic colors ---
            for i, uid in enumerate(selected_ids):
                player_data = comparison_df[comparison_df['Unique ID'] == uid].iloc[0]
                category_values = [sum(parse_attribute_value(player_data.get(attr, 0)) for attr in attrs) / len(attrs) if attrs else 0 for attrs in gameplay_attrs.values()]
                
                
                # --- FIX: Append the first value to the end to CLOSE the shape ---
                if category_values:
                    category_values.append(category_values[0])

                # Assign a color from the palette, looping if necessary
                color = trace_palette[i % len(trace_palette)]
                
                fig1.add_trace(go.Scatterpolar(
                    r=category_values, 
                    # --- FIX: Use a closed list of labels that matches the data ---
                    theta=gameplay_theta + [gameplay_theta[0]], 
                    fill='toself', 
                    name=player_map[uid],
                    line=dict(color=color),
                    fillcolor=f"rgba({','.join(str(c) for c in hex_to_rgb(color))}, 0.2)"
                ))
            
            # --- UPDATED: Dynamic layout styling ---
            fig1.update_layout(
                polar=dict(
                    radialaxis=dict(visible=True, range=[0, 20], tickfont=dict(color=font_color), gridcolor=grid_color),
                    angularaxis=dict(tickfont=dict(size=12, color=font_color), direction="clockwise"),
                    bgcolor=chart_bg_color
                ),
                showlegend=False, 
                paper_bgcolor='rgba(0,0,0,0)', 
                plot_bgcolor='rgba(0,0,0,0)', 
                margin=dict(l=40, r=40, t=40, b=40)
            )
            st.plotly_chart(fig1, use_container_width=True)

        with chart_col2:
            st.subheader(meta_chart_title)
            fig2 = go.Figure()
            # --- FIX: Define the theta values for the loop ONCE ---
            meta_theta = list(meta_categories.keys())

            # --- UPDATED: Loop to build chart with dynamic colors ---
            for i, uid in enumerate(selected_ids):
                player_data = comparison_df[comparison_df['Unique ID'] == uid].iloc[0]
                category_values = [sum(parse_attribute_value(player_data.get(attr, 0)) for attr in attrs) / len(attrs) if attrs else 0 for attrs in meta_categories.values()]

                # --- FIX: Append the first value to the end to CLOSE the shape ---
                if category_values:
                    category_values.append(category_values[0])

                color = trace_palette[i % len(trace_palette)]

                fig2.add_trace(go.Scatterpolar(
                    r=category_values, 
                    # --- FIX: Use a closed list of labels that matches the data ---
                    theta=meta_theta + [meta_theta[0]], 
                    fill='toself', 
                    name=player_map[uid],
                    line=dict(color=color),
                    fillcolor=f"rgba({','.join(str(c) for c in hex_to_rgb(color))}, 0.2)"
                ))

            # --- UPDATED: Dynamic layout styling ---
            fig2.update_layout(
                polar=dict(
                    radialaxis=dict(visible=True, range=[0, 20], tickfont=dict(color=font_color), gridcolor=grid_color),
                    angularaxis=dict(tickfont=dict(size=12, color=font_color), direction="clockwise"),
                    bgcolor=chart_bg_color
                ),
                legend=dict(font=dict(color=font_color)), 
                paper_bgcolor='rgba(0,0,0,0)', 
                plot_bgcolor='rgba(0,0,0,0)', 
                margin=dict(l=40, r=40, t=40, b=40)
            )
            st.plotly_chart(fig2, use_container_width=True)

        st.divider()
        st.subheader("Detailed Attribute Comparison")

        # 1. Get a master set of all attribute names for quick lookups.
        all_attributes_set = set(GLOBAL_STAT_CATEGORIES.keys()) | set(GK_STAT_CATEGORIES.keys())
        
        # 2. Prepare the DataFrame fully BEFORE styling.
        df_display = comparison_df.copy()
        df_display['Display Name'] = df_display['Unique ID'].map(player_map)
        df_display['Assigned Roles'] = df_display['Assigned Roles'].apply(lambda roles: ', '.join(roles) if isinstance(roles, list) else roles)
        
        # Set index and transpose. Attributes are now the index.
        df_display = df_display.set_index('Display Name').T

        # 3. Convert attribute rows to numeric values for the styling logic.
        for idx in df_display.index:
            if idx in all_attributes_set:
                df_display.loc[idx] = df_display.loc[idx].apply(lambda x: (int(x.split('-')[0]) + int(x.split('-')[1])) / 2 if isinstance(x, str) and '-' in x else x)
                df_display.loc[idx] = pd.to_numeric(df_display.loc[idx], errors='coerce')
        
        # 4. Define the "smart" styling function.
        def smart_styler(row):
            if row.name in all_attributes_set:
                return [color_attribute_by_value(val) for val in row]
            else:
                return ['' for val in row]

        # 5. Apply styling and formatting.
        styler = df_display.style.format(na_rep='-', precision=0).apply(
            smart_styler,
            axis=1
        )
        
        # This bypasses Streamlit's Arrow serialization and eliminates the warnings.
        html = styler.to_html()
        st.markdown(html, unsafe_allow_html=True)