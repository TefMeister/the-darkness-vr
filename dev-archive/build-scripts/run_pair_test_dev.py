"""Dev-PC A/B runner for the eye-by-packet change (2026-10-12). Written by the static reader, NOT yet run.

    python run_pair_test_dev.py A            the old per-frame eye (no DK_EYE_BY_PACKET)
    python run_pair_test_dev.py B            the same plus DK_EYE_BY_PACKET=1
    python run_pair_test_dev.py H            B plus DK_SBS_HOLD=1 (needs sdk-patches/08b): only same-pair pictures shown
    python run_pair_test_dev.py O            H with DK_FREEZE=0: the freeze-off baseline
    python run_pair_test_dev.py AB | ABH | BHO   those runs one after the other
    python run_pair_test_dev.py NF --scene car --turn   screen-effects test: N = normal flat picture, F = with
                                             DK_VR_SCREEN_FX_OFF=1 (needs vr_viewport_probe_fx.cpp); prints the
                                             picture sharpness while turning and while still
    options: --shots N (default 200)  --scene menu|car (default menu)  --no-obs

Both runs: DK_SBS=1 DK_STEREO_PERIOD=1 DK_CAM_EYE=1.25 DK_FREEZE=1. Each picture holds two swaps side by
side (sdk-patches/07, or 08 when installed). With a real eye offset the two halves of a correct pair are
shifted sideways against each other (about 21 px in a 640-wide half, 2026-09-28); a pair with ONE eye on
both halves is not shifted. So the measure is the sideways shift that best lines the halves up:
  - prediction A: a large share (30-50%) of pictures at shift ~0, the rest at one sign;
  - prediction B: almost none at ~0, and all of the rest at the SAME sign (left eye always on the left).
  - prediction H: as B, and "identical halves" no longer possible at offset 1.25; with DK_CAM_EYE=0 every
    picture would be a same-instant pair (the [SBS] HOLD line shows pairs_shown and the orphans).
The old "identical halves" share is printed too. ⚠️ Whether the eye offset shows on the main-menu
background at all is unchecked: the 42 px was measured on the car scene. If A and B both give mostly 0
on the menu, rerun with --scene car (CONTINUE, then a mostly still scene; best for the shift).

Route (this PC has a save: Documents\\darknessrecomp\\...\\545407EE\\00000001\\DEFAULT, Chapter1 +
Checkpoint, 2026-09-16): wait for the title art, then START (title), A (save notice), START (back on the
title on the 2026-09-18 dev route; does nothing if the menu is already up) -> main menu, CONTINUE
highlighted. --scene car adds A (CONTINUE) and waits for the level.

Pictures go OUTSIDE every git repo: D:\\the-darkness\\screens\\eye-by-packet\\<mode>-<time>\\.
"""
import argparse, ctypes, glob, json, os, re, subprocess, sys, time

BUILD_SCRIPTS = r"D:\claude video game stuff\github-backups\the-darkness-vr\dev-archive\build-scripts"
OBS_REC = r"D:\claude video game stuff\github-backups\claude-memory\tools\obs-rec.py"
sys.path.insert(0, BUILD_SCRIPTS)

import numpy as np                                    # noqa: E402
import vgamepad as vg                                 # noqa: E402
from PIL import Image                                 # noqa: E402
from winshot import user32, find_window, grab         # noqa: E402
from sbs_pair_test import client_box, halves_difference, SAME_BELOW   # noqa: E402

