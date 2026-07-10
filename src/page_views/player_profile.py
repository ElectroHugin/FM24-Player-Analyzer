# player_profile.py

import streamlit as st
import pandas as pd

from sqlite_db import (get_user_club, get_second_team_club, get_all_players,
                       get_latest_dwrs_ratings, get_dwrs_history,
                       get_national_squad_ids, get_national_team_settings,
                       update_dwrs_ratings)
from constants import GLOBAL_STAT_CATEGORIES, GK_STAT_CATEGORIES, get_valid_roles
from config_handler import get_age_threshold
from data_parser import force_update_single_player, load_data
from talent_logic import calculate_talent_score, talent_age_cap_for_player
from utils import (format_role_display, get_last_name, color_attribute_by_value,
                   color_personality, is_national_mode_active)
from ui_components import display_custom_header, display_pros_and_cons, clear_all_caches
from role_analysis_logic import (analyze_player_for_role, get_top_roles_for_player,
                                 parse_attribute_value, ALL_GK_ROLES)


@st.cache_data
def get_profile_pool(scope, user_club, second_club):
    """
    Return all players filtered by scope ('my_club'|'second_team'|'scouted'),
    sorted by last name. Cached per (scope, clubs); cleared by clear_all_caches().
    """
    players = get_all_players()

    if scope == 'my_club':
        players = [p for p in players if p.get('Club') == user_club]
    elif scope == 'second_team':
        players = [p for p in players if p.get('Club') == second_club]
    elif scope == 'scouted':
        exclude = {user_club, second_club} if second_club else {user_club}
        players = [p for p in players if p.get('Club') not in exclude]

    players.sort(key=lambda p: get_last_name(p.get('Name', '')))
    return players


def _status_tag(label, value, color):
    """Render a small colored pill for a qualitative status."""
    if not value:
        return ""
    return (
        f"<span style='background-color:{color}; color:white; padding:3px 10px; "
        f"border-radius:12px; font-size:0.8rem; margin-right:6px; white-space:nowrap;'>"
        f"{label}: {value}</span>"
    )


def _display_key_attributes(player, role):
    """Show the role's key/preferable attributes as compact colored tags."""
    from constants import get_role_specific_weights

    role_weights = get_role_specific_weights().get(role, {"key": [], "preferable": []})
    key_attrs = role_weights.get("key", [])
    pref_attrs = role_weights.get("preferable", [])

    if not key_attrs and not pref_attrs:
        st.caption("No attribute definitions for this role.")
        return

    def render_attr_row(attrs, heading):
        if not attrs:
            return
        st.markdown(f"**{heading}**")
        # Build one HTML block of colored attribute chips
        chips = []
        for attr in attrs:
            val = parse_attribute_value(player.get(attr, 0))
            if val <= 0:
                continue
            style = color_attribute_by_value(int(round(val)))
            chips.append(
                f"<span style='{style} padding:2px 8px; border-radius:6px; "
                f"margin:2px; display:inline-block; font-size:0.85rem;'>"
                f"{attr}: {int(round(val))}</span>"
            )
        if chips:
            st.markdown(" ".join(chips), unsafe_allow_html=True)
        else:
            st.caption("No data for these attributes.")

    render_attr_row(key_attrs, "Key Attributes")
    render_attr_row(pref_attrs, "Preferable Attributes")


def _display_development_chart(player, roles):
    """Line chart of normalized DWRS over time for the player's top roles."""
    uid = player.get('Unique ID')
    history_series = []

    for role in roles:
        hist = get_dwrs_history([uid], role)
        if hist.empty:
            continue
        hist = hist.copy()
        hist['dwrs_normalized'] = pd.to_numeric(
            hist['dwrs_normalized'].astype(str).str.rstrip('%'), errors='coerce'
        )
        series = hist.set_index('snapshot')['dwrs_normalized'].rename(format_role_display(role))
        history_series.append(series)

    if history_series:
        chart_data = pd.concat(history_series, axis=1).interpolate(
            method='linear', limit_direction='forward', axis=0
        )
        st.line_chart(chart_data)
    else:
        st.info("No historical DWRS data yet. Upload more snapshots over time to see development.")


