"""Stereo probe run: drive to a live 3D level, then turn the camera, and mark the log.

The build now carries a read-only hook on sub_82249580 that logs the projection
matrix P whenever it changes. This run produces the three observations the dossier
asks for:

  1. how often P changes, and how many distinct P there are per frame
  2. whether a perspective P exists at all, distinct from the orthographic HUD ones
  3. whether the perspective P stays put while the LOOK STICK turns the camera --
     the prediction is that it does NOT move, because the turn lives in the view
     matrix M, not in P. If P moves with the stick, the whole injection-point model
     is wrong and has to be revisited before any stereo code is written.

The look phase is bracketed by wall-clock timestamps printed here, so log lines can
be attributed to "before the turn", "during the turn" and "after" without needing
the log and this script to share a clock beyond the second.
"""
import ctypes, ctypes.wintypes as wt, json, os, subprocess, sys, time
import numpy as np
import vgamepad as vg
from PIL import Image

GAME_DIR = r"E:\the-darkness\build-local"
EXE = os.path.join(GAME_DIR, "darknessrecomp.exe")
OUT = r"E:\the-darkness\screens\stereo"
os.makedirs(OUT, exist_ok=True)

# Menu route, measured 2026-09-18: title ~70 s, then START / A(dialogue) / START,
# main menu by ~86 s with CONTINUE highlighted, A to load in.
SCHEDULE = [
    (74.0, "START", "title -> save-notice dialogue"),
    (78.0, "A",     "dismiss the OK dialogue"),
    (82.0, "START", "-> main menu"),
    (91.0, "A",     "CONTINUE -> load the save"),
    (150.0, "SHOT", "expect live 3D by now"),
    (158.0, "SHOT", "same spot, no input"),
    (166.0, "SHOT", "same spot, no input"),
]
LOOK_DEFLECT = 1.0   # full deflection: the profile records that a 0.55 nudge produces no visible turn at all
LOOK_HOLD = 2.0

from winshot import user32, gdi32, kernel32, BIH, find_window, grab


def stamp():
    return time.strftime("%H:%M:%S")


def main():
    pad = vg.VX360Gamepad()
    time.sleep(1.0)
    proc = subprocess.Popen([EXE], cwd=GAME_DIR)
    t0 = time.time()
    print("launched pid %d at %s" % (proc.pid, stamp()))
    hwnd = None
    events = []

    def shot(tag):
        rgb = grab(hwnd)
        colours = len(np.unique(rgb.reshape(-1, 3), axis=0))
        path = os.path.join(OUT, "%s.png" % tag)
        Image.fromarray(rgb).save(path)
        return colours

    def press(button, label):
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.4)
        pad.press_button(button); pad.update()
        time.sleep(0.12)
        pad.release_button(button); pad.update()
        print("        press %-6s at t=%.1f" % (label, time.time() - t0))

    # --- wait for the window, then for the title art -------------------------
    while hwnd is None:
        if time.time() - t0 > 60:
            print("no window after 60 s"); return 2
        time.sleep(2.0)
        hwnd = find_window(proc.pid)
    print("window at t=%.1f" % (time.time() - t0))

    t_title = None
    while t_title is None:
        time.sleep(2.0)
        t = time.time() - t0
        if t > 200:
            print("title art never appeared"); break
        c = shot("wait_t%05.1f" % t)
        print("  t=%5.1f waiting for title, colours=%d" % (t, c))
        if t > 55 and c > 12000:
            t_title = t
    if t_title is None:
        proc.terminate(); return 2
    print("TITLE at t=%.1f" % t_title)

    # --- drive the menus relative to the title -------------------------------
    # The exact chain is title -> START -> save-notice -> A -> title -> START -> main
    # menu -> A(CONTINUE), but boot timing drifts by tens of seconds between runs, so a
    # fixed schedule silently misses every press. Alternating START/A four times reaches
    # the same place from wherever it actually is: A on the title does nothing, START on
    # the menu does nothing, and an extra A on the menu is CONTINUE, which is the goal.
    for i in range(4):
        time.sleep(4.0); press(vg.XUSB_BUTTON.XUSB_GAMEPAD_START, "START")
        time.sleep(4.0); press(vg.XUSB_BUTTON.XUSB_GAMEPAD_A, "A")
        shot("nav_%d" % i)

    # --- wait for gameplay, then hold still and shoot ------------------------
    print("loading, waiting 55 s")
    time.sleep(55.0)
    # BURST capture: grab as fast as possible into memory, stamp each grab with the
    # wall clock to the millisecond, save afterwards. With the eye flipping every few
    # frames, neighbouring grabs are opposite eyes ~0.1 s apart - close enough in time
    # that the moving world barely changes between them.
    import datetime
    burst = []
    for rep in range(3):
        for i in range(25):
            t_before = datetime.datetime.now()
            rgb = grab(hwnd)
            t_after = datetime.datetime.now()
            burst.append((t_before, t_after, rgb))
        time.sleep(4.0)
    for i, (tb, ta, rgb) in enumerate(burst):
        Image.fromarray(rgb).save(os.path.join(OUT, "burst_%02d.png" % i))
        events.append({"tag": "burst_%02d" % i,
                       "t_before": tb.strftime("%H:%M:%S.%f")[:-3],
                       "t_after": ta.strftime("%H:%M:%S.%f")[:-3]})
    print("  burst of %d grabs saved" % len(burst))

    print("closing at %s" % stamp())
    proc.terminate()
    try:
        proc.wait(timeout=15)
    except Exception:
        proc.kill()
    with open(os.path.join(OUT, "events.json"), "w") as f:
        json.dump(events, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
