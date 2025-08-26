# new_role.py
import streamlit as st

from definitions_handler import get_definitions, save_definitions
from ui_components import clear_all_caches

def create_new_role_page():
    st.title("Create a New Player Role")
    st.info("Define a new field player role. The short name is generated automatically as you type. After creation, the app will reload with the new role available everywhere.")

    # Attribute lists as you defined them
    TECHNICAL_ATTRS = ["Corners", "Crossing", "Dribbling", "Finishing", "First Touch", "Free Kick Taking", "Heading", "Long Shots", "Long Throws", "Marking", "Passing", "Penalty Taking", "Tackling", "Technique"]
    MENTAL_ATTRS = ["Aggression", "Anticipation", "Bravery", "Composure", "Concentration", "Decisions", "Determination", "Flair", "Leadership", "Off the Ball", "Positioning", "Teamwork", "Vision", "Work Rate"]
    PHYSICAL_ATTRS = ["Acceleration", "Agility", "Balance", "Jumping Reach", "Natural Fitness", "Pace", "Stamina", "Strength"]
    ALL_ATTRIBUTES = TECHNICAL_ATTRS + MENTAL_ATTRS + PHYSICAL_ATTRS

    # --- PART 1: Inputs that need live feedback (moved outside the form) ---
    st.subheader("1. Basic Role Information")
    
    c1, c2, c3 = st.columns(3)
    with c1:
        role_name = st.text_input("Full Role Name (e.g., 'Advanced Playmaker')", key="new_role_name", help="The descriptive name of the role.")
    with c2:
        role_cat = st.selectbox("Role Category", ["Defense", "Midfield", "Attack"], key="new_role_cat")
    with c3:
        role_duty = st.selectbox("Role Duty", ["Defend", "Support", "Attack", "Automatic", "Cover", "Stopper"], key="new_role_duty")

    # Auto-generate and display short name dynamically
    if role_name:
        short_name_suggestion = "".join([word[0] for word in role_name.split()]).upper()
        duty_suffix = role_duty[0] if role_duty not in ["Cover", "Stopper"] else role_duty[:2]
        final_short_name = f"{short_name_suggestion}-{duty_suffix}"
        st.write(f"**Generated Short Name:** `{final_short_name}` (This will be the unique ID)")
    else:
        final_short_name = ""
        st.write("**Generated Short Name:** `...`")


    with st.form("new_role_form"):
        st.subheader("2. Position Mapping")
        definitions = get_definitions()
        possible_positions = sorted([p for p in definitions['position_to_role_mapping'].keys() if p != "GK"])
        assigned_positions = st.multiselect("Assign this role to positions:", options=possible_positions, help="Select one or more positions where this role can be used.", key="new_role_positions")

        st.subheader("3. Key and Preferable Attributes")
        st.warning("A 'Key' attribute gets the highest multiplier. A 'Preferable' attribute gets a medium multiplier. If both are checked, 'Key' will be prioritized.")

        tc, mc, pc = st.columns(3)
        with tc:
            st.markdown("#### Technical")
            for attr in TECHNICAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        with mc:
            st.markdown("#### Mental")
            for attr in MENTAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        with pc:
            st.markdown("#### Physical")
            for attr in PHYSICAL_ATTRS:
                cols = st.columns([0.6, 0.2, 0.2])
                cols[0].write(attr)
                cols[1].checkbox("K", key=f"key_{attr}")
                cols[2].checkbox("P", key=f"pref_{attr}")
        
        submitted = st.form_submit_button("Create New Role", type="primary")

    if submitted:
        with st.spinner("Validating and saving new role..."):
            if not role_name or not assigned_positions:
                st.error("Validation Failed: Please provide a Full Role Name and at least one Position Mapping.")
                return
            
            if final_short_name in definitions['role_specific_weights']:
                st.error(f"Validation Failed: The short name '{final_short_name}' already exists. Please choose a different role name or duty.")
                return

            key_attrs, pref_attrs = [], []
            for attr in ALL_ATTRIBUTES:
                if st.session_state.get(f"key_{attr}"):
                    key_attrs.append(attr)
                elif st.session_state.get(f"pref_{attr}"):
                    pref_attrs.append(attr)

            full_role_display_name = f"{role_name} ({role_duty})"
            
            definitions['player_roles'][role_cat][final_short_name] = full_role_display_name
            definitions['role_specific_weights'][final_short_name] = {"key": key_attrs, "preferable": pref_attrs}
            for pos in assigned_positions:
                if pos in definitions['position_to_role_mapping']:
                    definitions['position_to_role_mapping'][pos].append(final_short_name)
                    definitions['position_to_role_mapping'][pos].sort()

            success, message = save_definitions(definitions)

            if success:
                st.success(f"Role '{full_role_display_name}' created successfully! Reloading application...")
                
                # --- FIX: Clear all session state keys to reset the form completely ---
                keys_to_delete = [k for k in st.session_state.keys() if k.startswith('new_role_') or k.startswith('key_') or k.startswith('pref_')]
                for key in keys_to_delete:
                    del st.session_state[key]
                
                clear_all_caches()
                st.rerun()
            else:
                st.error(f"Failed to save role: {message}")