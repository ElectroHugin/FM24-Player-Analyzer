# FM Dashboard — Future Ideas (revised)

> Revision note: This file was re-evaluated after a development session that
> shipped Pros & Cons, the Player Profile page, the Squad Gap Analysis tool,
> Tabs + Conditional Formatting, Data-Loading consolidation, and a large round
> of bug fixes / HTML-import hardening. Items are now tagged:
> **[DONE]**, **[OPEN]**, **[RE-EVALUATE]**, **[SCRAPPED]**, **[NEW]**.

---

## ⭐ Recommended next steps (priority order)

1. **[OPEN] Global Player Search in Sidebar** — highest value now, because it
   plugs directly into the new Player Profile page. A search box in the sidebar
   filters `get_all_players()` as you type; clicking a result jumps to the
   Player Profile. Finishes the navigation loop the last session started.
2. **[NEW] Fix `get_master_role_ratings` caching bug** — quick, high-impact.
   In `squad_logic.py` the `st.cache_data` decorator is missing its `@`, so the
   function recomputes every call. Fixing it speeds up Gap Analysis, Best XI,
   Transfers and the National pages.
3. **[NEW] Gap Analysis → Transfer shortlist bridge** — turn a detected gap into
   action: from a flagged slot, show the best available scouted players for that
   slot's role and let the user shortlist them. Natural follow-up to the Gap tool.

---

## Category 1: High-Impact UX & Quality of Life

- **[DONE] Conditional Formatting in Data Tables**
  Shipped via `build_player_table_config` / `display_player_table` (ProgressColumn
  for Average Rating, NumberColumn for Age) on dashboard + national squad tables.
  Matrix pages and Role Analysis already had rainbow `.style` coloring, kept by
  user choice. *Possible extension:* extend ProgressColumn-style bars to more
  tables where `.style` isn't already in use — low priority.

- **[OPEN] Global Player Search in Sidebar** — see "Recommended next steps".

- **[RE-EVALUATE] In-Context Editing with Modals (`st.dialog`)**
  Still nice, but lower priority now that a dedicated Player Profile exists. Could
  be folded INTO the profile (an "edit" button opening a dialog) rather than
  scattering edit icons across tables. Re-scope around the profile page.

---

## Category 2: Deeper Analytical Features

- **[DONE] "Pros & Cons" Analysis for Roles**
  Shipped in `role_analysis_logic.py` (`analyze_player_for_role`) + UI on Role
  Analysis and inside Player Profile. Thresholds documented in the handover.

- **[DONE] Youth Development Squad View** — already present on the Best XI page
  (Youth & Second Team tab). Left as-is.

- **[RE-EVALUATE] Data Management: Settings Export/Import (zip)**
  Still useful for backup/sharing. Now slightly bigger in scope because there
  are more config sections (`[GapAnalysis]` added). Worth doing once the feature
  set stabilizes so the exported bundle is "complete".

- **[NEW] Gap Analysis → Transfer shortlist bridge** — see "Recommended next steps".

- **[NEW] Multi-tactic Gap Overview**
  Run the gap analysis across the user's favorite tactics at once and highlight
  positions that are weak in ALL of them — those are the safest transfer targets
  regardless of which system is played. Direct extension of the new Gap tool.

---

## Category 3: Club Identity & Layout

- **[DONE] Dynamic Personalized Dashboard Header** — `display_custom_header`.
- **[DONE] "Full Club Name" in Settings** — present.
- **[DONE] Leverage Tabs for Cleaner Pages**
  Best XI already used tabs; Role Analysis now uses tabs (club groups); Gap
  Analysis uses XI/B-Team tabs. *Remaining candidate:* none pressing.

- **[OPEN] Dedicated "Player Profile" View**
  Shipped as `page_views/player_profile.py` — header, top roles, pros & cons,
  attribute chips, DWRS chart. **Remaining:** make it the click-through target
  for the Global Player Search (and optionally for player names elsewhere). So:
  the page is done, the "click any name to open it" integration is still OPEN.

---

