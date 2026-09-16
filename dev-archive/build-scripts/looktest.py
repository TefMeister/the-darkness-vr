"""Look-stick test for The Darkness recompilation - does the right stick turn the camera?

One process owns launch, virtual pad, window capture and analysis, so timing is exact.
Trials interleave no-input CONTROLS with RIGHT and LEFT stick nudges. For each trial a
burst of frames is taken just before and just after the action, and phase correlation
measures how far the picture slid horizontally. Input-driven yaw shows up as RIGHT and
LEFT producing OPPOSITE shifts, both clearly larger than the controls; the car's own
motion cannot reverse direction on cue.
"""
import ctypes, ctypes.wintypes as wt, json, os, subprocess, sys, time
import numpy as np
import vgamepad as vg
from PIL import Image

GAME_DIR = r"E:\the-darkness\build-local"
OUT = r"E:\the-darkness\screens\looktest"
os.makedirs(OUT, exist_ok=True)

MENU = [(55, "START"), (60, "A"), (65, "A"), (70, "START"), (75, "A"), (80, "A"), (85, "A"), (90, "A")]
FIRST_TRIAL_AT = 112.0
HOLD = 0.8           # seconds the stick is held (run 2: longer, to test "too small")
DEFLECT = 1.0        # run 2: full deflection - a does-it-respond-at-all test, screenshots only
BURST = 5
TRIALS = [("CONTROL-1", 0.0, 0.0), ("RIGHT", DEFLECT, 0.0), ("CONTROL-2", 0.0, 0.0), ("LEFT", -DEFLECT, 0.0),
          ("CONTROL-3", 0.0, 0.0), ("UP", 0.0, DEFLECT), ("CONTROL-4", 0.0, 0.0)]
GAP = 2.5

# ---------------------------------------------------------------- win32 capture
user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
user32.SetProcessDPIAware()
for fn, res, args in [
    (user32.GetWindowDC, ctypes.c_void_p, [ctypes.c_void_p]),
    (user32.ReleaseDC, ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p]),
    (user32.PrintWindow, wt.BOOL, [ctypes.c_void_p, ctypes.c_void_p, wt.UINT]),
    (user32.GetWindowRect, wt.BOOL, [ctypes.c_void_p, ctypes.POINTER(wt.RECT)]),
    (gdi32.CreateCompatibleDC, ctypes.c_void_p, [ctypes.c_void_p]),
    (gdi32.CreateCompatibleBitmap, ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]),
    (gdi32.SelectObject, ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_void_p]),
    (gdi32.DeleteObject, wt.BOOL, [ctypes.c_void_p]),
    (gdi32.DeleteDC, wt.BOOL, [ctypes.c_void_p]),
    (user32.GetForegroundWindow, ctypes.c_void_p, []),
    (user32.GetWindowThreadProcessId, wt.DWORD, [ctypes.c_void_p, ctypes.POINTER(wt.DWORD)]),
    (gdi32.GetDIBits, ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p, wt.UINT, wt.UINT,
                                     ctypes.c_void_p, ctypes.c_void_p, wt.UINT]),
]:
    fn.restype = res; fn.argtypes = args

class BIH(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]

def find_window(pid):
    found = []
    PROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, _):
        p = wt.DWORD(); user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(h) and user32.GetWindowTextLengthW(h) > 0:
            found.append(h)
        return True
    user32.EnumWindows(PROC(cb), 0)
    return found[0] if found else None

def grab(hwnd):
    r = wt.RECT(); user32.GetWindowRect(hwnd, ctypes.byref(r))
    w, h = r.right - r.left, r.bottom - r.top
    hdc = user32.GetWindowDC(hwnd); mdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h); gdi32.SelectObject(mdc, bmp)
    user32.PrintWindow(hwnd, mdc, 2)  # PW_RENDERFULLCONTENT
    bi = BIH(); bi.biSize = ctypes.sizeof(BIH); bi.biWidth = w; bi.biHeight = -h
    bi.biPlanes = 1; bi.biBitCount = 32; bi.biCompression = 0
    buf = (ctypes.c_ubyte * (w * h * 4))()
    gdi32.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    gdi32.DeleteObject(bmp); gdi32.DeleteDC(mdc); user32.ReleaseDC(hwnd, hdc)
    return np.frombuffer(buf, dtype=np.uint8).reshape(h, w, 4)[:, :, 2::-1].copy()

