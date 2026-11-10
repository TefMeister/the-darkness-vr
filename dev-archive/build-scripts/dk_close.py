"""dk_close.py -- close The Darkness the game's own way, and wait until no copy is left (2026-11-10).

    python dk_close.py [seconds to wait, default 30]      exit 0 = closed (or was not running), 1 = still running

Why this exists: on the OpenXR Simulator the process has TWO visible windows, and the simulator's preview window
("OpenXR Simulator - ...") comes first in the window list. A close message sent to "the process's window" lands on
that preview: it goes away and the game plays on, which read as "the game ignores a polite close in a level".
Asked on its OWN window (title starts with "darknessrecomp") the game closes in under a second, in a level too
(log: "Window closing, shutting down..." / "Title terminated"). Measured 2026-11-10 on b025, one run each way.

It also waits until no darknessrecomp.exe is left, so the next launch never starts beside a copy still closing.
Never forces: when it returns 1 the caller decides.
"""
import ctypes
import ctypes.wintypes as W
import subprocess
import sys
import time

EXE = "darknessrecomp.exe"
GAME_TITLE_START = "darknessrecomp"      # the game's own window; the simulator's preview starts "OpenXR Simulator"
WM_CLOSE = 0x0010
DEFAULT_WAIT_S = 30.0
POLL_S = 0.5
user32 = ctypes.windll.user32


def pids():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq " + EXE, "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    return [int(line.split('","')[1]) for line in out.splitlines() if line.lower().startswith('"' + EXE)]


def windows(pid):
    found = []

    @ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    def each(hwnd, _):
        owner = W.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            title = ctypes.create_unicode_buffer(256)
            user32.GetWindowTextW(hwnd, title, 256)
            found.append((hwnd, title.value))
        return True

    user32.EnumWindows(each, 0)
    return found


def main():
    wait_s = float(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_WAIT_S
    running = pids()
    if not running:
        print("not running")
        return 0
    asked = 0
    for pid in running:
        for hwnd, title in windows(pid):
            if title.lower().startswith(GAME_TITLE_START):
                user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
                asked += 1
    if not asked:
        print("no game window found to ask (only: %s)" % [t for p in running for _, t in windows(p)])
    start = time.time()
    while time.time() - start < wait_s:
        if not pids():
            print("closed after %.1f s" % (time.time() - start))
            return 0
        time.sleep(POLL_S)
    print("STILL RUNNING after %.0f s" % wait_s)
    return 1


if __name__ == "__main__":
    sys.exit(main())
