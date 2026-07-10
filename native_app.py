#!/usr/bin/env python3
# native_app.py — FM24 Dashboard als echtes Desktop-Fenster (Stufe 3)
#
# Nutzt pywebview: ein natives Fenster mit eigenem Titel — kein Browser nötig.
#   Voraussetzung:  pip install pywebview
#   (Windows nutzt die WebView2-Runtime; auf Win10/11 normalerweise vorinstalliert.)
#
# Verhalten:
#   * Startet den Streamlit-Server unsichtbar, falls er nicht läuft
#     (gleiche Logik wie launcher.py — diese Datei gehört daneben ins Projekt-Root).
#   * Beim Schließen des Fensters: stoppt den Server NUR, wenn dieses Fenster
#     ihn selbst gestartet hat. Lief er schon (z. B. via launcher.py), bleibt er an.
#
# Windows-Verknüpfung (empfohlen):
#   Ziel:  pythonw.exe "C:\...\native_app.py"

import sys

from launcher import URL, ensure_server, stop_server, fail


def main():
    try:
        import webview
    except ImportError:
        fail("pywebview ist nicht installiert.\n\n"
             "Installation:  pip install pywebview")
        return  # unreachable; keeps linters happy

    started_by_us = ensure_server()

    webview.create_window(
        "FM24 Dashboard",
        URL,
        width=1600,
        height=950,
        min_size=(1100, 700),
    )
    # Blocks until the window is closed.
    webview.start()

    if started_by_us:
        stop_server()


if __name__ == "__main__":
    main()
