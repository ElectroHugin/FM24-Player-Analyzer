# FM24 Player Analyzer

*[🇬🇧 English](README.md) · 🇩🇪 Deutsch*

Ein natives Begleit-Tool für **Football Manager 2024**: Importiere deine
gescouteten Spieler aus den HTML-Exporten von FM, bewerte jeden Spieler mit dem
eigenen **DWRS**-Rollenscore gegen 65 taktische Rollen und mach aus einer rohen
80.000-Spieler-Scouting-Datenbank konkrete Kader-Entscheidungen — Best XI,
Gap-Analyse, Taktik-Explorer, Talent-Scouting, Transferplanung und komplettes
Nationalmannschafts-Management.

Gebaut als schnelle, offline nutzbare Windows-Desktop-App (C++ / Qt 6). Deine
Daten verlassen deinen Rechner nie.

---

## Highlights

- **DWRS-Rollenscore** — jeder Spieler von 0–100 für jede der 65 Rollen
  bewertet, aus voll konfigurierbaren Attribut-Gewichten, mit einer Farbskala,
  die Stärken und Schwächen auf einen Blick sichtbar macht.
- **Intelligenter HTML-Import** — liest den Standard-Export von FM, vereinheit-
  licht Newgen-IDs über mehrere Snapshots, erkennt Spieler, die deinen Verein
  verlassen haben, und weist Neuzugängen automatisch Rollen zu.
- **Kader-Werkzeuge** — Best XI auf einem taktischen Spielfeld (Startelf,
  B-Team, Jugend & Zweitteam), Gap-Analyse (offensichtliche *und* versteckte
  Schwächen) und ein Taktik-Explorer, der jede Formation danach bewertet, wie
  gut sie zu deinem Kader passt.
- **Spieler-Einblicke** — Profile mit Top-Rollen, Talent-Projektion und
  DWRS-Entwicklungsdiagrammen; Direktvergleich mit Attribut-Radaren.
- **Talent-Scouting** — Talent-Score aus aktueller Stärke, Entwicklungs-
  spielraum, Mentalität und Persönlichkeit, mit Inland-/Ausland-Filter.
- **Nationalmannschafts-Modus** — stelle einen berechtigten Kader zusammen und
  erhalte ein eigenes Dashboard, eine Matrix, Best XI und Nachnominierungs-
  Vorschläge.
- **Eigene Rollen & Taktiken** — In-App-Editoren erstellen neue Rollen und
  Formationen, die sofort überall verfügbar sind.
- **Moderne, schnelle Bedienung** — helle/dunkle Themes, Rechtsklick-Aktionen
  auf jeden Spieler (Profil öffnen, vergleichen, bearbeiten, Transfer/Leihe/
  Shortlist umschalten), globale Spielersuche, DWRS-Verlauf über die Zeit.
  Für sehr große Datenbanken optimiert.

---

## Installation

**Empfohlen:** Lade den neuesten Installer von der
[Releases](../../releases)-Seite herunter und führe ihn aus. Die App ist
in sich abgeschlossen; es muss kein Qt oder eine andere Laufzeitumgebung
separat installiert werden.

Deine Daten (SQLite-Datenbanken, Konfiguration, Rollen-Definitionen und
Backups) liegen standardmäßig in `%LOCALAPPDATA%\FM24PlayerAnalyzer`. Der Ort
wird beim ersten Start gewählt und lässt sich später in den Einstellungen
ändern — der Installer fasst ihn nie an, sodass Updates und Deinstallationen
deine Spielstände unangetastet lassen.

Kommst du von der älteren Streamlit-Version? Ein einmaliger Migrations-
Assistent (Einstellungen → Datenbank → *Legacy-Datenbank importieren*)
konvertiert Legacy-Datenbanken und -Einstellungen ins neue Format.

## Schnellstart

1. Exportiere in FM deine Spieler-Ansicht als **HTML-Datei** (die Standard-
   Scouting-Ansicht mit einer `UID`-Spalte).
2. Öffne die App und geh auf **Dashboard → Neue Spielerdaten importieren**;
   wähle die HTML-Datei und lass sie importieren und den DWRS berechnen.
3. Setze **Mein Verein** (und optional ein Zweitteam) in den Einstellungen oder
   auf dem Dashboard.
4. Erkunde: Squad Matrix, Best XI, Gap-Analyse und den Taktik-Explorer.

## Sprache

Die Oberfläche ist standardmäßig auf **Englisch**. Eine **deutsche** Über-
setzung ist über das Menü **Sprache** (bzw. *Language*) verfügbar; der Wechsel
greift nach einem kurzen Neustart (deine Wahl wird gespeichert).

## FM-Export: benötigte Spalten

Die App liest den Standard-**HTML-Export** von Football Manager (Kader-Ansicht
→ Rechtsklick → *Drucken/Exportieren* → Webseite). Richte in FM eine Ansicht
mit den untenstehenden Spalten ein und nutze sie für jeden Export. Nur **UID**
und **Name** sind zwingend erforderlich; jede weitere Spalte verbessert die
Bewertungen, und fehlende Attribute werden einfach als unbekannt behandelt.

Welche Spalten auf welche Daten abgebildet werden, ist pro FM-Version definiert,
sodass künftige FM-Versionen durch Ergänzen einer neuen Zuordnung unterstützt
werden können — siehe
[`desktop/src/core/Constants.cpp`](desktop/src/core/Constants.cpp)
(`fm24AttributeMapping`). Die Version, mit der du importierst, wählst du unter
**Einstellungen → Football-Manager-Version** (aktuell *Football Manager 2024*).

Für Newgens muss in FM *„UIDs verwenden"* aktiviert sein, damit Spieler über
Exporte hinweg eine stabile ID behalten — der Import vereinheitlicht IDs, aber
eine konsistente UID ist es, die denselben Spieler über die Zeit nachverfolgbar
macht.

---

## Repository-Aufbau

Dies ist ein Monorepo mit zwei Implementierungen:

| Verzeichnis | Implementierung | Status |
|---|---|---|
| [`desktop/`](desktop/) | **C++ / Qt 6 Widgets** — die oben beschriebene native Windows-App | Feature-complete, aktiv |
| [`legacy/`](legacy/) | **Python / Streamlit** — die ursprüngliche Web-UI-Implementierung | Als Verhaltens-Referenz / Golden-Master-Testdaten erhalten |

### Aus dem Quellcode bauen

Voraussetzungen: Visual Studio 2022 (C++, x64), CMake ≥ 3.28, Ninja und
Qt 6.8 LTS mit dem Qt-Charts-Add-on.

```powershell
cd desktop
.\scripts\build.ps1 -Preset msvc-release -Test   # bauen + Tests ausführen
.\scripts\package.ps1                             # windeployqt + Installer
```

Details siehe [`desktop/README.md`](desktop/README.md).

### Legacy-App (Referenz)

```powershell
cd legacy
streamlit run src/app.py
```

Ihre Daten liegen innerhalb von `legacy/` (`databases/`, `backups/`,
`config/`). Siehe [`legacy/README.md`](legacy/README.md) für die ursprünglichen
Feature-Notizen.

---

## Lizenz

GPLv3 — siehe [LICENSE](LICENSE).
