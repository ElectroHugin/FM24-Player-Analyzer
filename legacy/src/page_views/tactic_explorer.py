# page_views/tactic_explorer.py

import streamlit as st
import pandas as pd

from sqlite_db import (get_all_players, get_user_club, get_second_team_club,
                       get_national_squad_ids, get_national_team_settings)
from squad_logic import get_master_role_ratings
from constants import get_tactic_roles
from tactic_explorer_logic import analyze_all_tactics, STRATUM_ORDER
from utils import format_role_display, color_dwrs_by_value, is_national_mode_active
from ui_components import display_custom_header

STRATUM_SHORT = {
    "Goalkeeper": "GK", "Defense": "DEF", "Defensive Midfield": "DM",
    "Midfield": "MID", "Attacking Midfield": "AM", "Strikers": "ST",
}


@st.cache_data
def _run_explorer(user_club, second_team_club):
    """Cached across reruns; rebuilt when player data changes (clear_all_caches)."""
    players = get_all_players()
    clubs = {user_club}
    if second_team_club:
        clubs.add(second_team_club)
    pool = [p for p in players if p.get('Club') in clubs]
    master = get_master_role_ratings(user_club, second_team_club)
    return analyze_all_tactics(pool, master), len(pool)


@st.cache_data
def _run_explorer_national(squad_ids):
    """National variant: pool is the saved national squad (the squad-ID tuple
    is part of the cache key, so re-selecting the squad refreshes the result).
    Club APT is ignored — it must not influence national selection."""
    ids = set(squad_ids)
    pool = [p for p in get_all_players() if p.get('Unique ID') in ids]
    master = get_master_role_ratings()
    return analyze_all_tactics(pool, master, apply_apt_weight=False), len(pool)


def _slots_with_roles(slots, positions):
    return ", ".join(f"{s} ({format_role_display(positions[s])})" for s in slots)


def tactic_explorer_page():
    display_custom_header("Tactic Explorer")
    st.info(
        "Evaluates your squad against **every** tactic to find the best fit — "
        "especially useful when taking over an unfamiliar team. Tactics are "
        "ranked **coverage first** (how many positions you can actually field), "
        "then by **strength** (median DWRS of the best XI). Strength is measured "
        "over filled slots only, so an unfillable position never fakes a low score."
    )

    if is_national_mode_active():
        nat_name, _, _ = get_national_team_settings()
        squad_ids = get_national_squad_ids()
        if not squad_ids:
            st.info("No players have been selected for the national squad yet. Go to 'National Squad' to build your team.")
            return
        pool_label = f"{nat_name or 'National'} squad"
        with st.spinner(f"Analyzing every tactic for the {pool_label}…"):
            results, pool_size = _run_explorer_national(tuple(sorted(squad_ids)))
    else:
        user_club = get_user_club()
        second_team_club = get_second_team_club()
        if not user_club:
            st.warning("Please select your club in the sidebar.")
            return

        pool_label = f"{user_club}" + (f" + {second_team_club}" if second_team_club else "")
        with st.spinner(f"Analyzing every tactic for {pool_label}…"):
            results, pool_size = _run_explorer(user_club, second_team_club)

    if not results:
        st.warning("No tactics defined, or no players in the selected pool.")
        return

    st.caption(f"Pool: {pool_size} players ({pool_label}).")

    # --- Best-fit headline ---
    best = results[0]
    median_txt = f"{int(best['overall_median'])}%" if best['overall_median'] is not None else "–"
    st.success(
        f"🏆 **Best fit: {best['tactic']}** — fills "
        f"{best['filled_slots']}/{best['total_slots']} positions, "
        f"median DWRS {median_txt}."
    )

    # --- Ranked table ---
    strength_cols = ["Median", "Mean"] + [STRATUM_SHORT[s] for s in STRATUM_ORDER]

    rows = []
    for r in results:
        row = {
            "Tactic": r["tactic"],
            "XI": f"{r['filled_slots']}/{r['total_slots']}",
            "Empty": len(r["empty_slots"]),
            "Thin": len(r["thin_slots"]),
            "Median": r["overall_median"],
            "Mean": r["overall_mean"],
        }
        for stratum in STRATUM_ORDER:
            stat = r["per_stratum"].get(stratum)
            row[STRATUM_SHORT[stratum]] = stat["median"] if stat else None
        rows.append(row)

    table = pd.DataFrame(rows)

    def _warn_if_positive(val):
        try:
            return "background-color: rgba(207,30,30,0.55);" if float(val) > 0 else ""
        except (ValueError, TypeError):
            return ""

    styler = table.style.format({c: "{:.0f}" for c in strength_cols}, na_rep="–")
    for c in strength_cols:
        styler = styler.map(color_dwrs_by_value, subset=[c])
    styler = styler.map(_warn_if_positive, subset=["Empty", "Thin"])

    st.dataframe(styler, use_container_width=True, hide_index=True)
    st.caption(
        "DEF / DM / MID / AM / ST = median DWRS per stratum. "
        "**Empty** = slots the best XI couldn't fill · **Thin** = only one eligible player (no backup)."
    )

    # --- Per-tactic detail ---
    st.divider()
    st.subheader("Inspect a tactic")
    tactic_names = [r["tactic"] for r in results]
    selected = st.selectbox("Tactic", options=tactic_names)
    res = next((r for r in results if r["tactic"] == selected), None)
    if not res:
        return

    positions = res["positions"]

    c1, c2, c3 = st.columns(3)
    c1.metric("Positions filled", f"{res['filled_slots']}/{res['total_slots']}")
    c2.metric("Median DWRS", f"{int(res['overall_median'])}%" if res['overall_median'] is not None else "–")
    c3.metric("Avg. options / slot", f"{res['avg_depth']:.1f}")

    if res["uncoverable_slots"]:
        st.error(
            "⛔ **No eligible player at all** for: "
            + _slots_with_roles(res["uncoverable_slots"], positions)
            + " — this formation needs players you don't have."
        )
    if res["empty_slots"]:
        # Empty-in-XI that aren't already flagged as structurally uncoverable
        only_xi_empty = [s for s in res["empty_slots"] if s not in res["uncoverable_slots"]]
        if only_xi_empty:
            st.warning(
                "⚠️ **Left empty in the best XI** (players exist but are needed elsewhere): "
                + _slots_with_roles(only_xi_empty, positions)
            )
    if res["thin_slots"]:
        st.markdown(
            "**Only one option (no backup):** "
            + _slots_with_roles(res["thin_slots"], positions)
        )

    # Per-slot depth table
    depth_rows = [
        {"Slot": s, "Role": format_role_display(positions[s]), "Eligible players": res["eligible_counts"][s]}
        for s in positions
    ]
    depth_df = pd.DataFrame(depth_rows)

    def _depth_color(val):
        try:
            v = int(val)
        except (ValueError, TypeError):
            return ""
        if v == 0:
            return "background-color: rgba(207,30,30,0.55);"
        if v == 1:
            return "background-color: rgba(216,210,30,0.45);"
        return "background-color: rgba(13,160,37,0.35);"

    depth_styler = depth_df.style.map(_depth_color, subset=["Eligible players"])
    st.dataframe(depth_styler, use_container_width=True, hide_index=True)