# ---------------------------------------------------------------- analysis
def prep(rgb, title=31, width=480):
    g = rgb[title:, :, :].astype(np.float32).mean(axis=2)
    h, w = g.shape; step = max(1, w // width)
    g = g[::step, ::step]
    g = g - g.mean()
    win = np.outer(np.hanning(g.shape[0]), np.hanning(g.shape[1]))
    return g * win, step

def shift(a, b):
    """Translation of b relative to a, in original-resolution pixels (dx, dy)."""
    pa, step = prep(a); pb, _ = prep(b)
    R = np.fft.fft2(pa) * np.conj(np.fft.fft2(pb))
    R /= np.abs(R) + 1e-9
    r = np.fft.ifft2(R).real
    y, x = np.unravel_index(np.argmax(r), r.shape)
    h, w = r.shape
    if y > h // 2: y -= h
    if x > w // 2: x -= w
    peak = float(r.max()); sharp = float(peak / (np.abs(r).mean() + 1e-9))
    return int(-x * step), int(-y * step), sharp

def fg_is_game(pid):
    h = user32.GetForegroundWindow(); p = wt.DWORD()
    user32.GetWindowThreadProcessId(h, ctypes.byref(p))
    return p.value == pid

def game_alive(proc):
    return proc.poll() is None

# ---------------------------------------------------------------- run
def main():
    pad = vg.VX360Gamepad(); pad.reset(); pad.update()
    proc = subprocess.Popen([os.path.join(GAME_DIR, "darknessrecomp.exe")], cwd=GAME_DIR)
    t0 = time.time()
    report = {"trials": [], "notes": []}
    def T(): return time.time() - t0
    print(f"launched pid={proc.pid}", flush=True)

    try:
        hwnd = None
        while hwnd is None and T() < 30:
            hwnd = find_window(proc.pid); time.sleep(0.2)
        if hwnd is None:
            print("no game window found"); return 2
        print(f"window found at t={T():.1f}s", flush=True)

        for at, btn in MENU:
            while T() < at:
                if not game_alive(proc): print(f"game exited at t={T():.1f}s"); return 3
                time.sleep(0.05)
            b = vg.XUSB_BUTTON.XUSB_GAMEPAD_START if btn == "START" else vg.XUSB_BUTTON.XUSB_GAMEPAD_A
            pad.press_button(button=b); pad.update(); time.sleep(0.15)
            pad.release_button(button=b); pad.update()
            print(f"t={T():5.1f}s pressed {btn}", flush=True)

        while T() < FIRST_TRIAL_AT:
            if not game_alive(proc): print(f"game exited at t={T():.1f}s"); return 3
            time.sleep(0.1)

        for name, x, y in TRIALS:
            before, bt = [], []
            for _ in range(BURST):
                before.append(grab(hwnd)); bt.append(T())
            a0 = T()
            focus_game = fg_is_game(proc.pid)
            if x != 0.0 or y != 0.0:
                pad.right_joystick_float(x_value_float=x, y_value_float=y); pad.update()
            time.sleep(HOLD)
            pad.right_joystick_float(x_value_float=0.0, y_value_float=0.0); pad.update()
            a1 = T()
            time.sleep(0.05)
            after, at_ = [], []
            for _ in range(BURST):
                after.append(grab(hwnd)); at_.append(T())

            base = [shift(before[i], before[i + 1]) for i in range(BURST - 1)]
            base_dx = [abs(b[0]) for b in base]
            dx, dy, sharp = shift(before[-1], after[0])
            dx_span, _, _ = shift(before[0], after[-1])
            lo_b = before[-1][before[-1].shape[0] // 2:]; lo_a = after[0][after[0].shape[0] // 2:]
            dx_low, dy_low, _ = shift(lo_b, lo_a)
            mad = float(np.abs(before[-1].astype(np.int16) - after[0].astype(np.int16)).mean())
            per_frame_s = (bt[-1] - bt[0]) / (BURST - 1)
            gap_s = at_[0] - bt[-1]
            natural = float(np.median(base_dx)) / max(per_frame_s, 1e-3) * gap_s
            row = {
                "trial": name, "stick_x": x, "stick_y": y, "game_has_focus": focus_game, "action_t": round(a0, 2), "hold_s": round(a1 - a0, 3),
                "frame_interval_s": round(per_frame_s, 3), "before_to_after_gap_s": round(gap_s, 3),
                "baseline_dx_per_frame_median": round(float(np.median(base_dx)), 1),
                "expected_natural_dx_over_gap": round(natural, 1),
                "dx_before_last_to_after_first": dx, "dy": dy, "peak_sharpness": round(sharp, 1),
                "dx_before_first_to_after_last": dx_span,
                "dx_lower_half": dx_low, "dy_lower_half": dy_low, "mean_abs_pixel_change": round(mad, 2),
            }
            report["trials"].append(row)
            print(json.dumps(row), flush=True)
            if name in ("RIGHT", "LEFT", "UP", "CONTROL-2"):
                Image.fromarray(before[-1]).save(os.path.join(OUT, f"{name}-before.png"))
                Image.fromarray(after[0]).save(os.path.join(OUT, f"{name}-after.png"))
            time.sleep(GAP)
            if not game_alive(proc):
                report["notes"].append(f"game exited after {name}"); break

        # verdict - uses the LOWER HALF of the frame. Whole-frame phase correlation is pinned at
        # zero by fixed HUD overlays (the "Use to look around" prompt), which is exactly how run 1
        # was misread. Validated 2026-09-16 on known synthetic shifts of +24/-24/+60/0 px.
        rows = {r["trial"]: r for r in report["trials"]}
        if "RIGHT" in rows and "LEFT" in rows:
            r, l = rows["RIGHT"]["dx_lower_half"], rows["LEFT"]["dx_lower_half"]
            ctrl = [abs(rows[k]["dx_lower_half"]) for k in rows if k.startswith("CONTROL") and k != "CONTROL-4"]
            cmax = max(ctrl) if ctrl else 0
            opposite = (r < 0 < l)            # camera right => picture slides left (negative dx)
            big = min(abs(r), abs(l)) > max(3 * cmax, 8)
            up_ok = ("UP" in rows and abs(rows["UP"]["dy_lower_half"]) > max(3 * cmax, 8))
            report["verdict"] = {
                "right_dx_lower": r, "left_dx_lower": l, "largest_clean_control_dx": cmax,
                "right_turn_slides_picture_left_and_left_turn_right": opposite,
                "both_clearly_above_controls": big, "up_produces_vertical_shift": up_ok,
                "camera_responds_to_look_stick": bool(opposite and big),
            }
            print("VERDICT " + json.dumps(report["verdict"]), flush=True)
    finally:
        try:
            pad.right_joystick_float(x_value_float=0.0, y_value_float=0.0); pad.update()
        except Exception:
            pass
        if game_alive(proc):
            subprocess.run(["taskkill", "/F", "/PID", str(proc.pid)], capture_output=True)
        with open(os.path.join(OUT, "looktest.json"), "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
        print("closed.", flush=True)
    return 0

if __name__ == "__main__":
    sys.exit(main())
