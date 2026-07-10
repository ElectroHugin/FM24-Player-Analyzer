# FM24 Dashboard — Launcher-Anleitung

Alle Dateien gehören ins **Projekt-Root** (derselbe Ordner, in dem `src/` und
`page_views/` liegen):

```
projekt/
├── launcher.py              ← Stufe 1: App-Modus-Fenster (Browser, rahmenlos)
├── native_app.py            ← Stufe 3: echtes Desktop-Fenster (pywebview)
├── FM24-Dashboard.bat       ← Windows-Doppelklick für launcher.py
├── FM24-Dashboard-Nativ.bat ← Windows-Doppelklick für native_app.py
├── src/ …
└── page_views/ …
```

## Stufe 1 — Launcher (Browser-App-Modus)

**Was er tut:** Prüft, ob der Server läuft → startet ihn sonst unsichtbar im
Hintergrund (kein Terminal) → öffnet ein rahmenloses Fenster ohne Adressleiste
(Chrome/Edge/Brave App-Modus). Läuft der Server schon, öffnet sich das Fenster
sofort.

**Windows-Einrichtung (einmalig):**
1. `FM24-Dashboard.bat` doppelklicken — fertig. Oder schöner:
2. Rechtsklick auf Desktop → *Neu → Verknüpfung* → als Ziel:
   `pythonw.exe "C:\Pfad\zum\projekt\launcher.py"`
3. Verknüpfung → *Eigenschaften → Anderes Symbol* → Wunsch-Icon (.ico) wählen.

**Server beenden** (optional — er stört im Hintergrund nicht):
```
python launcher.py --stop
```

## Stufe 3 — Natives Fenster

**Einmalig installieren:**
```
pip install pywebview
```
Dann `FM24-Dashboard-Nativ.bat` doppelklicken (oder Verknüpfung auf
`pythonw.exe …\native_app.py`). Ergebnis: ein echtes Desktop-Fenster mit Titel
„FM24 Dashboard“, komplett ohne Browser.

**Lebenszyklus:** Schließt du das native Fenster, wird der Server automatisch
mitbeendet — aber nur, wenn das Fenster ihn selbst gestartet hat. Lief er
schon (z. B. über Stufe 1), bleibt er an.

Hinweis Windows: pywebview nutzt die WebView2-Runtime (auf Windows 10/11
normalerweise vorinstalliert; sonst einmalig von Microsoft herunterladen).

## Wissenswertes

* **Port:** 8501 (Konstante oben in `launcher.py`, bei Konflikt änderbar).
* **Log:** Server-Ausgaben landen in `launcher.log` im Projekt-Root.
* **PID-Datei:** `.streamlit_server.pid` merkt sich den Hintergrundprozess
  für `--stop`. Beide Dateien gehören in `.gitignore`:
  ```
  launcher.log
  .streamlit_server.pid
  ```
* **Linux/macOS:** `python3 launcher.py` bzw. `python3 native_app.py` —
  gleiche Logik, App-Modus wird mit installiertem Chrome/Chromium/Edge erkannt.
* Der Launcher startet mit `--server.fileWatcherType none` (schneller,
  ruhiger). Zum **Entwickeln** weiterhin klassisch `streamlit run src/app.py`
  benutzen, damit Code-Änderungen live neu laden.
