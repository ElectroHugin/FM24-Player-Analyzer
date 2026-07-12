# FM Player Analyzer

*🇬🇧 English · [🇩🇪 Deutsch](README.de.md)*

A native companion tool for **Football Manager** (currently FM 2024 only —
support for other releases can be added): import your scouted and squad players
from the game's HTML exports, rate every player against a large library of
tactical roles with a transparent role-fit score (**DWRS**), and turn a
scouting export into clear squad decisions — Best XI, gap analysis, tactic
explorer, talent scouting, transfer planning and full national-team management.

Built as a fast, offline Windows desktop app (C++ / Qt 6).

---

## Highlights

- **DWRS role-fit rating** — every player scored 0–100 for each role, from
  fully configurable attribute weights, with a colour scale that makes
  strengths and weaknesses obvious at a glance. [How it works ↓](#what-is-dwrs)
- **HTML import** — reads Football Manager's standard export and keeps your
  database up to date as you re-import over a season, auto-assigning roles to
  new players.
- **Squad tools** — Best XI on a tactical pitch (starting XI, B-team, youth &
  second team), gap analysis (obvious *and* hidden weaknesses), and a tactic
  explorer that ranks every formation by how well it fits your squad.
- **Player insight** — profiles with top roles, talent projection and DWRS
  development charts over time; side-by-side comparison with attribute radars.
- **Talent scouting** — a talent score combining current ability, development
  runway, mentality and personality, with domestic/foreign filtering.
- **National-team mode** — build an eligible squad and get its own dashboard,
  matrix, Best XI and call-up suggestions.
- **Your own roles & tactics** — in-app editors write new roles and formations
  that become available everywhere instantly.
- **Modern, fast UX** — light/dark themes, right-click actions on any player
  (open profile, compare, edit, toggle transfer/loan/shortlist) and global
  player search. Handles very large scouting databases smoothly.

---

## What is DWRS?

**DWRS** (*Dynamic Weighted Role Score*) is the heart of the app: a single,
understandable number that rates how well a player fits a specific role. It is
built on a two-layer idea:

1. A player's **general effectiveness**, based on the attributes that matter
   most in the match engine ("meta" attributes).
2. Their **specific aptitude** for a particular role — the attributes that are
   *key* or *preferred* for that job.

**1 · Meta-attribute weighting.** Not all attributes are equally important.
Following community research (notably
[u/florin133's meta-attribute guide](https://www.reddit.com/r/footballmanagergames/comments/16fuksi/a_not_so_short_guide_to_meta_player_attributes/)),
attributes are grouped into importance tiers, each with a default weight:

| Tier | Default weight | Examples |
|---|---|---|
| Extremely Important | 8.0 | Pace, Acceleration |
| Important | 4.0 | Jumping Reach, Anticipation, Balance, Agility, Concentration, Finishing |
| Good | 2.0 | Work Rate, Dribbling, Stamina, Strength, Passing, Determination, Vision |
| Decent | 1.0 | Long Shots, Marking, Decisions, First Touch |
| Almost Irrelevant | 0.2 | Off the Ball, Tackling, Teamwork, Composure, Technique, Positioning |

**2 · Role-specific multipliers.** A strong all-rounder is not automatically
right for a given job. For the role being scored, each attribute marked **Key**
is boosted ×1.5 and each **Preferred** attribute ×1.2 before averaging — so
Passing and Vision count for more on a Ball-Playing Defender, Crossing and
Dribbling on a Winger.

**3 · The score.** For each meta tier, the app averages the player's (boosted)
attributes and multiplies by that tier's weight; summing the tiers gives an
"absolute" score.

**4 · Normalisation to 0–100 %.** The absolute score is scaled between two
benchmarks — a player with 1 in every attribute (0 %) and one with 20 in every
attribute (100 %). So **100 %** means "effectively a 20 in every attribute that
matters for this role", which keeps scores comparable across different roles.

Everything is transparent and adjustable: the tier weights, the key/preferred
multipliers (default **1.5×** / **1.2×**), and which attributes are key or
preferred per role can all be changed under **Settings** (goalkeepers have their
own weights). Tune it to your tactical philosophy and every rating updates.

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

1. In FM, export a player view as an **HTML file** (see
   [FM export](#fm-export-getting-your-data-in) for a ready-made view).
2. Open the app and go to **Dashboard → Import new player data**; select the
   HTML file and let it import and calculate DWRS.
3. Set **My Club** (and optionally a second team) under Settings or on the
   dashboard.
4. Explore: Squad Matrix, Best XI, Gap Analysis and the Tactic Explorer.

## Language

The interface is in **English by default**. A **German** translation is
available under the **Language** menu; switching applies after a quick restart
(your choice is remembered).

## FM export: getting your data in

The app reads Football Manager's standard **HTML export** (in FM: a
squad/scouting view → right-click → *Print/Export* → *Web page (.html)*). Set up
a view once that contains the columns below, then reuse it for every export.
Only **UID** and **Name** are strictly required; every other column improves the
ratings, and any missing attribute is simply treated as unknown.

**Ready-made views:** the custom FM views that export exactly these columns were
created by **PlayingSquirrel** ([@playingsquirrel](https://x.com/playingsquirrel))
and are the easiest way to get started —
[download the FM24 view files here](https://www.mediafire.com/file/ymf6xhw0bk4enjj/FM24_files.zip/file).
Make sure the view includes the **UID** column; for your own club's players you
can also add **Agreed Playing Time** to import that too.

Which columns map to which data is defined per FM release, so future FM versions
can be supported by adding a new mapping — see
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

For newgens, enable FM's *"Use UIDs"* option so players keep a stable ID across
exports — that is what lets the app track the same player over time.

## Included tactics

The formations you can pick in Squad Matrix, Best XI and the Tactic Explorer are
replicated from the **[FM-Arena FM24 Hall of Fame](https://fm-arena.com/table/fm24-hall-of-fame/)** —
a community effort that tests and ranks the most effective FM24 tactics — so you
can evaluate your squad against proven, meta-defining systems out of the box.
Full credit for creating and testing these tactics goes to their authors and the
FM-Arena team. You can also define your own tactics in the app at any time.

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

## Acknowledgements

This project builds on the work of the wider Football Manager community:

- The "meta" attribute-weighting concept was inspired by the research of
  **[u/florin133](https://www.reddit.com/r/footballmanagergames/comments/16fuksi/a_not_so_short_guide_to_meta_player_attributes/)**.
- The custom export views are the work of
  **[PlayingSquirrel](https://x.com/playingsquirrel)**.
- The bundled tactics are replicated from the
  **[FM-Arena FM24 Hall of Fame](https://fm-arena.com/table/fm24-hall-of-fame/)**.
- Early data-view ideas were inspired by the
  **[FM Client App](https://fm-client-app.vercel.app/)**.

## License & disclaimer

Licensed under **GPLv3** — see [LICENSE](LICENSE).

This is an unofficial, fan-made tool and is not endorsed by or affiliated with
Sports Interactive or SEGA. *Football Manager*, the Football Manager logo and
Sports Interactive are trademarks of Sports Interactive Limited; SEGA and the
SEGA logo are trademarks of SEGA Corporation. All in-game data is the property
of Sports Interactive and/or SEGA. The software is provided "as is", without
warranty of any kind.
