# new_tactic.py

import streamlit as st

from definitions_handler import get_definitions, save_definitions
from utils import format_role_display, get_role_display_map
from constants import MASTER_POSITION_MAP, get_position_to_role_mapping
from ui_components import clear_all_caches

def create_new_tactic_page():
    st.title("Create a New Tactical Formation")
    st.info("Design your formation on the pitch below. Use the dropdowns to select a role for each active position. You must select exactly one Goalkeeper and ten outfield players.")

    # --- Load necessary definitions ---
    definitions = get_definitions()
    pos_to_role_map = get_position_to_role_mapping()
    role_display_map = get_role_display_map()

    with st.form("new_tactic_form"):
        # --- Tactic Naming ---
        st.subheader("1. Tactic Name")
        c1, c2 = st.columns([2, 1])
        # --- CHANGE: Added explicit keys for easier clearing ---
        formation_name = c1.text_input("Formation Name (e.g., 'My Counter Press')", help="The descriptive name for your tactic.", key="new_tactic_name")
        formation_shape = c2.text_input("Formation Shape (e.g., '4-2-3-1')", help="The numerical shape, like '4-4-2' or '4-2-3-1'. This becomes part of the final name.", key="new_tactic_shape")

        final_tactic_name = f"{formation_shape} {formation_name}".strip() if formation_name else ""
        if final_tactic_name:
            st.write(f"**Full Tactic Name:** `{final_tactic_name}`")

        st.divider()
        st.subheader("2. Design Your Formation")

        # --- VISUAL PITCH LAYOUT USING STREAMLIT COLUMNS ---
        with st.container():
            st.markdown("""
                <style>
                div[data-testid="stVerticalBlock"] > div[style*="flex-direction: column;"] > div[data-testid="stHorizontalBlock"] {
                    background-color: #2a5d34;
                    border: 1px solid #ccc;
                    border-radius: 10px;
                    padding: 15px 10px;
                }
                </style>
            """, unsafe_allow_html=True)

            # --- Strikers (3 positions, centered) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Strikers</p>", unsafe_allow_html=True)
            s_cols = st.columns(5)
            with s_cols[1]:
                selections = {'STL': st.selectbox("STL", ["- Unused -"] + sorted(pos_to_role_map.get("ST (C)", []), key=role_display_map.get), key="role_STL", format_func=format_role_display)}
            with s_cols[2]:
                selections['STC'] = st.selectbox("STC", ["- Unused -"] + sorted(pos_to_role_map.get("ST (C)", []), key=role_display_map.get), key="role_STC", format_func=format_role_display)
            with s_cols[3]:
                selections['STR'] = st.selectbox("STR", ["- Unused -"] + sorted(pos_to_role_map.get("ST (C)", []), key=role_display_map.get), key="role_STR", format_func=format_role_display)

            # --- Attacking Midfield (5 positions) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Attacking Midfield</p>", unsafe_allow_html=True)
            am_cols = st.columns(5)
            selections['AML'] = am_cols[0].selectbox("AML", ["- Unused -"] + sorted(pos_to_role_map.get("AM (L)", []), key=role_display_map.get), key="role_AML", format_func=format_role_display)
            selections['AMCL'] = am_cols[1].selectbox("AMCL", ["- Unused -"] + sorted(pos_to_role_map.get("AM (C)", []), key=role_display_map.get), key="role_AMCL", format_func=format_role_display)
            selections['AMC'] = am_cols[2].selectbox("AMC", ["- Unused -"] + sorted(pos_to_role_map.get("AM (C)", []), key=role_display_map.get), key="role_AMC", format_func=format_role_display)
            selections['AMCR'] = am_cols[3].selectbox("AMCR", ["- Unused -"] + sorted(pos_to_role_map.get("AM (C)", []), key=role_display_map.get), key="role_AMCR", format_func=format_role_display)
            selections['AMR'] = am_cols[4].selectbox("AMR", ["- Unused -"] + sorted(pos_to_role_map.get("AM (R)", []), key=role_display_map.get), key="role_AMR", format_func=format_role_display)

            # --- Midfield (5 positions) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Midfield</p>", unsafe_allow_html=True)
            m_cols = st.columns(5)
            selections['ML'] = m_cols[0].selectbox("ML", ["- Unused -"] + sorted(pos_to_role_map.get("M (L)", []), key=role_display_map.get), key="role_ML", format_func=format_role_display)
            selections['MCL'] = m_cols[1].selectbox("MCL", ["- Unused -"] + sorted(pos_to_role_map.get("M (C)", []), key=role_display_map.get), key="role_MCL", format_func=format_role_display)
            selections['MC'] = m_cols[2].selectbox("MC", ["- Unused -"] + sorted(pos_to_role_map.get("M (C)", []), key=role_display_map.get), key="role_MC", format_func=format_role_display)
            selections['MCR'] = m_cols[3].selectbox("MCR", ["- Unused -"] + sorted(pos_to_role_map.get("M (C)", []), key=role_display_map.get), key="role_MCR", format_func=format_role_display)
            selections['MR'] = m_cols[4].selectbox("MR", ["- Unused -"] + sorted(pos_to_role_map.get("M (R)", []), key=role_display_map.get), key="role_MR", format_func=format_role_display)

            # --- Defensive Midfield (5 positions) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Defensive Midfield</p>", unsafe_allow_html=True)
            dm_cols = st.columns(5)
            selections['DML'] = dm_cols[0].selectbox("DML", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DML", format_func=format_role_display)
            selections['DMCL'] = dm_cols[1].selectbox("DMCL", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DMCL", format_func=format_role_display)
            selections['DMC'] = dm_cols[2].selectbox("DMC", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DMC", format_func=format_role_display)
            selections['DMCR'] = dm_cols[3].selectbox("DMCR", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DMCR", format_func=format_role_display)
            selections['DMR'] = dm_cols[4].selectbox("DMR", ["- Unused -"] + sorted(pos_to_role_map.get("DM", []), key=role_display_map.get), key="role_DMR", format_func=format_role_display)

            # --- Defense (5 positions) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Defense</p>", unsafe_allow_html=True)
            d_cols = st.columns(5)
            selections['DL'] = d_cols[0].selectbox("DL/WBL", ["- Unused -"] + sorted(pos_to_role_map.get("D (L)", []) + pos_to_role_map.get("WB (L)", []), key=role_display_map.get), key="role_DL", format_func=format_role_display)
            selections['DCL'] = d_cols[1].selectbox("DCL", ["- Unused -"] + sorted(pos_to_role_map.get("D (C)", []), key=role_display_map.get), key="role_DCL", format_func=format_role_display)
            selections['DC'] = d_cols[2].selectbox("DC", ["- Unused -"] + sorted(pos_to_role_map.get("D (C)", []), key=role_display_map.get), key="role_DC", format_func=format_role_display)
            selections['DCR'] = d_cols[3].selectbox("DCR", ["- Unused -"] + sorted(pos_to_role_map.get("D (C)", []), key=role_display_map.get), key="role_DCR", format_func=format_role_display)
            selections['DR'] = d_cols[4].selectbox("DR/WBR", ["- Unused -"] + sorted(pos_to_role_map.get("D (R)", []) + pos_to_role_map.get("WB (R)", []), key=role_display_map.get), key="role_DR", format_func=format_role_display)

            # --- Goalkeeper (Mandatory) ---
            st.markdown("<p style='text-align: center; color: #ccc;'>Goalkeeper</p>", unsafe_allow_html=True)
            gk_roles = [role for role, name in get_role_display_map().items() if "GK" in role or "SK" in role]
            selected_gk_role = st.selectbox("Goalkeeper Role", options=gk_roles, label_visibility="collapsed", format_func=format_role_display, key="role_GK")

        st.divider()
        submitted = st.form_submit_button("Create New Tactic", type="primary")

    if submitted:
        with st.spinner("Validating and saving new tactic..."):
            # --- Validation (reading from session_state for reliability) ---
            final_tactic_name = f"{st.session_state.new_tactic_shape} {st.session_state.new_tactic_name}".strip()
            
            if not final_tactic_name or not st.session_state.new_tactic_name or not st.session_state.new_tactic_shape:
                st.error("Validation Failed: Please provide both a Formation Name and a Formation Shape.")
                return

            if final_tactic_name in definitions['tactic_roles']:
                st.error(f"Validation Failed: A tactic named '{final_tactic_name}' already exists. Please choose a different name.")
                return

            all_selections = {key.split('_')[1]: value for key, value in st.session_state.items() if key.startswith("role_")}
            outfield_players = {pos: role for pos, role in all_selections.items() if role != "- Unused -" and pos != "GK"}
            
            if len(outfield_players) != 10:
                st.error(f"Validation Failed: You must select exactly 10 outfield players. You have selected {len(outfield_players)}.")
                return

            # --- Data Structuring ---
            new_tactic_roles = {"GK": st.session_state.role_GK}
            new_tactic_roles.update(outfield_players)

            new_tactic_layout = {}
            for pos_key in outfield_players.keys():
                stratum, _ = MASTER_POSITION_MAP.get(pos_key, (None, None))
                if pos_key in ["ST", "STC"]: stratum = "Strikers"

                if stratum:
                    if stratum not in new_tactic_layout: new_tactic_layout[stratum] = []
                    new_tactic_layout[stratum].append(pos_key)

            # --- Saving ---
            definitions['tactic_roles'][final_tactic_name] = new_tactic_roles
            definitions['tactic_layouts'][final_tactic_name] = new_tactic_layout
            success, message = save_definitions(definitions)

            if success:
                st.success(f"Tactic '{final_tactic_name}' created successfully! Reloading application...")

                # ------------------- START OF NEW CODE -------------------
                # This block will clear all the form's widget states.
                
                # Find all keys associated with this form's widgets
                keys_to_delete = [k for k in st.session_state.keys() if k.startswith("role_") or k in ["new_tactic_name", "new_tactic_shape"]]
                
                # Safely delete each key from the session state
                for key in keys_to_delete:
                    if key in st.session_state:
                        del st.session_state[key]
                # -------------------- END OF NEW CODE --------------------

                clear_all_caches()
                st.rerun()
            else:
                st.error(f"Failed to save tactic: {message}")