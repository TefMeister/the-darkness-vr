"""Camera-constant trace for The Darkness: drive the right stick in a known pattern and record
exactly when each move happened, so the game's per-frame vertex-constant dump can be lined up
against it. Launches the game, reaches gameplay via the proven pad route, runs the pattern, closes.

Timeline times are Unix epoch MILLISECONDS (time.time()*1000), the same clock the C++ dump writes,
so the two files line up without any offset guessing.
"""
import json, os, subprocess, sys, time
import vgamepad as vg
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import looktest as LT   # tested helpers: find_window, grab, fg_is_game
from PIL import Image

GAME_DIR = r"E:\the-darkness\build-local"
OUT = sys.argv[1] if len(sys.argv) > 1 else r"E:\the-darkness\screens\camtrace\timeline.json"
os.makedirs(os.path.dirname(OUT), exist_ok=True)

MENU = [(55, "START"), (60, "A"), (65, "A"), (70, "START"), (75, "A"), (80, "A"), (85, "A"), (90, "A")]
PATTERN_AT = 112.0
# (label, x, y, seconds). Idle gaps between moves are the in-run no-input controls.
PATTERN = [
    ("idle", 0, 0, 4.0),
    ("right", 1.0, 0, 1.2), ("idle", 0, 0, 3.0),
    ("left", -1.0, 0, 1.2), ("idle", 0, 0, 3.0),
    ("up", 0, 1.0, 1.0), ("idle", 0, 0, 3.0),
    ("down", 0, -1.0, 1.0), ("idle", 0, 0, 3.0),
    ("right", 1.0, 0, 1.2), ("idle", 0, 0, 3.0),
    ("left", -1.0, 0, 1.2), ("idle", 0, 0, 4.0),
]

def ms(): return time.time() * 1000.0

def main():
    pad = vg.VX360Gamepad(); pad.reset(); pad.update()
    proc = subprocess.Popen([os.path.join(GAME_DIR, "darknessrecomp.exe")], cwd=GAME_DIR)
    t0 = time.time()
    T = lambda: time.time() - t0
    tl = {"launch_epoch_ms": t0 * 1000.0, "menu": [], "events": [], "notes": []}
    try:
        for at, btn in MENU:
            while T() < at:
                if proc.poll() is not None:
                    tl["notes"].append(f"game exited at t={T():.1f}s during menus"); return 3
                time.sleep(0.05)
            b = vg.XUSB_BUTTON.XUSB_GAMEPAD_START if btn == "START" else vg.XUSB_BUTTON.XUSB_GAMEPAD_A
            pad.press_button(button=b); pad.update(); time.sleep(0.15)
            pad.release_button(button=b); pad.update()
            tl["menu"].append({"t": round(T(), 2), "button": btn, "game_has_focus": LT.fg_is_game(proc.pid)})
        while T() < PATTERN_AT:
            if proc.poll() is not None:
                tl["notes"].append(f"game exited at t={T():.1f}s before the pattern"); return 3
            time.sleep(0.1)
        hwnd = LT.find_window(proc.pid)
        if hwnd:
            try:
                Image.fromarray(LT.grab(hwnd)).save(os.path.join(os.path.dirname(OUT), "pattern-start.png"))
            except Exception as e:
                tl["notes"].append(f"screenshot failed: {e}")
        for label, x, y, dur in PATTERN:
            start = ms()
            pad.right_joystick_float(x_value_float=float(x), y_value_float=float(y)); pad.update()
            time.sleep(dur)
            pad.right_joystick_float(x_value_float=0.0, y_value_float=0.0); pad.update()
            tl["events"].append({"label": label, "x": x, "y": y, "start_ms": start, "end_ms": ms(),
                                 "game_has_focus": LT.fg_is_game(proc.pid)})
            print(f"t={T():6.1f}s {label:<6} x={x:+.1f} y={y:+.1f} {dur}s", flush=True)
            if proc.poll() is not None:
                tl["notes"].append(f"game exited during pattern at t={T():.1f}s"); break
        time.sleep(1.0)
    finally:
        try:
            pad.right_joystick_float(x_value_float=0.0, y_value_float=0.0); pad.update()
        except Exception:
            pass
        if proc.poll() is None:
            subprocess.run(["taskkill", "/F", "/PID", str(proc.pid)], capture_output=True)
        tl["closed_epoch_ms"] = ms()
        with open(OUT, "w", encoding="utf-8") as f:
            json.dump(tl, f, indent=1)
        print(f"timeline written: {OUT}", flush=True)
    return 0

if __name__ == "__main__":
    sys.exit(main())
