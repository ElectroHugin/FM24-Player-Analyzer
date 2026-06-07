# page_views/gap_analysis.py

import streamlit as st
import pandas as pd

from constants import get_tactic_roles, get_tactic_layouts
from sqlite_db import get_user_club, get_second_team_club, get_favorite_tactics
from config_handler import (get_theme_settings, get_gap_analysis_setting)
from squad_logic import get_cached_squad_analysis
from gap_analysis_logic import analyze_team_gaps
from utils import format_role_display
from ui_components import display_custom_header


def _gap_type_label(gap):
    if gap["player_name"] in ("—", "-"):
        return "⛔ Empty"
    if gap["is_displacement"] and gap["is_dropoff"]:
        return "🕳️⚠️ Hidden + Obvious"
    if gap["is_displacement"]:
        return "🕳️ Hidden"
    if gap["is_dropoff"]:
        return "⚠️ Obvious"
    return ""


def _build_gap_table(gaps):
    """Turn the gap dicts into a display DataFrame."""
    rows = []
    for g in gaps:
        rows.append({
            "Slot": g["slot"],
            "Role": format_role_display(g["role"]),
            "Player": g["player_name"],
            "DWRS": int(round(g["assigned_dwrs"])) if g["assigned_dwrs"] else 0,
            "Type": _gap_type_label(g),
            "Gap Score": round(g["gap_score"], 1),
            "Why": g["reason"],
        })
    return pd.DataFrame(rows)


def _render_gap_pitch(gaps, positions, layout):
    """A simple pitch view coloring each slot by its gap score (red = big gap)."""
    gap_by_slot = {g["slot"]: g for g in gaps}
    max_score = max((g["gap_score"] for g in gaps), default=0.0)

    def cell_color(slot):
        g = gap_by_slot.get(slot)
        if not g:
            return "rgba(40, 120, 60, 0.85)"  # green: no gap
        # Normalize 0..1 against the worst gap on the pitch
        norm = (g["gap_score"] / max_score) if max_score > 0 else 0
        # green -> red
        r = int(40 + norm * 180)
        green = int(120 - norm * 90)
        return f"rgba({r}, {green}, 50, 0.9)"

    # Render strata top (Strikers) to bottom (GK), matching display_tactic_grid order
    strata_order = ["Strikers", "Attacking Midfield", "Midfield", "Defensive Midfield", "Defense"]
    for stratum in strata_order:
        slots = layout.get(stratum, [])
        if not slots:
            continue
        cols = st.columns(len(slots))
        for i, slot in enumerate(slots):
            g = gap_by_slot.get(slot)
            role = positions.get(slot, "")
            label = format_role_display(role)
            dwrs = int(round(g["assigned_dwrs"])) if (g and g["assigned_dwrs"]) else "-"
            name = g["player_name"] if g else "OK"
            with cols[i]:
                st.markdown(
                    f"<div style='background:{cell_color(slot)}; border-radius:8px; "
                    f"padding:8px; text-align:center; color:white; margin:2px;'>"
                    f"<strong>{slot}</strong><br><small>{label}</small><br>"
                    f"{name} ({dwrs}%)</div>",
                    unsafe_allow_html=True,
                )
    # GK row
    if "GK" in positions:
        g = gap_by_slot.get("GK")
        dwrs = int(round(g["assigned_dwrs"])) if (g and g["assigned_dwrs"]) else "-"
        name = g["player_name"] if g else "OK"
        c = st.columns([2, 1, 2])
        with c[1]:
            st.markdown(
                f"<div style='background:{cell_color('GK')}; border-radius:8px; "
                f"padding:8px; text-align:center; color:white; margin:2px;'>"
                f"<strong>GK</strong><br><small>{format_role_display(positions['GK'])}</small><br>"
                f"{name} ({dwrs}%)</div>",
                unsafe_allow_html=True,
            )