# ---- settings (named, in one place) ----------------------------------------------------------------
GAME_DIR = r"D:\the-darkness\build-local"
EXE = os.path.join(GAME_DIR, "darknessrecomp.exe")
LOG_DIR = os.path.join(GAME_DIR, "logs")
OUT_ROOT = r"D:\the-darkness\screens\eye-by-packet"
BASE_ENV = {"DK_SBS": "1", "DK_STEREO_PERIOD": "1", "DK_CAM_EYE": "1.25", "DK_FREEZE": "1"}
WINDOW_WAIT_S = 90.0
TITLE_EARLIEST_S = 55.0       # dev PC title art came up at ~70 s on 2026-09-18
TITLE_LATEST_S = 200.0
TITLE_COLOURS = 12000         # distinct colours that mean "title art is up" (stereo_probe_run, dev PC)
STEP_S = 4.5                  # between menu presses
MENU_SETTLE_S = 15.0          # on the main menu before measuring
MENU_REF_WAIT_S = 8.0         # on the main menu before the reference picture
CONTINUE_CHECK_S = 6.0        # after A on CONTINUE, before comparing with the reference
CONTINUE_TRIES = 6
MENU_LEFT_DIFF = 12.0         # mean absolute picture change (0-255) that means it left the menu
LEVEL_WAIT_S = 120.0          # the opening film and titles play before the level
LOG_POLL_S = 3.0
LEVEL_SETTLE_S = 5.0          # after the first camera builds, before measuring
CAMERA_1_RE = re.compile(r"camera builds per frame: 0:\d+ 1:(\d+)")
CAM_IN_PACKET_RE = re.compile(r"cam_in_packet=(\d+)")
KEEP_PICTURES = 12
MAX_SHIFT_PX = 40             # sideways shifts tried, each way, in window pixels
SAME_EYE_SHIFT_PX = 2         # |best shift| at or below this = one eye on both halves
CLOSE_WAIT_S = 20.0
FOCUS_SETTLE_S = 0.4
PRESS_HOLD_S = 0.12
WM_CLOSE = 0x0010
TURN_BURST_SHOTS = 10         # --turn: 10 pictures with the right stick held, then 10 with it centred
TURN_DEFLECT = 1.0            # 0.55 produced no visible turn (control profile, 2026-09-16)
user32.PostMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_size_t, ctypes.c_ssize_t]
LOG_KEYS = ("[VP] P-offset", "[VP] packet", "[VP] camera build", "[VP] first FILLER", "[VP] PACKETS",
            "[VP] SWAPS", "[VP] FREEZE", "[SBS]", "[VP] screen", "[VP] PAIRS", "[VP] PINCALLERS")
BUTTONS = {"START": vg.XUSB_BUTTON.XUSB_GAMEPAD_START, "A": vg.XUSB_BUTTON.XUSB_GAMEPAD_A}


def best_shift(rgb, box):
    """Sideways shift (px) that best lines the right half up with the left, and the difference there."""
    x, y, w, h = box
    img = rgb[y:y + h, x:x + w, :].astype(np.float32).mean(axis=2)
    rows = np.where(img.max(axis=1) > 8)[0]            # drop the letterbox bars
    if len(rows) == 0:
        return None, None
    img = img[rows[0]:rows[-1] + 1]
    half = img.shape[1] // 2
    left, right = img[:, 2:half - 2], img[:, half + 2:2 * half - 2]
    best = (None, 1e9)
    for s in range(-MAX_SHIFT_PX, MAX_SHIFT_PX + 1):
        a = left[:, max(0, s):left.shape[1] + min(0, s)]
        b = right[:, max(0, -s):right.shape[1] + min(0, -s)]
        d = float(np.abs(a - b).mean())
        if d < best[1]:
            best = (s, d)
    return best


def sharpness(rgb, box):
    """Mean absolute horizontal gradient of the picture area: smearing (motion blur) lowers it."""
    x, y, w, h = box
    g = rgb[y:y + h, x:x + w, :].astype(np.float32).mean(axis=2)
    return float(np.abs(np.diff(g, axis=1)).mean())


def continue_into_level(pad, hwnd, proc):
    """On the main menu: press A (CONTINUE) until the picture leaves the menu, checked by screen."""
    time.sleep(MENU_REF_WAIT_S)
    ref = grab(hwnd).astype(np.int16)
    for attempt in range(1, CONTINUE_TRIES + 1):
        if proc.poll() is not None:
            return False
        press(pad, hwnd, "A")
        time.sleep(CONTINUE_CHECK_S)
        now = grab(hwnd).astype(np.int16)
        diff = float(np.abs(now - ref).mean()) if now.shape == ref.shape else 255.0
        print(f"    CONTINUE try {attempt}: picture change {diff:.1f} (left the menu above {MENU_LEFT_DIFF})",
              flush=True)
        if diff > MENU_LEFT_DIFF:
            return True
    return False


