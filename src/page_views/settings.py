# settings.py
import os
import streamlit as st
import re

from constants import get_tactic_roles, FIELD_PLAYER_APT_OPTIONS, GK_APT_OPTIONS
from config_handler import (get_theme_settings, save_theme_settings, 
                          get_apt_weight, set_apt_weight, get_weight, set_weight, get_role_multiplier, 
                          set_role_multiplier, get_age_threshold, set_age_threshold, get_selection_bonus, 
                          set_selection_bonus, get_db_name, set_db_name, get_squad_management_setting, 
                          set_squad_management_setting)
from definitions_handler import PROJECT_ROOT
from utils import calculate_contrast_ratio, get_available_databases
from ui_components import clear_all_caches, display_custom_header
from theme_handler import set_theme_toml
from sqlite_db import (update_dwrs_ratings, get_favorite_tactics, set_favorite_tactics, get_club_identity, 
                       set_club_identity, get_prunable_player_info, prune_scouted_players)
from data_parser import load_data
from constants import get_valid_roles

def settings_page():
    display_custom_header("Settings")
    # --- Fetch current theme settings once at the top ---
    theme_settings = get_theme_settings()
    current_mode = theme_settings.get('current_mode', 'night')

    with st.expander("⭐ Favorite Tactic Selection", expanded=True):
        st.info("The selected tactics will appear at the top of the list on the analysis pages.")
        
        # Get all available tactics and add a "None" option
        all_tactics = ["None"] + sorted(list(get_tactic_roles().keys()))
        
        # Get currently saved favorite tactics
        fav_tactic1, fav_tactic2 = get_favorite_tactics()
        
        # Set the index for the dropdowns, defaulting to "None" (index 0)
        index1 = all_tactics.index(fav_tactic1) if fav_tactic1 in all_tactics else 0
        index2 = all_tactics.index(fav_tactic2) if fav_tactic2 in all_tactics else 0

        c1, c2 = st.columns(2)
        with c1:
            new_fav_tactic1 = st.selectbox("Primary Favorite Tactic", options=all_tactics, index=index1)
        with c2:
            new_fav_tactic2 = st.selectbox("Secondary Favorite Tactic", options=all_tactics, index=index2)

    with st.expander("🎨 Club Identity & Theme"):
        st.info(f"You are currently customizing the **{current_mode.capitalize()} Mode** theme. Use the toggle in the sidebar to switch modes.")
        st.info("Upload your club's logo and select its primary and secondary colors to personalize the app's theme.")

        logo_file = st.file_uploader("Upload Club Logo", type=['png', 'jpg', 'jpeg'], help="Recommended size: 200x200 pixels.")
        if logo_file is not None:
            try:
                assets_dir = os.path.join(PROJECT_ROOT, 'config', 'assets')
                os.makedirs(assets_dir, exist_ok=True)
                with open(os.path.join(assets_dir, "logo.png"), "wb") as f:
                    f.write(logo_file.getbuffer())
                st.success("Logo uploaded successfully!")
            except Exception as e:
                st.error(f"Error saving logo: {e}")

        st.subheader("Club Details")
        current_full_name, current_stadium_name = get_club_identity()
        
        c1, c2 = st.columns(2)
        with c1:
            new_full_name = st.text_input("Full Club Name", value=current_full_name or "")
        with c2:
            new_stadium_name = st.text_input("Stadium Name", value=current_stadium_name or "")

        # --- NEW: Two Color Pickers ---
        st.subheader(f"{current_mode.capitalize()} Mode Colors")
        
        # Dynamically get the colors for the current mode
        primary_val = theme_settings.get(f"{current_mode}_primary_color")
        text_val = theme_settings.get(f"{current_mode}_text_color")
        bg_val = theme_settings.get(f"{current_mode}_background_color")
        sec_bg_val = theme_settings.get(f"{current_mode}_secondary_background_color")
        c1, c2 = st.columns(2)
        with c1:
            new_primary = st.color_picker("Primary Color (Buttons, Highlights)", primary_val)
            new_bg = st.color_picker("Background Color", bg_val)
        with c2:
            new_text = st.color_picker("Text & Accent Color", text_val)
            new_sec_bg = st.color_picker("Secondary Background Color", sec_bg_val)
        
        # --- NEW: Live Contrast Warning ---
        st.caption("The secondary color is used for most text. For best readability, ensure it has strong contrast with the app's dark background and your primary color.")
        
        # --- Live Contrast Warning ---
        bg_contrast_ratio = calculate_contrast_ratio(new_text, new_bg)
        primary_contrast_ratio = calculate_contrast_ratio(new_text, new_primary)
        
        if bg_contrast_ratio < 4.5 or primary_contrast_ratio < 3.0:
            st.warning(f"""
                **Low Contrast Warning!**
                - Text on background contrast: {bg_contrast_ratio:.2f}:1 (Aim for 4.5:1)
                - Text on primary color contrast: {primary_contrast_ratio:.2f}:1 (Aim for 3:1)
                
                Text may be difficult to read with this combination.
            """)

    with st.expander("📄 Agreed Playing Time (APT) Weights"):
        st.info("Adjust the multiplier for a player's selection score based on their promised playing time. A higher value makes them more likely to be selected.")
        
        all_apt_options = sorted(list(set(FIELD_PLAYER_APT_OPTIONS + GK_APT_OPTIONS) - {'None'}))
        new_apt_weights = {}
        
        cols = st.columns(3)
        col_idx = 0
        for apt in all_apt_options:
            with cols[col_idx % 3]:
                # Use the new get_apt_weight function to get the current value
                current_weight = get_apt_weight(apt)
                new_apt_weights[apt] = st.number_input(f"Weight for '{apt}'", 0.0, 5.0, current_weight, 0.01, key=f"apt_{apt}")
            col_idx += 1

    with st.expander("⚖️ DWRS Weights & Multipliers"):
        st.info(
            "These settings form the core of the DWRS calculation. "
            "'Global Weights' define the general importance of a meta category. "
            "'Role Multipliers' boost the value of an attribute if it is 'Key' or 'Preferable' for a specific role, "
            "making the calculation role-sensitive."
        )
        st.subheader("Global Stat Weights")
        new_weights = {cat: st.number_input(f"{cat} Weight", 0.0, 10.0, get_weight(cat.lower().replace(" ", "_"), val), 0.1) for cat, val in { "Extremely Important": 8.0, "Important": 4.0, "Good": 2.0, "Decent": 1.0, "Almost Irrelevant": 0.2 }.items()}
        st.subheader("Goalkeeper Stat Weights")
        new_gk_weights = {cat: st.number_input(f"{cat} Weight", 0.0, 10.0, get_weight("gk_" + cat.lower().replace(" ", "_"), val), 0.1, key=f"gk_{cat}") for cat, val in { "Top Importance": 10.0, "High Importance": 8.0, "Medium Importance": 6.0, "Key": 4.0, "Preferable": 2.0, "Other": 0.5 }.items()}
        st.subheader("Role & Position Multipliers")
        c1, c2, c3 = st.columns(3)
        with c1:
            key_mult = st.number_input("Key Attributes Multiplier", 1.0, 20.0, get_role_multiplier('key'), 0.1)
        with c2:
            pref_mult = st.number_input("Preferable Attr. Multiplier", 1.0, 20.0, get_role_multiplier('preferable'), 0.1)
        with c3:
            natural_pos_mult = st.number_input(
                "Natural Position Bonus", 
                min_value=1.0, 
                max_value=2.0, 
                value=get_selection_bonus('natural_position'), 
                step=0.01, # <-- Using 0.01 for finer control like 1.05
                help="Bonus multiplier applied in the Best XI calculator if a player is in one of their 'Natural Positions'. 1.0 = No bonus."
            )


    with st.expander("👶 Surplus Player Age Thresholds"):
        st.info(
            "Define the maximum age for a player to be considered 'youth'. Players at or below (<=) this age will be "
            "suggested for loan, while older players will be suggested for sale/release. This affects the "
            "'Best Position Calculator' and 'Transfer & Loan Management' pages."
        )

        col1, col2 = st.columns(2)
        with col1:
            new_outfielder_age = st.number_input(
                "Outfielder Youth Age", 
                min_value=15, 
                max_value=30, 
                value=get_age_threshold('outfielder'), 
                step=1
            )
        with col2:
            new_goalkeeper_age = st.number_input(
                "Goalkeeper Youth Age", 
                min_value=15, 
                max_value=35, 
                value=get_age_threshold('goalkeeper'), 
                step=1
            )

    with st.expander("👨‍👩‍👧‍👦 Squad Management"):
        st.info(
            "Configure the logic for the Best Position Calculator. This controls how the algorithm selects players for depth roles."
        )
        new_max_roles = st.number_input(
            "Max Unique Roles per Depth Player",
            min_value=1,
            max_value=5,
            value=get_squad_management_setting('max_roles_per_depth_player'),
            step=1,
            help="In the 'Best XI' calculator, this is the maximum number of different roles a single player can cover in the 'Additional Depth' section. A lower number encourages specialists, a higher number encourages versatile players."
        )

    # --- START OF NEW DATABASE MAINTENANCE SECTION ---
    with st.expander("💾 Database Maintenance"):
        st.warning("⚠️ **Danger Zone:** Actions here permanently delete data.")
        
        st.subheader("Prune Low-Potential Scouted Players")
        st.info("This tool will delete scouted players (not from your club or second team) whose single best DWRS rating is below the threshold you set. This is useful for cleaning up large databases of scouted players.")

        prune_threshold = st.slider(
            "DWRS Deletion Threshold",
            min_value=30, max_value=70, value=50,
            help="Any scouted player whose BEST role rating is BELOW this value will be deleted."
        )

        # Pre-calculate and show the impact of the deletion
        count, max_rating = get_prunable_player_info(prune_threshold)
        
        if count > 0:
            st.write(f"This action will permanently delete **{count}** scouted players.")
            st.write(f"The best player to be deleted has a max rating of **{max_rating:.0f}%**.")
        else:
            st.write("No players match the current criteria for deletion.")

        confirm = st.checkbox("I understand that this action is permanent and cannot be undone.")

        if st.button("Prune Scouted Players", disabled=not confirm, type="primary"):
            if confirm:
                with st.spinner(f"Deleting {count} players from the database..."):
                    deleted_count = prune_scouted_players(prune_threshold)
                
                if deleted_count >= 0:
                    st.success(f"Successfully deleted {deleted_count} players.")
                    clear_all_caches()
                    st.rerun()
                else:
                    st.error("The pruning process failed. Your data has not been changed.")

    with st.expander("⚙️ Database Settings"):
        db_action = st.radio("Action", ["Select Existing Database", "Create New Database"], horizontal=True)
        
        current_db_name = get_db_name()
        db_to_set = current_db_name # Default to current DB
        is_valid_new_db = False

        if db_action == "Select Existing Database":
            available_dbs = get_available_databases()
            if not available_dbs:
                st.warning("No existing databases found. Create one below.")
            else:
                try:
                    current_index = available_dbs.index(current_db_name)
                except ValueError:
                    current_index = 0
                
                selected_db = st.selectbox("Select a database", options=available_dbs, index=current_index)
                db_to_set = selected_db

        else: # Create New Database
            new_db_name = st.text_input("Enter new database name (e.g., 'My FM24 Save')")
            if new_db_name:
                # Sanitize the name to create a valid filename
                sanitized_name = re.sub(r'[^\w\s-]', '', new_db_name).strip()
                if not sanitized_name:
                    st.error("Invalid name. Please use letters, numbers, spaces, or hyphens.")
                elif sanitized_name in get_available_databases():
                    st.error(f"A database named '{sanitized_name}' already exists.")
                else:
                    st.success(f"Ready to create and switch to '{sanitized_name}.db' on save.")
                    db_to_set = sanitized_name
                    is_valid_new_db = True

    # This button remains outside the expanders
    if st.button("Save All Settings", type="primary"):
        set_favorite_tactics(new_fav_tactic1, new_fav_tactic2)

        # --- Update the theme settings dictionary with new values ---
        theme_settings[f"{current_mode}_primary_color"] = new_primary
        theme_settings[f"{current_mode}_text_color"] = new_text
        theme_settings[f"{current_mode}_background_color"] = new_bg
        theme_settings[f"{current_mode}_secondary_background_color"] = new_sec_bg

        # Save the updated dictionary back to config.ini
        save_theme_settings(theme_settings)

        # Apply the just-saved settings to the live theme
        set_theme_toml(new_primary, new_text, new_bg, new_sec_bg)
        
        # --- NEW: Save the APT weights ---
        for apt, val in new_apt_weights.items():
            set_apt_weight(apt, val)

        for cat, val in new_weights.items(): set_weight(cat.lower().replace(" ", "_"), val)
        for cat, val in new_gk_weights.items(): set_weight("gk_" + cat.lower().replace(" ", "_"), val)
        set_role_multiplier('key', key_mult)
        set_role_multiplier('preferable', pref_mult)
        set_age_threshold('outfielder', new_outfielder_age)
        set_age_threshold('goalkeeper', new_goalkeeper_age)
        set_selection_bonus('natural_position', natural_pos_mult)
        set_squad_management_setting('max_roles_per_depth_player', new_max_roles)
        set_club_identity(new_full_name, new_stadium_name)

        # Save only if the selection is a valid new DB name or different from the current one
        if (db_action == "Create New Database" and is_valid_new_db) or (db_action == "Select Existing Database" and db_to_set != current_db_name):
            set_db_name(db_to_set)
            st.toast(f"Switched active database to '{db_to_set}.db'", icon="💾")

        clear_all_caches()
        df = load_data()
        if df is not None: update_dwrs_ratings(df, get_valid_roles())
        st.success("Settings saved successfully!")
        st.info("Theme changes may require a full app restart (Ctrl+C in terminal and `streamlit run app.py`) to apply correctly.")
        st.rerun()