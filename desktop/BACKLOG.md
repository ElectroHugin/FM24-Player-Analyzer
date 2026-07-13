# Backlog: Robustheit, Performance & Code-Qualität

Offene Verbesserungen aus dem Architektur-Review (2026-07-13). Die **Bugs 1–7**
(Datenverlust-/Korruptions-/Crash-Risiken) sind in **v1.2.5** bereits behoben —
diese Liste enthält die verbleibenden Punkte zum späteren Abarbeiten.

Format: `Datei:Zeile` — Problem — Vorschlag. Priorität grob absteigend je Block.

---

## 🟠 Robustheit / latente Gefahren

### 8. DwrsEngine-Plan-Cache ist nicht threadsicher
- **Datei:** [src/core/DwrsEngine.h](src/core/DwrsEngine.h) (`m_planCache`, `mutable`), genutzt aus [src/app/MainWindow.cpp](src/app/MainWindow.cpp) `startDwrsRecalc`, [src/app/ImportRunner.cpp](src/app/ImportRunner.cpp), [src/app/RecalcHelper.cpp](src/app/RecalcHelper.cpp).
- **Problem:** Import-/Recalc-Worker rufen `calculateRole()` über den geteilten Engine-Pointer vom Fremd-Thread. `planFor()` **schreibt bei Cache-Miss** in `m_planCache`. Der Kommentar „touches cached plans only" stimmt nur, wenn alle Pläne schon gebaut sind. Aktuell durch modale Dialoge serialisiert, aber fragil.
- **Vorschlag:** Vor dem Start des Workers alle `validRoles` einmal auf dem UI-Thread `planFor()` aufwärmen (billige Schleife), oder den Cache mit einem `QMutex` schützen.

### 9. Rohe `Player*` in Models/Seiten überleben `reloadFromDatabase()` nicht
- **Datei:** [src/app/widgets/PlayerTableModel.h](src/app/widgets/PlayerTableModel.h) (`m_rows` als `std::vector<const Player*>`), betrifft alle Seiten mit PlayerTableModel.
- **Problem:** `reloadFromDatabase()` ersetzt den kompletten `PlayerStore`-Vektor → alle gehaltenen `Player*` zeigen ins Leere. Gerettet nur dadurch, dass `MainWindow` bei `dataChanged` nur die **aktuelle** Seite refresht. Ein Event, das `data()` einer versteckten Seite triggert, wäre ein Use-after-free.
- **Vorschlag:** Bei `dataChanged` die Rows aller Seiten leeren (billig), oder Rows uid-basiert halten und beim Zugriff auflösen.

### 10. AssignRolesPage: Store-Mutation ohne Revert bei Schreibfehler
- **Datei:** [src/app/pages/AssignRolesPage.cpp](src/app/pages/AssignRolesPage.cpp) `savePending`, `autoAssign`.
- **Problem:** Gleiche Klasse wie Bug 7, aber hier ist die Store-Mutation **absichtlich** (der nachfolgende `recalcDwrsFor` snapshottet den Store mit den neuen Rollen). Bei `upsertPlayers`-Fehler bleibt der Store mutiert, DB nicht → Divergenz bis zum nächsten Reload.
- **Vorschlag:** Alte Rollen vor der Mutation merken und bei Fehler zurücksetzen (Revert-on-failure), statt einfach `return`.

---

## 🟡 Performance (größter Hebel zuerst)

### 11. Voll-Reload nach jedem kleinen Save (größter Alltags-Hebel)
- **Datei:** [src/app/AppContext.cpp](src/app/AppContext.cpp) `reloadFromDatabase`; Aufrufer u. a. TransfersPage, EditPlayerPage, DashboardPage (Abgänge), PlayerProfilePage.
- **Problem:** Jeder kleine Save lädt 35k Spieler + das 416k-Zeilen-Ratings-Aggregat **synchron auf dem UI-Thread** neu (~1,6 s Freeze auf dem bayern-Save) — für 1–20 geänderte Zeilen.
- **Vorschlag:** Geänderte Spieler gezielt im Store patchen (wie es `PlayerActions::togglePlayerFlag` schon tut) und nur `dataChanged` emittieren; Ratings-Cache inkrementell pflegen statt komplett neu aufzubauen.

