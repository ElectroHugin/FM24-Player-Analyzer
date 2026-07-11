# FM24 Player Analyzer — Desktop (C++ / Qt 6)

Native Windows port of the FM24 Player Analyzer, optimized for very large
scouting databases (80k+ players).

**Feature-complete** with the legacy Streamlit app: HTML import (incl. the
newgen-ID unification engine and departure detection), DWRS engine, dashboard,
squad matrix with talent filter, role assignment/analysis, player profile /
comparison (radar charts) / development charts, Best XI + gap analysis +
tactic explorer, full national-team mode, transfer/loan management, and the
role/tactic editors. Legacy databases are migrated via
Einstellungen → Datenbank → "Legacy-Datenbank importieren".

## Build requirements

- Visual Studio 2022 Build Tools (C++ workload, x64)
- CMake ≥ 3.28, Ninja
- Qt 6.8 LTS (`msvc2022_64`) with the **Qt Charts** add-on, expected at
  `C:/Qt/6.8.3/msvc2022_64` (adjust `CMakePresets.json` if elsewhere)
- Inno Setup 6 (only for building the installer)

## Building

```powershell
.\scripts\build.ps1 -Preset msvc-release -Test
```

or manually from a *x64 Native Tools* command prompt:

```
cmake --preset msvc-release
cmake --build --preset msvc-release
ctest --preset msvc-release
```

The app binary lands at `build/msvc-release/src/app/fmplayeranalyzer.exe`
(needs `C:\Qt\6.8.3\msvc2022_64\bin` on `PATH` to run outside the installer,
or run `windeployqt` on it).

## Packaging / Installer

```powershell
.\scripts\package.ps1
```

builds the release, stages it with `windeployqt` into `installer/staging/`
and — if Inno Setup 6 is installed — compiles
`installer/output/FM24PlayerAnalyzer-Setup-<version>.exe`. Without Inno Setup
the staging folder works as a portable install. The app version comes from
`src/core/Version.cpp`.

## Layout

- `src/core/` — `fmcore` static library: all domain logic (DWRS engine,
  squad builder, importer, database). Links Qt Core/Sql/Concurrent only —
  no Widgets — so it stays headless-testable.
- `src/app/` — `fmplayeranalyzer` executable: Qt Widgets UI.
- `src/tests/` — Qt Test unit tests and golden-master tests (`ctest`).
- `installer/` — Inno Setup script (built by `scripts/package.ps1`).
