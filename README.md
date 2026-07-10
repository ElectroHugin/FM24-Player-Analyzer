# FM24 Player Analyzer

A Football Manager 2024 companion tool: import your scouted players from FM's
HTML exports, rate every player against 65 tactical roles with the custom
**DWRS** (role-fit) score, and analyze your squad — Best XI, gap analysis,
tactic explorer, talent scouting, national team management and more.

This repository is a monorepo containing two implementations:

| Directory | Implementation | Status |
|---|---|---|
| [`desktop/`](desktop/) | **C++ / Qt 6 Widgets** — native Windows desktop app, optimized for very large scouting databases (80k+ players) | In development (active) |
| [`legacy/`](legacy/) | **Python / Streamlit** — the original web-UI implementation | Maintained as behavioral reference |

## Desktop app (C++ / Qt)

The modern implementation. Stores its data (SQLite databases, configuration,
role definitions, backups) in `%LOCALAPPDATA%\FM24PlayerAnalyzer` by default;
the data directory is configurable on first run and in the settings page.
Ships as a Windows installer built with Inno Setup.

Build requirements: MSVC 2022, CMake ≥ 3.28, Ninja, Qt 6.8 (with Qt Charts).
See [`desktop/README.md`](desktop/README.md) for build instructions.

## Legacy app (Python / Streamlit)

The original implementation, kept runnable as the reference for feature
parity and golden-master test data. See [`legacy/README.md`](legacy/README.md)
for features and usage; launch with:

```
cd legacy
streamlit run src/app.py
```

Its data lives inside `legacy/` (`databases/`, `backups/`, `config/`). A
one-time migration wizard in the desktop app imports legacy databases and
settings into the new format.

## License

GPLv3 — see [LICENSE](LICENSE).