### 12. `latestDwrsRatings`-Self-Join bei jedem Reload
- **Datei:** [src/core/Database.cpp](src/core/Database.cpp) `latestDwrsRatings`.
- **Problem:** Aggregat (`MAX(ts) GROUP BY player_id, role` + Self-Join) über die komplette History ist der teuerste Teil des Reloads (hängt mit #11 zusammen).
- **Vorschlag:** Eine `dwrs_latest`-Tabelle beim Insert mitpflegen (`INSERT OR REPLACE`), dann ist „latest" ein einfacher Scan. Alternativ ein Index auf `(player_id, role, ts)`.

### 13. `rebuildSuggestions`: 2× Voll-Scan aller Spieler pro Rolle
- **Datei:** [src/app/pages/DashboardPage.cpp](src/app/pages/DashboardPage.cpp) `rebuildSuggestions`.
- **Problem:** ~10 Rollen × 2 Durchläufe × 35k Spieler ≈ 700k Hash-Lookups pro Slider-Tick (debounced 180 ms).
- **Vorschlag:** Pro Rolle nur über die Ratings-Hash der Rolle iterieren (nur bewertete Spieler), nicht über alle Spieler. Club-Bestwert einmalig vorab je Rolle bestimmen.

### 14. Sidebar-Suche baut 80k `QStandardItem`s + faltet keine Akzente
- **Datei:** [src/app/MainWindow.cpp](src/app/MainWindow.cpp) `rebuildSearchModel` (analog PlayerProfilePage).
- **Problem:** Erster Tastendruck erzeugt zehntausende Heap-Objekte. Zudem faltet die Sidebar-Suche **keine Umlaute/Akzente** — die Profil-Suche kann das seit v1.2.3 (`foldForSearch`).
- **Vorschlag:** Schlankes `QAbstractListModel` direkt über den Store; `foldForSearch`-Schlüssel je Player cachen; Sidebar-Suche auf denselben faltenden Completer umstellen (Profil-Muster wiederverwenden).

### 15. `GapAnalysis::playerCanPlaySlot` parst Positionen per Regex pro Slot
- **Datei:** [src/core/GapAnalysis.cpp](src/core/GapAnalysis.cpp) `playerCanPlaySlot`.
- **Problem:** `parsePositionString` (Regex) wird pro Spieler×Slot neu aufgerufen; SquadBuilder cached das bereits, GapAnalysis nicht.
- **Vorschlag:** Positionen einmal je Spieler parsen und cachen (wie [src/core/SquadBuilder.cpp](src/core/SquadBuilder.cpp) `parsedPositions`).

### 16. DB-Settings ohne Cache
- **Datei:** [src/app/AppContext.h](src/app/AppContext.h) (`userClub()`, `secondTeamClub()`, …) → [src/core/Database.cpp](src/core/Database.cpp) `setting`.
- **Problem:** Jeder Aufruf geht an SQLite; mehrfach pro Seiten-Refresh.
- **Vorschlag:** Kleiner In-Memory-Cache der Settings mit Invalidierung in `setSetting`/`removeSetting`.

---

## ⚪ Code-Qualität / Aufräumen

### 17. `NumericItem` dreifach dupliziert
- **Dateien:** [src/app/pages/BestXiPage.cpp](src/app/pages/BestXiPage.cpp), [src/app/pages/NationalBestXiPage.cpp](src/app/pages/NationalBestXiPage.cpp), [src/app/pages/DashboardPage.cpp](src/app/pages/DashboardPage.cpp).
- **Vorschlag:** In ein gemeinsames Header (z. B. `widgets/NumericTableItem.h`) ziehen.

### 18. Timestamp-Parsing dupliziert
- **Dateien:** [src/app/pages/PlayerProfilePage.cpp](src/app/pages/PlayerProfilePage.cpp) (festes `QDateTime::fromString`-Format) vs. [src/app/pages/DwrsProgressPage.cpp](src/app/pages/DwrsProgressPage.cpp) `toMsecs`.
- **Vorschlag:** Einen `parseDwrsTimestamp()`-Helfer in `core/Utils` zentralisieren.

### 19. Tote Schema-Spalten `registration` / `information`
- **Datei:** [src/core/Database.cpp](src/core/Database.cpp) `initSchema` (createPlayers).
- **Problem:** Werden nie gelesen/geschrieben.
- **Vorschlag:** Entfernen (Schema-Migration v1→v2) oder bewusst dokumentieren, falls für später reserviert.

### 20. Toter Placeholder-Pfad
- **Datei:** [src/app/MainWindow.cpp](src/app/MainWindow.cpp) `milestoneFor()` + [src/app/pages/PlaceholderPage.h](src/app/pages/PlaceholderPage.h).
- **Problem:** Alle Menü-Seiten sind implementiert; der Fallback ist unerreichbar.
- **Vorschlag:** Entfernen, oder als bewussten „unknown pageId"-Guard belassen und so kommentieren.

### 21. Widget-Aufräumen inkonsistent
- **Dateien:** [src/app/pages/DashboardPage.cpp](src/app/pages/DashboardPage.cpp) (`deleteLater()`) vs. [src/app/pages/BestXiPage.cpp](src/app/pages/BestXiPage.cpp)/TopRoles (`delete item->widget()` direkt).
- **Vorschlag:** Auf ein Muster vereinheitlichen (bevorzugt `deleteLater()` bei Layout-Neuaufbau innerhalb von Slots).

### 22. `dwrsHistory` baut unbegrenzte `IN (…)`-Klausel
- **Datei:** [src/core/Database.cpp](src/core/Database.cpp) `dwrsHistory`.
- **Problem:** Bei sehr großen id-Listen theoretisch >Platzhalter-Limit (praktisch club-begrenzt).
- **Vorschlag:** Chunking (z. B. 500er-Blöcke) oder Temp-Table-Join.
