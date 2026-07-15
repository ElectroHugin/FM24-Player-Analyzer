# Backlog: Robustheit, Performance & Code-Qualität

Offene Verbesserungen aus dem Architektur-Review (2026-07-13). Die **Bugs 1–7**
(Datenverlust-/Korruptions-/Crash-Risiken) sind in **v1.2.5** behoben.

Format: `Datei:Zeile` — Problem — Vorschlag.

---

## ✅ Erledigt (v1.2.6 – v1.2.8)

Die drei Abarbeitungs-Schritte aus dem Review sind bis auf zwei Restpunkte
umgesetzt:

| Schritt | Version | Punkte |
|---------|---------|--------|
| 1 — Cleanup + contained Perf-Wins | v1.2.6 | 15, 16, 17, 18, 20, 21 |
| 2 — Perf-Hebel Reload/Ratings/Suche + Schema v2 | v1.2.7 | 11 *(teilweise)*, 12, 13, 14, 19 |
| 3 — Robustheit | v1.2.8 | 8, 9, 10 |

Kurzfassung der Umsetzung:

- **#8** DwrsEngine-Plan-Cache: alle `validRoles`-Pläne werden vorab auf dem
  UI-Thread in `reloadConfig()` gebaut → Worker lesen den Cache nur noch.
- **#9** `Player*`-Lebensdauer: `PageBase::releaseStoreRows()` leert bei
  `dataChanged` die rohen `Player*` versteckter Seiten (4 `PlayerTableModel`-
  Seiten + `SquadMatrix::m_scoutedPool`), bevor der Store neu gelesen wird.
- **#10** AssignRolesPage: `savePending`/`autoAssign` mit Revert-on-failure.
- **#12** `dwrs_latest`-Tabelle (materialisiertes „latest je player/role"),
  gepflegt in `appendDwrsRatings`/`mergePlayerInto`; `latestDwrsRatings()` ist
  jetzt ein Scan statt `MAX(ts)`-Self-Join.
- **#13** `rebuildSuggestions` iteriert nur bewertete Spieler je Rolle.
- **#14** Sidebar-Suche: schlankes `PlayerSearchModel` + `FoldingCompleter`
  (Umlaut-/Akzent-Faltung).
- **#15** GapAnalysis cached geparste Positionen je Spieler.
- **#16** In-Memory-Cache der DB-Settings.
- **#17** gemeinsames `widgets/NumericTableItem.h`.
- **#18** `parseDwrsTimestamp()` in `core/Utils`.
- **#19** Schema-Migration v1→v2: `dwrs_latest` + tote Spalten
  `registration`/`information` per `ALTER TABLE DROP COLUMN` entfernt.
- **#20** Milestone-Placeholder → bewusster „unknown pageId"-Guard.
- **#21** Widget-Aufräumen auf `deleteLater()` vereinheitlicht.
- **Nachtrag zu #14 (v1.2.9):** Profilseiten-Suche ebenfalls auf
  `PlayerSearchModel` umgestellt. Der alte `QStandardItemModel`-Aufbau feuerte
  pro Spieler ein `dataChanged`, worauf der Completer jedes Mal mit dem alten
  Suchtext neu filterte (O(n²) → 20–30+ s Freeze beim ersten Tippen nach
  jedem Reload/Scope-Wechsel). Jetzt ein einziger Modell-Reset (<0,5 s).

---

## 🟡 Offen: Rest von #11 — Voll-Reload bei jedem kleinen Save

- **Datei:** [src/app/AppContext.cpp](src/app/AppContext.cpp) `reloadFromDatabase`;
  Aufrufer u. a. TransfersPage, EditPlayerPage, DashboardPage (Abgänge),
  NationalDashboardPage, NationalSquadSelectionPage.
- **Stand:** Der teuerste Teil (Ratings-Aggregat) ist mit **#12** weg, und der
  Recalc-Pfad nutzt jetzt `reloadRatings()` (nur Ratings, Spieler unangetastet).
  Die verbleibenden Save-Pfade laden weiterhin **alle** ~35k Spieler synchron
  neu für 1–20 geänderte Zeilen.
- **Vorschlag:** Geänderte Spieler gezielt im Store patchen (in-place per Row,
  hält `Player*` gültig — verwandt mit **#9**) und nur `dataChanged` emittieren,
  statt den kompletten Store zu ersetzen. Erfordert einen gezielten
  `Database::loadPlayers(ids)`-Pfad und Store-Patch je Aufrufer.

---

## ⚪ Offen: #22 — `dwrsHistory` baut unbegrenzte `IN (…)`-Klausel

- **Datei:** [src/core/Database.cpp](src/core/Database.cpp) `dwrsHistory`.
- **Problem:** Bei sehr großen id-Listen theoretisch > Platzhalter-Limit
  (praktisch club-begrenzt).
- **Vorschlag:** Chunking (z. B. 500er-Blöcke) oder Temp-Table-Join.
