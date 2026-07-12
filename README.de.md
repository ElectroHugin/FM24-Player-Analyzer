# FM Player Analyzer

*[🇬🇧 English](README.md) · 🇩🇪 Deutsch*

Ein natives Begleit-Tool für **Football Manager** (aktuell nur FM 2024 —
weitere Versionen können ergänzt werden): Importiere deine gescouteten und
Kader-Spieler aus den HTML-Exporten des Spiels, bewerte jeden Spieler mit einem
transparenten Rollen-Score (**DWRS**) gegen eine große Auswahl taktischer Rollen
und mach aus einem Scouting-Export klare Kader-Entscheidungen — Best XI,
Gap-Analyse, Taktik-Explorer, Talent-Scouting, Transferplanung und komplettes
Nationalmannschafts-Management.

Gebaut als schnelle, offline nutzbare Windows-Desktop-App (C++ / Qt 6).

---

## Highlights

- **DWRS-Rollenscore** — jeder Spieler von 0–100 für jede Rolle bewertet, aus
  voll konfigurierbaren Attribut-Gewichten, mit einer Farbskala, die Stärken und
  Schwächen auf einen Blick sichtbar macht. [Wie es funktioniert ↓](#was-ist-dwrs)
- **HTML-Import** — liest den Standard-Export von Football Manager und hält deine
  Datenbank über eine Saison hinweg aktuell; Neuzugänge bekommen automatisch
  Rollen zugewiesen.
- **Kader-Werkzeuge** — Best XI auf einem taktischen Spielfeld (Startelf,
  B-Team, Jugend & Zweitteam), Gap-Analyse (offensichtliche *und* versteckte
  Schwächen) und ein Taktik-Explorer, der jede Formation danach bewertet, wie
  gut sie zu deinem Kader passt.
- **Spieler-Einblicke** — Profile mit Top-Rollen, Talent-Projektion und
  DWRS-Entwicklungsdiagrammen über die Zeit; Direktvergleich mit Attribut-Radaren.
- **Talent-Scouting** — ein Talent-Score aus aktueller Stärke, Entwicklungs-
  spielraum, Mentalität und Persönlichkeit, mit Inland-/Ausland-Filter.
- **Nationalmannschafts-Modus** — stelle einen berechtigten Kader zusammen und
  erhalte ein eigenes Dashboard, eine Matrix, Best XI und Nachnominierungs-
  Vorschläge.
- **Eigene Rollen & Taktiken** — In-App-Editoren erstellen neue Rollen und
  Formationen, die sofort überall verfügbar sind.
- **Moderne, schnelle Bedienung** — helle/dunkle Themes, Rechtsklick-Aktionen
  auf jeden Spieler (Profil öffnen, vergleichen, bearbeiten, Transfer/Leihe/
  Shortlist umschalten) und globale Spielersuche. Kommt auch mit sehr großen
  Scouting-Datenbanken flüssig zurecht.

---

## Was ist DWRS?

**DWRS** (*Dynamic Weighted Role Score*) ist das Herzstück der App: eine einzelne,
verständliche Zahl, die bewertet, wie gut ein Spieler zu einer bestimmten Rolle
passt. Sie beruht auf einer zweistufigen Idee:

1. Die **allgemeine Effektivität** eines Spielers, basierend auf den Attributen,
   die in der Match-Engine am meisten zählen ("Meta"-Attribute).
2. Seine **spezifische Eignung** für eine bestimmte Rolle — die Attribute, die
   für diese Aufgabe *Schlüssel* oder *bevorzugt* sind.

**1 · Meta-Attribut-Gewichtung.** Nicht alle Attribute sind gleich wichtig.
Angelehnt an Community-Recherche (insbesondere
[u/florin133s Meta-Attribut-Guide](https://www.reddit.com/r/footballmanagergames/comments/16fuksi/a_not_so_short_guide_to_meta_player_attributes/))
werden Attribute in Wichtigkeits-Stufen mit je einem Standardgewicht gruppiert:

| Stufe | Standardgewicht | Beispiele |
|---|---|---|
| Extremely Important | 8.0 | Pace, Acceleration |
| Important | 4.0 | Jumping Reach, Anticipation, Balance, Agility, Concentration, Finishing |
| Good | 2.0 | Work Rate, Dribbling, Stamina, Strength, Passing, Determination, Vision |
| Decent | 1.0 | Long Shots, Marking, Decisions, First Touch |
| Almost Irrelevant | 0.2 | Off the Ball, Tackling, Teamwork, Composure, Technique, Positioning |

**2 · Rollenspezifische Multiplikatoren.** Ein starker Allrounder ist nicht
automatisch der richtige Spieler für eine bestimmte Aufgabe. Für die bewertete
Rolle wird jedes als **Schlüssel** markierte Attribut vor der Mittelung ×1,5 und
jedes **bevorzugte** Attribut ×1,2 verstärkt — so zählen Passing und Vision bei
einem Ball-Playing Defender mehr, Crossing und Dribbling bei einem Winger.

**3 · Der Score.** Für jede Meta-Stufe mittelt die App die (verstärkten)
Attribute des Spielers und multipliziert mit dem Gewicht der Stufe; die Summe
aller Stufen ergibt einen "absoluten" Score.

**4 · Normalisierung auf 0–100 %.** Der absolute Score wird zwischen zwei
Referenzpunkten skaliert — ein Spieler mit 1 in jedem Attribut (0 %) und einer
mit 20 in jedem Attribut (100 %). **100 %** bedeutet also "praktisch eine 20 in
jedem für diese Rolle relevanten Attribut", was die Scores über verschiedene
Rollen hinweg vergleichbar hält.

Alles ist transparent und anpassbar: die Stufen-Gewichte, die Schlüssel-/
Bevorzugt-Multiplikatoren (Standard **1,5×** / **1,2×**) und welche Attribute pro
Rolle Schlüssel oder bevorzugt sind, lassen sich unter **Einstellungen** ändern
(Torhüter haben eigene Gewichte). Passe es an deine taktische Philosophie an,
und jede Bewertung aktualisiert sich.

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

1. Exportiere in FM eine Spieler-Ansicht als **HTML-Datei** (siehe
   [FM-Export](#fm-export-so-kommen-deine-daten-rein) für eine fertige Ansicht).
2. Öffne die App und geh auf **Dashboard → Neue Spielerdaten importieren**;
   wähle die HTML-Datei und lass sie importieren und den DWRS berechnen.
3. Setze **Mein Verein** (und optional ein Zweitteam) in den Einstellungen oder
   auf dem Dashboard.
4. Erkunde: Squad Matrix, Best XI, Gap-Analyse und den Taktik-Explorer.

## Sprache

Die Oberfläche ist standardmäßig auf **Englisch**. Eine **deutsche** Über-
setzung ist über das Menü **Sprache** (bzw. *Language*) verfügbar; der Wechsel
greift nach einem kurzen Neustart (deine Wahl wird gespeichert).

## FM-Export: So kommen deine Daten rein

Die App liest den Standard-**HTML-Export** von Football Manager (in FM: eine
Kader-/Scouting-Ansicht → Rechtsklick → *Drucken/Exportieren* →
*Webseite (.html)*). Richte einmal eine Ansicht mit den untenstehenden Spalten
ein und nutze sie für jeden Export. Nur **UID** und **Name** sind zwingend
erforderlich; jede weitere Spalte verbessert die Bewertungen, und fehlende
Attribute werden einfach als unbekannt behandelt.

**Fertige Ansichten:** Die benutzerdefinierten FM-Ansichten, die genau diese
Spalten exportieren, stammen von **PlayingSquirrel**
([@playingsquirrel](https://x.com/playingsquirrel)) und sind der einfachste
Einstieg —
[hier gibt es die FM24-Ansichtsdateien](https://www.mediafire.com/file/ymf6xhw0bk4enjj/FM24_files.zip/file).
Achte darauf, dass die Ansicht die **UID**-Spalte enthält; für die Spieler
deines eigenen Vereins kannst du zusätzlich **Agreed Playing Time** aufnehmen,
um auch das zu importieren.

Welche Spalten auf welche Daten abgebildet werden, ist pro FM-Version definiert,
sodass künftige FM-Versionen durch Ergänzen einer neuen Zuordnung unterstützt
werden können — siehe
[`desktop/src/core/Constants.cpp`](desktop/src/core/Constants.cpp)
(`fm24AttributeMapping`). Die Version, mit der du importierst, wählst du unter
**Einstellungen → Football-Manager-Version** (aktuell *Football Manager 2024*).

**Football Manager 2024** nutzt diese Export-Spaltenüberschriften:

- **Identität & Info:** `UID` (erforderlich), `Name` (erforderlich), `Age`,
  `Nat`, `2nd Nat`, `Club`, `Position`, `Personality`, `Media Handling`,
  `Preferred Foot`, `Left Foot`, `Right Foot`, `Height`, `Wage`,
  `Transfer Value`, `Av Rat`, `Agreed Playing Time`
- **Technisch:** `Cor`, `Cro`, `Dri`, `Fin`, `Fir`, `Hea`, `Lon`, `Mar`,
  `Pas`, `Tck`, `Tec`
- **Mental:** `Agg`, `Ant`, `Bra`, `Cmp`, `Cnt`, `Dec`, `Det`, `Fla`, `Ldr`,
  `OtB`, `Pos`, `Tea`, `Vis`, `Wor`
- **Physisch:** `Acc`, `Agi`, `Bal`, `Jum`, `Pac`, `Sta`, `Str`
- **Torwart:** `1v1`, `Aer`, `Cmd`, `Han`, `Kic`, `Ref`, `TRO`, `Thr`

Für Newgens aktiviere in FM die Option *„UIDs verwenden"*, damit Spieler über
Exporte hinweg eine stabile ID behalten — nur so kann die App denselben Spieler
über die Zeit nachverfolgen.

## Enthaltene Taktiken

Die Formationen, die du in Squad Matrix, Best XI und Taktik-Explorer wählen
kannst, sind aus der
**[FM-Arena FM24 Hall of Fame](https://fm-arena.com/table/fm24-hall-of-fame/)**
nachgebaut — einem Community-Projekt, das die effektivsten FM24-Taktiken testet
und rankt — sodass du deinen Kader von Anfang an gegen erprobte, meta-prägende
Systeme bewerten kannst. Voller Dank für Erstellung und Test dieser Taktiken
geht an ihre Autoren und das FM-Arena-Team. Du kannst jederzeit auch eigene
Taktiken in der App definieren.

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

## Danksagungen

Dieses Projekt baut auf der Arbeit der breiteren Football-Manager-Community auf:

- Das Konzept der „Meta"-Attribut-Gewichtung wurde von der Recherche von
  **[u/florin133](https://www.reddit.com/r/footballmanagergames/comments/16fuksi/a_not_so_short_guide_to_meta_player_attributes/)**
  inspiriert.
- Die benutzerdefinierten Export-Ansichten stammen von
  **[PlayingSquirrel](https://x.com/playingsquirrel)**.
- Die enthaltenen Taktiken sind aus der
  **[FM-Arena FM24 Hall of Fame](https://fm-arena.com/table/fm24-hall-of-fame/)**
  nachgebaut.
- Frühe Ideen für die Datenansichten wurden von der
  **[FM Client App](https://fm-client-app.vercel.app/)** inspiriert.

## Lizenz & Haftungsausschluss

Lizenziert unter **GPLv3** — siehe [LICENSE](LICENSE).

Dies ist ein inoffizielles, von Fans erstelltes Tool und wird von Sports
Interactive oder SEGA weder unterstützt noch ist es mit ihnen verbunden.
*Football Manager*, das Football-Manager-Logo und Sports Interactive sind
Marken von Sports Interactive Limited; SEGA und das SEGA-Logo sind Marken der
SEGA Corporation. Alle spielinternen Daten sind Eigentum von Sports Interactive
und/oder SEGA. Die Software wird „wie besehen" bereitgestellt, ohne Gewährleistung
jeglicher Art.
