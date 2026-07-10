# Entwickeln mit Qt Creator

Das Projekt ist ein **CMake-Projekt** — die Projektdatei für Qt Creator ist
[`CMakeLists.txt`](CMakeLists.txt) in diesem Ordner. Eine separate `.pro`-Datei
gibt es bei CMake-Projekten nicht.

## Projekt öffnen

1. Qt Creator starten
2. **Datei → Projekt öffnen…** (oder „Open Project" auf der Startseite)
3. `desktop/CMakeLists.txt` auswählen
4. Beim ersten Öffnen fragt Qt Creator nach der Konfiguration:
   - Qt Creator erkennt unsere **`CMakePresets.json`** automatisch — wähle
     das Preset **`msvc-debug`** (zum Entwickeln/Debuggen) oder
     **`msvc-release`** (volle Optimierung, für Performance-Messungen).
   - Alternativ ein Kit mit **Qt 6.8.3 MSVC2022 64bit** (`C:\Qt\6.8.3\msvc2022_64`)
     und dem **MSVC 2022 x64**-Compiler wählen.

## Bauen und Starten

- **Bauen:** Strg+B (Hammer-Symbol)
- **Starten:** Strg+R — als Run-Target **`fmplayeranalyzer`** auswählen
  (links unten über das Monitor-Symbol). Qt Creator setzt den Qt-`PATH`
  beim Start automatisch, `windeployqt` ist nicht nötig.
- Weitere Targets: `fmmigrate` (Legacy-DB-Migration per Kommandozeile),
  `fmgolden` (Golden-Master-Vergleich gegen die Python-Referenz).

## Tests ausführen

- **Werkzeuge → Tests → Alle Tests ausführen**, oder im Terminal:
  ```
  ctest --preset msvc-debug
  ```
- Qt Creator zeigt die Qt-Test-Suiten (`test_utils`, `test_dwrs`, …) im
  Ausgabefenster „Testergebnisse" einzeln an.

## Hinweise

- Die Build-Ordner liegen unter `desktop/build/<preset>/` und sind gitignoriert.
- Eigene lokale Konfigurationen speichert Qt Creator in
  `CMakeUserPresets.json` bzw. `CMakeLists.txt.user` — beide sind (bzw.
  werden) gitignoriert und gehören nicht ins Repo.
- Kommandozeilen-Build ohne Qt Creator: `.\scripts\build.ps1 -Preset msvc-release -Test`