def _display_talent(player, best_dwrs):
    """Show a Talent Score for prospects at or below the youth age threshold.
    Uses the same formula as the Squad Matrix talent filter."""
    outfielder_cap = get_age_threshold('outfielder')
    goalkeeper_cap = get_age_threshold('goalkeeper')
    age_cap = talent_age_cap_for_player(player, outfielder_cap, goalkeeper_cap)

    try:
        age = int(player.get('Age'))
    except (TypeError, ValueError):
        return
    if age > age_cap:
        return  # Not a prospect for this age limit — no talent projection.

    score = calculate_talent_score(
        best_dwrs, age,
        player.get('Determination'), player.get('Work Rate'),
        player.get('Personality'), age_cap,
    )

    st.markdown("### 🌱 Talent Projection")
    c1, c2 = st.columns([1, 3])
    c1.metric(f"Talent Score (U{age_cap})", f"{score:.0f}")
    with c2:
        st.caption(
            f"Prospect under the U{age_cap} development cap. "
            f"Best role DWRS **{best_dwrs}%** + development runway ({age_cap}−{age} yrs) "
            f"+ mentality (Det {player.get('Determination', '?')} / Wor {player.get('Work Rate', '?')}) "
            f"+ personality ({player.get('Personality', '—')})."
        )
    st.divider()


