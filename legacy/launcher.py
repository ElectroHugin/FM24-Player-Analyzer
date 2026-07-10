#!/usr/bin/env python3
# launcher.py — FM24 Dashboard Launcher (Stufe 1)
#
# Doppelklick-Start ohne Terminal:
#   1. Prüft, ob der Streamlit-Server schon läuft (Port 8501).
#   2. Falls nicht: startet ihn UNSICHTBAR im Hintergrund (kein Konsolenfenster),
#      Log nach launcher.log, PID nach .streamlit_server.pid.
#   3. Öffnet die App als rahmenloses Fenster (Chrome/Edge/Brave App-Modus,
#      ohne Adressleiste/Tabs). Fallback: Standardbrowser.
#
# Windows-Verknüpfung (empfohlen):
#   Ziel:  pythonw.exe "C:\...\launcher.py"     -> startet ohne Konsole
# Server beenden:
#   python launcher.py --stop
#
# Diese Datei gehört ins PROJEKT-ROOT (neben src/ und page_views/).

import os
import sys
import time
import socket
import shutil
import subprocess

# --- Configuration ---------------------------------------------------------
PORT = 8501
HOST = "127.0.0.1"
WINDOW_SIZE = "1600,950"

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
APP_SCRIPT = os.path.join(PROJECT_ROOT, "src", "app.py")
LOG_FILE = os.path.join(PROJECT_ROOT, "launcher.log")
PID_FILE = os.path.join(PROJECT_ROOT, ".streamlit_server.pid")
URL = f"http://{HOST}:{PORT}"

# Windows process-creation flags (no console window, detached from launcher)
_CREATE_NO_WINDOW = 0x08000000
_DETACHED_PROCESS = 0x00000008


# --- Core ------------------------------------------------------------------

def server_running():
    """True if something is listening on HOST:PORT."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(0.4)
        return s.connect_ex((HOST, PORT)) == 0


def start_server():
    """Spawn the Streamlit server invisibly in the background."""
    cmd = [
        sys.executable, "-m", "streamlit", "run", APP_SCRIPT,
        "--server.headless", "true",          # don't auto-open a browser tab
        "--server.fileWatcherType", "none",   # no dev file-watching: quieter & faster
        "--server.port", str(PORT),
        "--browser.gatherUsageStats", "false",
    ]
    log = open(LOG_FILE, "a")
    kwargs = dict(cwd=PROJECT_ROOT, stdout=log, stderr=log, stdin=subprocess.DEVNULL)
    if os.name == "nt":
        kwargs["creationflags"] = _CREATE_NO_WINDOW | _DETACHED_PROCESS
    else:
        kwargs["start_new_session"] = True
    proc = subprocess.Popen(cmd, **kwargs)
    with open(PID_FILE, "w") as f:
        f.write(str(proc.pid))
    return proc


def wait_for_server(timeout=40.0):
    """Poll until the server answers (True) or the timeout passes (False)."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if server_running():
            return True
        time.sleep(0.3)
    return False


def ensure_server():
    """Make sure the server is up. Returns True if WE started it just now."""
    if server_running():
        return False
    start_server()
    if not wait_for_server():
        fail(f"Der Server ist nicht innerhalb von 40s gestartet.\n"
             f"Details im Log:\n{LOG_FILE}")
    return True


def stop_server():
    """Stop a server that was started by this launcher (via its PID file)."""
    if not os.path.exists(PID_FILE):
        if server_running():
            print("Server läuft, wurde aber nicht vom Launcher gestartet — bitte dort beenden, wo er gestartet wurde.")
        else:
            print("Kein laufender Launcher-Server gefunden.")
        return
    try:
        pid = int(open(PID_FILE).read().strip())
    except (ValueError, OSError):
        pid = None
    if pid:
        try:
            if os.name == "nt":
                subprocess.run(
                    ["taskkill", "/PID", str(pid), "/T", "/F"],
                    check=False, creationflags=_CREATE_NO_WINDOW,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                )
            else:
                import signal
                try:
                    os.killpg(pid, signal.SIGTERM)   # we spawned a new session
                except (ProcessLookupError, PermissionError, OSError):
                    try:
                        os.kill(pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
        finally:
            pass
    try:
        os.remove(PID_FILE)
    except OSError:
        pass
    print("Server gestoppt.")


# --- UI helpers ------------------------------------------------------------

def find_chromium_browser():
    """Locate a Chromium-based browser that supports --app mode."""
    if os.name == "nt":
        candidates = []
        for env in ("PROGRAMFILES", "PROGRAMFILES(X86)", "LOCALAPPDATA"):
            base = os.environ.get(env)
            if not base:
                continue
            candidates += [
                os.path.join(base, "Google", "Chrome", "Application", "chrome.exe"),
                os.path.join(base, "Microsoft", "Edge", "Application", "msedge.exe"),
                os.path.join(base, "BraveSoftware", "Brave-Browser", "Application", "brave.exe"),
            ]
        for c in candidates:
            if os.path.exists(c):
                return c
        return None
    # POSIX
    for name in ("google-chrome", "google-chrome-stable", "chromium",
                 "chromium-browser", "brave-browser", "microsoft-edge"):
        path = shutil.which(name)
        if path:
            return path
    if sys.platform == "darwin":
        for c in ("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
                  "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"):
            if os.path.exists(c):
                return c
    return None


def open_app_window():
    """Open the dashboard as a chromeless app window; fallback: default browser."""
    browser = find_chromium_browser()
    if browser:
        kwargs = {}
        if os.name == "nt":
            kwargs["creationflags"] = _CREATE_NO_WINDOW
        subprocess.Popen([browser, f"--app={URL}", f"--window-size={WINDOW_SIZE}"], **kwargs)
    else:
        import webbrowser
        webbrowser.open(URL)


def fail(msg):
    """Show an error dialog (no console available in pythonw) and exit."""
    try:
        import tkinter as tk
        from tkinter import messagebox
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("FM24 Dashboard", msg)
        root.destroy()
    except Exception:
        print(msg, file=sys.stderr)
    sys.exit(1)


# --- Entry point -----------------------------------------------------------

def main():
    if "--stop" in sys.argv:
        stop_server()
        return
    if not os.path.exists(APP_SCRIPT):
        fail(f"src/app.py nicht gefunden.\nLiegt launcher.py wirklich im Projekt-Root?\n\nErwartet: {APP_SCRIPT}")
    ensure_server()
    open_app_window()


if __name__ == "__main__":
    main()