## Category 4: Component Polish & Storytelling

- **[OPEN] Icons for Visual Cues** — partially in use (page icons, some emojis).
  Could be applied more consistently to subheaders/buttons. Low effort, cosmetic.

- **[NEW] Custom Status Tags (pills)** — partly prototyped: the Player Profile
  already renders APT / Primary / Foot as colored pills via an inline helper.
  Promote that helper into `ui_components.py` and reuse for APT/transfer status
  in tables across the app. (Supersedes the old "Custom Status Tags" idea.)

- **[OPEN] "Manager's Insights" summaries**
  `st.info`/`st.success` natural-language takeaways after big calculations. The
  Gap Analysis "reason" strings are effectively a first version of this. Could
  add a one-line headline insight at the top of Best XI / Gap Analysis.

---

## Proposed Larger Features

- **[DONE-ish] Scouting & Shortlist Management** — a `shortlist.py` page exists
  (currently commented out in the router) and `player_role_matrix.py` has
  shortlist add/remove. **[RE-EVALUATE]:** decide whether to re-enable the
  dedicated shortlist page and add notes/scout-rating columns, or consolidate
  shortlisting into the Gap-Analysis→shortlist bridge above.

- **[RE-EVALUATE] Financial Overview Page**
  Still a clean standalone feature (wage bill, value vs age scatter). The data
  (`Wage`, `Transfer Value`) and a parser (`utils.value_to_float`) already exist.
  Niche but low-risk; do it when you want a "breadth" feature rather than depth.

- **[RE-EVALUATE] Head-to-Head Team Comparison**
  Conceptually nice but heavy: it runs the squad calculation twice. With the
  `get_master_role_ratings` caching bug fixed first, this becomes much cheaper.
  Defer until that fix lands.

---

## Smaller QoL

- **[DONE] Persistent Filters** — implemented on Assign Roles via session_state;
  the pattern is now also used (scope selectors) on Role Analysis / Player Profile.
- **[OPEN] Advanced Player Search** (age/position/attribute filters in an
  expander) — would pair well with the Global Player Search.
- **[OPEN] Data Export (CSV)** — `st.download_button` + `df.to_csv()` on the
  big tables (Role Analysis, Matrix, Gap Analysis). Easy, genuinely useful.
- **[OPEN] Full CRUD for Roles & Tactics** — create exists; edit/delete still
  missing. Medium effort; needs confirmation dialogs.

---

## Technical / Refactoring

- **[DONE] Consolidate Data Loading** — `load_app_data()` in `app.py`. (Was an
  architecture cleanup; caching was already in place.)
- **[DONE] Isolate Business Logic from UI** — established with
  `role_analysis_logic.py` and `gap_analysis_logic.py`. **[OPEN] continuation:**
  apply the same split to `transfer_loan_management.py` (still mixes calculation
  and UI) — move candidate identification into `squad_logic.py`.
- **[NEW] Fix `get_master_role_ratings` caching** — see top.
- **[RE-EVALUATE] Centralize Definitions** (move `GLOBAL_STAT_CATEGORIES` /
  `GK_STAT_CATEGORIES` into `definitions.json`). Still sensible for full
  customizability, but it's invasive and touches the DWRS core — do it only when
  there's appetite for careful regression testing.
- **[NEW] Minimal test harness** — there is no automated test suite. A small
  pytest file around `analytics.calculate_dwrs`, `gap_analysis_logic`, and
  `role_analysis_logic` would protect the math during future refactors.

---

## Scrapped / not recommended

- **[SCRAPPED] Using `Preferred Foot` as a side-preference signal anywhere.**
  Learned the hard way in the Gap Analysis: a strong foot does NOT mean a player
  is out of position. Only the manual `preferred_side` field is meaningful. Keep
  this principle for any future side-aware logic.
- **[SCRAPPED] ProgressColumn bars on Role Analysis / Matrix.** These pages use
  rainbow `.style` coloring which the user prefers; `.style` and `column_config`
  bars are mutually exclusive, so don't try to combine them there.