def wait_for_camera(launched_at, proc):
    """The opening film and titles play first: wait until the probe logs camera builds."""
    deadline = time.time() + LEVEL_WAIT_S
    while time.time() < deadline:
        if proc.poll() is not None:
            return False
        logs = [q for q in glob.glob(os.path.join(LOG_DIR, "*.log")) if os.path.getmtime(q) >= launched_at - 5]
        if logs:
            with open(max(logs, key=os.path.getmtime), encoding="utf-8", errors="replace") as f:
                text = f.read()
            built = [int(n) for n in CAMERA_1_RE.findall(text)] + [int(n) for n in CAM_IN_PACKET_RE.findall(text)]
            if any(n > 0 for n in built):
                print(f"    camera builds seen in the log at t={time.time() - launched_at:.0f} s", flush=True)
                return True
        time.sleep(LOG_POLL_S)
    return False


def press(pad, hwnd, label):
    user32.SetForegroundWindow(hwnd); time.sleep(FOCUS_SETTLE_S)
    pad.press_button(BUTTONS[label]); pad.update(); time.sleep(PRESS_HOLD_S)
    pad.release_button(BUTTONS[label]); pad.update()
    print(f"    press {label}", flush=True)


def obs(*args):
    try:
        r = subprocess.run([sys.executable, OBS_REC, *args], capture_output=True, text=True, timeout=60)
        print(f"  obs-rec {args[0]}: {(r.stdout or r.stderr).strip()[-300:]}", flush=True)
    except Exception as e:                              # a recording fault is reported, never fatal
        print(f"  WARNING obs-rec {args[0]} failed: {e}", flush=True)


def close_game(proc, hwnd):
    if hwnd:
        user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    try:
        proc.wait(timeout=CLOSE_WAIT_S)
        print("  closed with WM_CLOSE", flush=True)
    except subprocess.TimeoutExpired:
        print("  WM_CLOSE not honoured in time; terminating", flush=True)
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()


def log_lines(launched_at):
    logs = [p for p in glob.glob(os.path.join(LOG_DIR, "*.log")) if os.path.getmtime(p) >= launched_at - 5]
    if not logs:
        return None, []
    path = max(logs, key=os.path.getmtime)
    with open(path, encoding="utf-8", errors="replace") as f:
        hits = [ln.rstrip() for ln in f if any(k in ln for k in LOG_KEYS)]
    # once-only lines in full, then the last two of each repeating kind
    out, seen = [], {}
    for ln in hits:
        key = next(k for k in LOG_KEYS if k in ln)
        seen.setdefault(key, []).append(ln)
    for key, lines in seen.items():
        out += lines if len(lines) <= 2 else lines[-2:]
    return path, out


