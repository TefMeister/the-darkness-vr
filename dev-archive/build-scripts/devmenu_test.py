"""Does The Darkness's built-in developer menu open in our build?

The reader found a full DevMenu / GAMEMENU2 tree in retail `Content/Gui/CubeWnd.xcr`
(FreezeCam, Toggle DebugCamera2, Noclip, God-mode, Milestone level jumps), every entrance
wrapped in a gate command `cheat(...)`; and two ENV keys `SHOW_DEVELOPMENT` /
`SHOW_CONFIDENTIAL` read by the same front-end routine that reads QUICKMAP.

This run: set both ENV keys, boot normally, reach the title, press START to get to the
main menu, then press X (pad button index 2) which is what CubeWnd binds the DevMenu
entrance to. Screenshot densely around every press.

Pad is created BEFORE launch so SDL sees it at init (profile note), and the window is
brought to the front before each press.
"""
import ctypes, ctypes.wintypes as wt, json, os, subprocess, sys, time
import numpy as np
import vgamepad as vg
from PIL import Image

GAME_DIR = r"E:\the-darkness\build-local"
EXE = os.path.join(GAME_DIR, "darknessrecomp.exe")
OUT = r"E:\the-darkness\screens\devmenu"
os.makedirs(OUT, exist_ok=True)

# (seconds after launch, action, note)
SCHEDULE = [
    (70.0, "SHOT", "title: PRESS START"),
    (74.0, "START", "title -> save-notice dialogue"),
    (78.0, "A", "dismiss the OK dialogue"),
    (82.0, "START", "second START: now into the main menu"),
    (86.0, "SHOT", "main menu expected here"),
    (90.0, "X", "CubeWnd DevMenu entrance (GUI_BUTTON2)"),
    (94.0, "SHOT", "did a dev menu appear?"),
    (98.0, "X", "second press"),
    (102.0, "SHOT", ""),
    (106.0, "SPACE", "keyboard fallback for the same entrance"),
    (110.0, "SHOT", ""),
    (116.0, "SHOT", "settle"),
]

from winshot import user32, gdi32, kernel32, BIH, find_window, grab, send_key


def main():
    cfg = r"E:\the-darkness\game-files\EnvironmentXbox.cfg"
    print("cfg:", repr(open(cfg).read()))

    pad = vg.VX360Gamepad()          # BEFORE launch, so SDL enumerates it at init
    time.sleep(1.0)
    print("virtual pad created")

    proc = subprocess.Popen([EXE], cwd=GAME_DIR)
    t0 = time.time()
    print("launched pid", proc.pid)
    hwnd = None
    events = []

    for when, action, note in SCHEDULE:
        while time.time() - t0 < when:
            time.sleep(0.2)
        t = round(time.time() - t0, 1)
        if proc.poll() is not None:
            print("PROCESS EXITED at t=%.1f rc=%s" % (t, proc.returncode))
            break
        if hwnd is None:
            hwnd = find_window(proc.pid)
        if hwnd is None:
            print("t=%5.1f  no window" % t)
            continue

        if action != "SHOT":
            user32.SetForegroundWindow(hwnd)
            time.sleep(0.4)
        if action == "START":
            pad.press_button(vg.XUSB_BUTTON.XUSB_GAMEPAD_START); pad.update()
            time.sleep(0.12)
            pad.release_button(vg.XUSB_BUTTON.XUSB_GAMEPAD_START); pad.update()
        elif action == "X":
            pad.press_button(vg.XUSB_BUTTON.XUSB_GAMEPAD_X); pad.update()
            time.sleep(0.12)
            pad.release_button(vg.XUSB_BUTTON.XUSB_GAMEPAD_X); pad.update()
        elif action == "A":
            pad.press_button(vg.XUSB_BUTTON.XUSB_GAMEPAD_A); pad.update()
            time.sleep(0.12)
            pad.release_button(vg.XUSB_BUTTON.XUSB_GAMEPAD_A); pad.update()
        elif action == "SPACE":
            send_key(0x20, 0x39)

        time.sleep(1.2)
        rgb = grab(hwnd)
        colours = len(np.unique(rgb.reshape(-1, 3), axis=0))
        path = os.path.join(OUT, "t%05.1f_%s.png" % (t, action))
        Image.fromarray(rgb).save(path)
        events.append({"t": t, "action": action, "note": note, "colours": int(colours)})
        print("t=%5.1f  %-6s colours=%6d  %s" % (t, action, colours, note))

    print("closing")
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