def _display_team_gaps(team, positions, layout, players_by_id, master_ratings, settings):
    gaps = analyze_team_gaps(
        team, positions, players_by_id, master_ratings,
        displacement_threshold=settings["displacement_threshold"],
        dropoff_threshold=settings["dropoff_threshold"],
        wrong_side_penalty=settings["wrong_side_penalty"],
    )

    if not gaps:
        st.success("✅ No significant gaps found for this team with the current thresholds.")
        return

    df = _build_gap_table(gaps)

    # Conditional formatting on the Gap Score (higher = worse = redder)
    max_score = df["Gap Score"].max() if not df.empty else 1
    def color_gap(val):
        try:
            norm = float(val) / max_score if max_score else 0
        except (ValueError, TypeError):
            return ""
        r = int(200 * norm + 40)
        g = int(160 * (1 - norm) + 40)
        return f"background-color: rgba({r},{g},50,0.55);"

    styler = df.style.map(color_gap, subset=["Gap Score"])
    st.dataframe(styler, use_container_width=True, hide_index=True)

    with st.expander("🟩 Show on pitch"):
        _render_gap_pitch(gaps, positions, layout)


def gap_analysis_page(players):
    display_custom_header("Squad Gap Analysis")
    st.info(
        "Finds weaknesses in your Best XI and B-Team — both **obvious** gaps "
        "(a starter well below the team median) and **hidden** gaps (a good player "
        "pulled out of his best slot, or onto the wrong side). Use it to target the "
        "right transfers instead of reinforcing a position that only *looks* weak."
    )

    user_club = get_user_club()
    second_team_club = get_second_team_club()
    if not user_club:
        st.warning("Please select your club in the sidebar.")
        return

    # Tactic selection — favorites first, matching the Best XI page
    fav_tactic1, fav_tactic2 = get_favorite_tactics()
    all_tactics = sorted(list(get_tactic_roles().keys()))
    if not all_tactics:
        st.warning("No tactics defined yet.")
        return
    sorted_tactics = []
    if fav_tactic1 and fav_tactic1 in all_tactics:
        sorted_tactics.append(fav_tactic1)
    if fav_tactic2 and fav_tactic2 in all_tactics and fav_tactic2 != fav_tactic1:
        sorted_tactics.append(fav_tactic2)
    for t in all_tactics:
        if t not in sorted_tactics:
            sorted_tactics.append(t)
    tactic = st.selectbox("Select Tactic", options=sorted_tactics, index=0)

    positions = get_tactic_roles()[tactic]
    layout = get_tactic_layouts().get(tactic, {})

    settings = {
        "displacement_threshold": get_gap_analysis_setting('displacement_threshold'),
        "dropoff_threshold": get_gap_analysis_setting('dropoff_threshold'),
        "wrong_side_penalty": get_gap_analysis_setting('wrong_side_penalty'),
    }
    st.caption(
        f"Thresholds — Hidden: {settings['displacement_threshold']:.1f} · "
        f"Obvious: {settings['dropoff_threshold']:.1f} · "
        f"Wrong-side penalty: {settings['wrong_side_penalty']:.1f}  "
        f"(adjust in Settings → Gap Analysis Thresholds)"
    )

    with st.spinner("Analyzing squad structure..."):
        analysis = get_cached_squad_analysis(players, tactic, user_club, second_team_club)

    if not analysis:
        st.warning(f"Could not analyze the '{tactic}' tactic — no suitable players found.")
        return

    first_team = analysis["first_team_squad_data"]
    master_ratings = analysis["master_role_ratings"]
    my_club_players = analysis["my_club_players"]
    players_by_id = {p['Unique ID']: p for p in my_club_players}

    xi_tab, b_tab = st.tabs(["🏆 Starting XI", "🅱️ B-Team"])
    with xi_tab:
        _display_team_gaps(first_team["starting_xi"], positions, layout,
                           players_by_id, master_ratings, settings)
    with b_tab:
        _display_team_gaps(first_team["b_team"], positions, layout,
                           players_by_id, master_ratings, settings)