def run(mode, args):
    out = os.path.join(OUT_ROOT, f"{mode}-{time.strftime('%Y%m%d-%H%M%S')}")
    os.makedirs(out, exist_ok=True)
    flat = mode in ("N", "F")                           # screen-effects test: plain flat picture
    env = dict(os.environ) if flat else dict(os.environ, **BASE_ENV)
    for k in ("DK_EYE_BY_PACKET", "DK_SBS_HOLD", "DK_VR_SCREEN_FX_OFF") + (tuple(BASE_ENV) if flat else ()):
        env.pop(k, None)
    if mode == "F":
        env["DK_VR_SCREEN_FX_OFF"] = "1"
    if mode in ("B", "H", "O"):
        env["DK_EYE_BY_PACKET"] = "1"
    if mode in ("H", "O"):
        env["DK_SBS_HOLD"] = "1"
    if mode == "O":
        env["DK_FREEZE"] = "0"   # freeze-off baseline: frame_start_dt should be about one frame
    print(f"== run {mode}: " + " ".join(f"{k}={v}" for k, v in env.items() if k.startswith("DK_")), flush=True)
    pad = vg.VX360Gamepad(); time.sleep(1.0)            # pad exists before the game starts
    launched_at = time.time()
    proc = subprocess.Popen([EXE], cwd=GAME_DIR, env=env)
    hwnd = None
    while hwnd is None and time.time() - launched_at < WINDOW_WAIT_S:
        time.sleep(1.0); hwnd = find_window(proc.pid)
    if hwnd is None:
        print("  no window"); proc.terminate(); return None
    if not args.no_obs:
        obs("start", "darknessrecomp.exe", f"eye-by-packet-{mode}", "--game", "the-darkness-vr")
    try:
        t_title = None
        while t_title is None and time.time() - launched_at < TITLE_LATEST_S:
            time.sleep(2.0)
            if proc.poll() is not None:
                print("  EXITED", proc.returncode); return None
            t = time.time() - launched_at
            if t > TITLE_EARLIEST_S and len(np.unique(grab(hwnd).reshape(-1, 3), axis=0)) > TITLE_COLOURS:
                t_title = t
        print(f"  title at t={t_title}", flush=True)
        if t_title is None:
            return None
        for label in ["START", "A", "START"]:
            time.sleep(STEP_S); press(pad, hwnd, label)
        if args.scene == "car":
            if not continue_into_level(pad, hwnd, proc):
                print("  CONTINUE never left the menu"); return None
            if not wait_for_camera(launched_at, proc):
                print(f"  no camera builds in the log within {LEVEL_WAIT_S:.0f} s"); return None
            time.sleep(LEVEL_SETTLE_S)
        else:
            time.sleep(MENU_SETTLE_S)

        box = client_box(hwnd)
        shifts, diffs, sharp_turn, sharp_still = [], [], [], []
        for i in range(args.shots):
            turning = args.turn and (i // TURN_BURST_SHOTS) % 2 == 0
            if args.turn:
                pad.right_joystick_float(x_value_float=TURN_DEFLECT if turning else 0.0, y_value_float=0.0)
                pad.update()
            pic = grab(hwnd)
            (sharp_turn if turning else sharp_still).append(sharpness(pic, box))
            s, _ = best_shift(pic, box)
            if s is not None:
                shifts.append(s)
            d = halves_difference(pic, box)
            if d is not None:
                diffs.append(d)
            if i < KEEP_PICTURES:
                Image.fromarray(pic).save(os.path.join(out, f"{mode}_{i:02d}.png"))
    finally:
        if args.turn:
            pad.right_joystick_float(x_value_float=0.0, y_value_float=0.0); pad.update()
        if not args.no_obs:
            obs("stop")
        close_game(proc, hwnd)
        if not args.no_obs:
            obs("done")

    n = max(1, len(shifts))
    res = {"mode": mode, "scene": args.scene, "pictures": len(shifts),
           "one_eye_share": round(sum(abs(s) <= SAME_EYE_SHIFT_PX for s in shifts) / n, 3),
           "positive_shift_share": round(sum(s > SAME_EYE_SHIFT_PX for s in shifts) / n, 3),
           "negative_shift_share": round(sum(s < -SAME_EYE_SHIFT_PX for s in shifts) / n, 3),
           "median_abs_shift_px": float(np.median(np.abs(shifts))) if shifts else None,
           "identical_halves_share": round(sum(d < SAME_BELOW for d in diffs) / max(1, len(diffs)), 3),
           "sharpness_turning": round(float(np.median(sharp_turn)), 3) if sharp_turn else None,
           "sharpness_still": round(float(np.median(sharp_still)), 3) if sharp_still else None}
    with open(os.path.join(out, f"{mode}_result.json"), "w") as f:
        json.dump({"result": res, "shifts": shifts, "differences": diffs}, f)
    log_path, lines = log_lines(launched_at)
    print(json.dumps(res), flush=True)
    print(f"  log: {log_path}")
    for ln in lines:
        print("   ", ln[:400])
    print(f"  pictures + result: {out}", flush=True)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["A", "B", "H", "O", "AB", "ABH", "BHO", "N", "F", "NF"])
    ap.add_argument("--shots", type=int, default=200)
    ap.add_argument("--scene", choices=["menu", "car"], default="menu")
    ap.add_argument("--no-obs", action="store_true")
    ap.add_argument("--turn", action="store_true", help="hold the right stick in bursts while taking pictures")
    args = ap.parse_args()
    results = [run(m, args) for m in (list(args.mode) if len(args.mode) > 1 else [args.mode])]
    print("\n== summary")
    for r in results:
        print("  ", json.dumps(r))
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
