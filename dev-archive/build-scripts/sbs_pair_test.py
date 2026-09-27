"""Does the freeze hold the world still across a stereo pair? Measured from single screenshots.

With DK_SBS=1 the SDK presents each swap beside the previous one (sdk-patches/07), so ONE window
picture holds two consecutive frames. With the eye offset at zero (DK_CAM_EYE=0) and the eye
flipping every frame (DK_STEREO_PERIOD=1), the two halves of a picture can differ only because the
world moved between the two frames.

Prediction, written before the first run (2026-09-27):
  DK_FREEZE=1 -> about HALF the pictures have identical halves (the pinned second eye shows the same
                 instant as the first), the other half differ (a pair boundary: a new instant).
  DK_FREEZE=0 -> few or no pictures with identical halves while the car scene moves.
  Both about the same share -> the freeze does not hold what reaches the screen (or the capture is
                 stale: check that the halves differ at all in the OFF run before believing ON).

    python sbs_pair_test.py ON|OFF <out dir> [<exe>]

Screenshots are game content: <out dir> must stay OUTSIDE every git repo.
"""
import json, os, subprocess, sys, time
import numpy as np
import vgamepad as vg
from PIL import Image

from winshot import user32, find_window, grab

# ---- settings (named, in one place) --------------------------------------------------
DEFAULT_EXE = r"C:\NonSteam\the-darkness\build-home-sbs\darknessrecomp.exe"
SEQ = "START A DOWN A WAIT A WAIT A WAIT A WAIT".split()   # title -> opening car scene (2026-09-27)
FIRST_PRESS_S = 51.0        # the title screen is up by then on the home PC
STEP_S = 5.6                # between presses; faster than this missed the profile dialogue
WINDOW_WAIT_S = 90.0
SETTLE_S = 20.0             # into the car scene before measuring
SHOTS = 200
SAME_BELOW = 0.05           # mean absolute difference (0-255 scale) that counts as identical halves
TITLE_BAR_PX = 40           # window chrome above the client area in a PrintWindow grab
FOCUS_SETTLE_S = 0.4
PRESS_HOLD_S = 0.12
BUTTONS = {"START": vg.XUSB_BUTTON.XUSB_GAMEPAD_START, "A": vg.XUSB_BUTTON.XUSB_GAMEPAD_A,
           "DOWN": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_DOWN}


def halves_difference(rgb):
    """Mean absolute difference between the left and right halves of the picture area."""
    img = rgb[TITLE_BAR_PX:, :, :].astype(np.int16)
    rows = np.where(img.max(axis=(1, 2)) > 8)[0]          # drop the letterbox bars
    if len(rows) == 0:
        return None
    img = img[rows[0]:rows[-1] + 1]
    w = img.shape[1] // 2
    return float(np.abs(img[:, :w] - img[:, w:2 * w]).mean())


def main():
    mode, out = sys.argv[1], sys.argv[2]
    exe = sys.argv[3] if len(sys.argv) > 3 else DEFAULT_EXE
    os.makedirs(out, exist_ok=True)
    env = dict(os.environ, DK_SBS="1", DK_STEREO_PERIOD="1", DK_CAM_EYE="0",
               DK_FREEZE="1" if mode == "ON" else "0")
    pad = vg.VX360Gamepad()
    time.sleep(1.0)
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe), env=env)
    t0 = time.time()
    print(f"{mode}: launched pid {proc.pid} at {time.strftime('%H:%M:%S')}", flush=True)
    hwnd = None
    while hwnd is None and time.time() - t0 < WINDOW_WAIT_S:
        time.sleep(1.0)
        hwnd = find_window(proc.pid)
    if hwnd is None:
        print("no window"); proc.terminate(); return 2
    while time.time() - t0 < FIRST_PRESS_S:
        time.sleep(1.0)
    for i, label in enumerate(SEQ):
        if proc.poll() is not None:
            print("EXITED", proc.returncode); return 3
        if label != "WAIT":
            user32.SetForegroundWindow(hwnd); time.sleep(FOCUS_SETTLE_S)
            pad.press_button(BUTTONS[label]); pad.update(); time.sleep(PRESS_HOLD_S)
            pad.release_button(BUTTONS[label]); pad.update()
        time.sleep(STEP_S)
    time.sleep(SETTLE_S)
    first = grab(hwnd)
    Image.fromarray(first).save(os.path.join(out, f"{mode}_pair.png"))
    diffs = [halves_difference(grab(hwnd)) for _ in range(SHOTS)]
    diffs = [d for d in diffs if d is not None]
    same = sum(1 for d in diffs if d < SAME_BELOW)
    res = {"mode": mode, "pictures": len(diffs), "identical_halves": same,
           "share_identical": round(same / max(1, len(diffs)), 3),
           "median_difference": round(float(np.median(diffs)) if diffs else -1, 3)}
    print(json.dumps(res), flush=True)
    with open(os.path.join(out, f"{mode}_pairs.json"), "w") as f:
        json.dump({"result": res, "differences": diffs}, f)
    proc.terminate()
    try:
        proc.wait(timeout=15)
    except Exception:
        proc.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main())
