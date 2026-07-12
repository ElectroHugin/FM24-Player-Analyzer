# FM24 Player Analyzer

*🇬🇧 English · [🇩🇪 Deutsch](README.de.md)*

A native companion tool for **Football Manager 2024**: import your scouted
players from FM's HTML exports, rate every player against 65 tactical roles
with the custom **DWRS** role-fit score, and turn a raw 80,000-player scouting
database into actionable squad decisions — Best XI, gap analysis, tactic
explorer, talent scouting, transfer planning and full national-team
management.

Built as a fast, offline Windows desktop app (C++ / Qt 6). Your data never
leaves your machine.

---

## Highlights

- **DWRS role-fit rating** — every player scored 0–100 for each of 65 roles,
  from fully configurable attribute weights, with a colour scale that makes
  strengths and weaknesses obvious at a glance.
- **Smart HTML import** — reads FM's standard export, unifies newgen IDs
  across snapshots, detects players who left your club, and auto-assigns
  roles to newcomers.
- **Squad tools** — Best XI on a tactical pitch (starting XI, B-team, youth &
  second team), gap analysis (obvious *and* hidden weaknesses), and a tactic
  explorer that ranks every formation by how well it fits your squad.
- **Player insight** — profiles with top roles, talent projection and DWRS
  development charts; side-by-side comparison with attribute radars.
- **Talent scouting** — talent score combining current ability, development
  runway, mentality and personality, with domestic/foreign filtering.
- **National-team mode** — build an eligible squad and get its own dashboard,
  matrix, Best XI and call-up suggestions.
- **Your own roles & tactics** — in-app editors write new roles and
  formations that become available everywhere instantly.
- **Modern, fast UX** — light/dark themes, right-click actions on any player
  (open profile, compare, edit, toggle transfer/loan/shortlist), global
  player search, DWRS history over time. Optimised for very large databases.

---

## Installation

**Recommended:** download the latest installer from the
[Releases](../../releases) page and run it. The app is self-contained; no Qt
or other runtime needs to be installed separately.

Your data (SQLite databases, configuration, role definitions and backups) is
stored in `%LOCALAPPDATA%\FM24PlayerAnalyzer` by default. The location is
chosen on first run and can be changed later in the settings — the installer
never touches it, so updates and uninstalls leave your saves intact.

Coming from the older Streamlit version? A one-time migration wizard
(Settings → Database → *Import legacy database*) converts legacy databases and
settings into the new format.

## Quick start

1. In FM, export your player view as an **HTML file** (the standard scouting
   view with a `UID` column).
2. Open the app and go to **Dashboard → Import new player data**; select the
   HTML file and let it import and calculate DWRS.
3. Set **My Club** (and optionally a second team) under Settings or on the
   dashboard.
4. Explore: Squad Matrix, Best XI, Gap Analysis and the Tactic Explorer.

## Language

The interface is in **English by default**. A **German** translation is
available under the **Language** menu; switching language applies after a quick
restart (your choice is remembered).

## FM export: required columns

The app reads Football Manager's standard **HTML export** (Squad view →
right-click → *Print/Export* → Web page). Set up an FM view containing the
columns below, then reuse it for every export. Only **UID** and **Name** are
strictly required; every other column improves the ratings, and any missing
attribute is simply treated as unknown.

Which columns map to which data is defined per FM release, so future FM
versions can be supported by adding a new mapping — see
[`desktop/src/core/Constants.cpp`](desktop/src/core/Constants.cpp)
(`fm24AttributeMapping`). The version you import with is selected under
**Settings → Football Manager Version** (currently *Football Manager 2024*).

**Football Manager 2024** uses these export column headers:

- **Identity & info:** `UID` (required), `Name` (required), `Age`, `Nat`,
  `2nd Nat`, `Club`, `Position`, `Personality`, `Media Handling`,
  `Preferred Foot`, `Left Foot`, `Right Foot`, `Height`, `Wage`,
  `Transfer Value`, `Av Rat`, `Agreed Playing Time`
- **Technical:** `Cor`, `Cro`, `Dri`, `Fin`, `Fir`, `Hea`, `Lon`, `Mar`,
  `Pas`, `Tck`, `Tec`
- **Mental:** `Agg`, `Ant`, `Bra`, `Cmp`, `Cnt`, `Dec`, `Det`, `Fla`, `Ldr`,
  `OtB`, `Pos`, `Tea`, `Vis`, `Wor`
- **Physical:** `Acc`, `Agi`, `Bal`, `Jum`, `Pac`, `Sta`, `Str`
- **Goalkeeping:** `1v1`, `Aer`, `Cmd`, `Han`, `Kic`, `Ref`, `TRO`, `Thr`

For newgens FM must have *"Use UIDs"* enabled so players keep a stable ID
across exports — the import unifies IDs, but a consistent UID is what lets it
track the same player over time.

---

## Repository layout

This is a monorepo with two implementations:

| Directory | Implementation | Status |
|---|---|---|
| [`desktop/`](desktop/) | **C++ / Qt 6 Widgets** — the native Windows app described above | Feature-complete, active |
| [`legacy/`](legacy/) | **Python / Streamlit** — the original web-UI implementation | Kept as behavioural reference / golden-master test data |

### Building from source

Requirements: Visual Studio 2022 (C++, x64), CMake ≥ 3.28, Ninja, and
Qt 6.8 LTS with the Qt Charts add-on.

```powershell
cd desktop
.\scripts\build.ps1 -Preset msvc-release -Test   # build + run tests
.\scripts\package.ps1                             # windeployqt + installer
```

See [`desktop/README.md`](desktop/README.md) for details.

### Legacy app (reference)

```powershell
cd legacy
streamlit run src/app.py
```

Its data lives inside `legacy/` (`databases/`, `backups/`, `config/`). See
[`legacy/README.md`](legacy/README.md) for the original feature notes.

---

## License

GPLv3 — see [LICENSE](LICENSE).