def player_profile_page(players):
    display_custom_header("Player Profile")

    user_club = get_user_club()
    second_club = get_second_team_club()

    if not players:
        st.info("No players loaded. Please upload player data first.")
        return

    # One-shot navigation from the global player search (sidebar): if a target
    # player was set, force the pool to "All Players" so the player is always
    # in scope regardless of his club. The selection itself is applied at the
    # selectbox below. Must run BEFORE the scope radio is instantiated.
    if st.session_state.get("profile_target_uid"):
        st.session_state["profile_scope"] = "all"

    # --- Player selection, scoped like the Role Analysis page ---
    # In National mode the pools are the saved national squad and all players;
    # the club-based scopes make no sense there.
    if is_national_mode_active():
        nat_name, _, _ = get_national_team_settings()
        scope_labels = {
            "national_squad": f"🌟 {nat_name or 'National'} Squad",
            "all": "🌍 All Players",
        }
    else:
        scope_labels = {"my_club": f"🏠 {user_club or 'My Club'}"}
        if second_club:
            scope_labels["second_team"] = f"🔄 {second_club}"
        scope_labels["scouted"] = "🔍 Scouted Players"
        scope_labels["all"] = "🌍 All Players"

    # The scope radio keeps its value across mode switches; drop a value that
    # is not a valid option in the current mode or st.radio raises.
    if st.session_state.get("profile_scope") not in scope_labels:
        st.session_state.pop("profile_scope", None)

    c1, c2 = st.columns([1, 2])
    with c1:
        scope = st.radio(
            "Player pool",
            options=list(scope_labels.keys()),
            format_func=lambda s: scope_labels[s],
            key="profile_scope",
        )

    if scope == "all":
        pool = sorted(get_all_players(), key=lambda p: get_last_name(p.get('Name', '')))
    elif scope == "national_squad":
        squad_ids = get_national_squad_ids()
        pool = sorted(
            [p for p in get_all_players() if p.get('Unique ID') in squad_ids],
            key=lambda p: get_last_name(p.get('Name', ''))
        )
    else:
        pool = get_profile_pool(scope, user_club, second_club)

    if not pool:
        st.info(f"No players found in '{scope_labels[scope]}'.")
        return

    player_map = {
        p['Unique ID']: f"{p.get('Name', 'Unknown')} ({p.get('Club', '-')})"
        for p in pool
    }

    options = list(player_map.keys())

    # One-shot preselection from the global player search. The scope was forced
    # to "all" above, so the target is guaranteed to be a valid option. Popping
    # makes the jump single-use; afterwards the user can change freely.
    target_uid = st.session_state.pop("profile_target_uid", None)
    if target_uid and target_uid in options:
        st.session_state["profile_player_select"] = target_uid

    # Guard: if a previously selected player is no longer in the current pool
    # (e.g. the user switched scope), drop the stale value so st.selectbox does
    # not raise "is not in options".
    if ("profile_player_select" in st.session_state
            and st.session_state["profile_player_select"] not in options):
        del st.session_state["profile_player_select"]

    with c2:
        selected_uid = st.selectbox(
            "Select a player",
            options=options,
            format_func=lambda uid: player_map[uid],
            key="profile_player_select",
        )

    player = next((p for p in pool if p['Unique ID'] == selected_uid), None)
    if not player:
        return

    st.divider()

    # --- Header block: name, vitals, status tags ---
    st.markdown(f"## {player.get('Name', 'Unknown')}")

    vitals = []
    if player.get('Age'):
        vitals.append(f"**Age:** {player['Age']}")
    if player.get('Club'):
        vitals.append(f"**Club:** {player['Club']}")
    if player.get('Position'):
        vitals.append(f"**Position:** {player['Position']}")
    if vitals:
        st.markdown(" &nbsp;|&nbsp; ".join(vitals))

    tags = []
    apt = player.get('Agreed Playing Time')
    if apt:
        tags.append(_status_tag("APT", apt, "#0069b3"))
    primary = player.get('primary_role')
    if primary:
        tags.append(_status_tag("Primary", format_role_display(primary), "#0da025"))
    foot = player.get('Preferred Foot')
    if foot:
        tags.append(_status_tag("Foot", foot, "#6c5ce7"))
    personality = player.get('Personality')
    if personality:
        p_style = color_personality(personality)
        if p_style:
            tags.append(
                f"<span style='{p_style} padding:3px 10px; border-radius:12px; "
                f"font-size:0.8rem; margin-right:6px; white-space:nowrap;'>"
                f"Personality: {personality}</span>"
            )
        else:
            tags.append(_status_tag("Personality", personality, "#555"))
    if tags:
        st.markdown("".join(tags), unsafe_allow_html=True)

    # Flash message from a manual update on the previous run (set before the
    # st.rerun() below, otherwise it would be lost by the rerun).
    flash = st.session_state.pop("profile_update_flash", None)
    if flash:
        st.success(flash)

    # --- Manual single-player update from an HTML export ---
    with st.expander("🔁 Update this player from an HTML file"):
        st.caption(
            "Upload an FM export that contains **only this player**. The data is "
            "written directly onto this profile — even if the UID in the file "
            "differs (e.g. a missing 'r-' prefix on a newgen). Assigned roles, "
            "primary role and transfer/loan settings are kept."
        )
        up_file = st.file_uploader(
            "HTML file with exactly one player",
            type=["html"],
            key=f"profile_update_file_{selected_uid}",
        )
        confirm = st.checkbox(
            f"Yes, I am sure this file is an update for **{player.get('Name', 'this player')}**.",
            key=f"profile_update_confirm_{selected_uid}",
        )
        if st.button("Update player", type="primary", disabled=not (up_file and confirm)):
            with st.spinner("Updating player and recalculating DWRS..."):
                file_player_name, err = force_update_single_player(up_file, selected_uid)
                if err:
                    st.error(f"❌ {err}")
                else:
                    clear_all_caches()
                    fresh_df = load_data()
                    if fresh_df is not None:
                        update_dwrs_ratings(fresh_df, get_valid_roles(), [selected_uid])
                    clear_all_caches()
                    msg = f"✅ {player.get('Name', 'Player')} updated from file."
                    if file_player_name and file_player_name != player.get('Name'):
                        msg += f" (Name in file: '{file_player_name}'.)"
                    st.session_state["profile_update_flash"] = msg
                    st.rerun()

    # --- Top roles by DWRS ---
    latest_ratings = get_latest_dwrs_ratings()
    top_roles = get_top_roles_for_player(player, latest_ratings, limit=5)

    st.markdown("### Top Roles")
    if not top_roles:
        st.info("This player has no rated roles yet. Assign roles on the 'Assign Roles' page.")
        return

    role_cols = st.columns(len(top_roles))
    for i, tr in enumerate(top_roles):
        medal = "🥇 " if i == 0 else ""
        role_cols[i].metric(
            label=f"{medal}{tr['role_name']}",
            value=f"{tr['normalized']}%",
        )

    best_role = top_roles[0]['role']

    st.divider()

    # --- Talent projection for young prospects ---
    _display_talent(player, top_roles[0]['normalized'])

    # --- Pros & Cons for the best role + key attributes side by side ---
    left, right = st.columns(2)
    with left:
        st.markdown(f"### Analysis as {format_role_display(best_role)}")
        analysis = analyze_player_for_role(player, best_role, include_global=True, include_personality=True)
        display_pros_and_cons(analysis)
    with right:
        st.markdown("### Attribute Snapshot")
        _display_key_attributes(player, best_role)

    st.divider()

    # --- Development chart for the top roles ---
    st.markdown("### DWRS Development")
    chart_roles = [tr['role'] for tr in top_roles[:3]]
    _display_development_chart(player, chart_roles)