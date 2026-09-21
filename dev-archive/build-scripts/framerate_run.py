"""Frame-rate run: launch a ReXGlue game, press through its menus, and measure as it goes.

Written 2026-09-21 on the home PC for the two owed/HOME items (The Darkness, Condemned 2).
Both games use the same SDK and the same window, so one script serves both.

It presses a given sequence of pad buttons, saving a screenshot after each, then sits
still and keeps taking screenshots. Every screenshot carries the wall-clock second, so the
game's own log can be cut to exactly the seconds a given picture was on screen.

THE FRAME RATE COMES FROM THE GAME LOG, not from this script: count_frames.py reads the
"[VP] PERSP" lines our build writes. Watching the window from outside was tried first
(claude-memory/tools/measure-frame-rate.py) and sees a frozen picture on this kind of
window [measured 2026-09-21]; only PrintWindow sees the real one, and it is too slow to count with.

  python framerate_run.py --exe C:\\NonSteam\\the-darkness\\build-local\\darknessrecomp.exe
                          --out C:\\NonSteam\\the-darkness\\screens\\framerate

Screenshots are game content: --out must stay OUTSIDE every git repo.
"""
import argparse, ctypes, ctypes.wintypes as wt, importlib.util, json, os, subprocess, sys, time
import numpy as np
import vgamepad as vg
from PIL import Image

# ---- settings (named, in one place) --------------------------------------------------
MEASURE_TOOL = r"C:\Users\TD3KX\github-backups\claude-memory\tools\measure-frame-rate.py"
WINDOW_WAIT_S = 90.0        # give up if no window by then
STEP_S = 4.0                # between presses; each step ends with a screenshot
HOLD_SHOT_EVERY_S = 10.0    # screenshot spacing while sitting still at the end
PRESS_HOLD_S = 0.12
FOCUS_SETTLE_S = 0.4
VK_RETURN = 0x0D
VK_F3 = 0x72                # ReXGlue's own debug overlay: "Guest: N FPS" (bind_debug_overlay, SDK src/ui/rex_app.cpp)
KEYEVENTF_KEYUP = 0x0002

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
user32.SetProcessDPIAware()
for fn, res, args in [
    (user32.GetWindowDC, ctypes.c_void_p, [ctypes.c_void_p]),
    (user32.ReleaseDC, ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p]),
    (user32.PrintWindow, wt.BOOL, [ctypes.c_void_p, ctypes.c_void_p, wt.UINT]),
    (user32.GetWindowRect, wt.BOOL, [ctypes.c_void_p, ctypes.POINTER(wt.RECT)]),
    (user32.GetClientRect, wt.BOOL, [ctypes.c_void_p, ctypes.POINTER(wt.RECT)]),
    (user32.SetForegroundWindow, wt.BOOL, [ctypes.c_void_p]),
    (user32.GetForegroundWindow, ctypes.c_void_p, []),
    (gdi32.CreateCompatibleDC, ctypes.c_void_p, [ctypes.c_void_p]),
    (gdi32.CreateCompatibleBitmap, ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]),
    (gdi32.SelectObject, ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_void_p]),
    (gdi32.DeleteObject, wt.BOOL, [ctypes.c_void_p]),
    (gdi32.DeleteDC, wt.BOOL, [ctypes.c_void_p]),
    (gdi32.GetDIBits, ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p, wt.UINT, wt.UINT,
                                     ctypes.c_void_p, ctypes.c_void_p, wt.UINT]),
]:
    fn.restype = res; fn.argtypes = args


class BIH(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]


def load_measure_tool():
    spec = importlib.util.spec_from_file_location("measure_frame_rate", MEASURE_TOOL)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def grab(hwnd):
    """Whole-window screenshot via PrintWindow, which works with the window behind others."""
    r = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    w, h = r.right - r.left, r.bottom - r.top
    hdc = user32.GetWindowDC(hwnd)
    mdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(mdc, bmp)
    user32.PrintWindow(hwnd, mdc, 2)
    bi = BIH()
    bi.biSize = ctypes.sizeof(BIH); bi.biWidth = w; bi.biHeight = -h
    bi.biPlanes = 1; bi.biBitCount = 32; bi.biCompression = 0
    buf = (ctypes.c_ubyte * (w * h * 4))()
    gdi32.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    gdi32.DeleteObject(bmp); gdi32.DeleteDC(mdc); user32.ReleaseDC(hwnd, hdc)
    return np.frombuffer(buf, dtype=np.uint8).reshape(h, w, 4)[:, :, 2::-1].copy()


BUTTONS = {
    "START": vg.XUSB_BUTTON.XUSB_GAMEPAD_START,
    "A": vg.XUSB_BUTTON.XUSB_GAMEPAD_A,
    "B": vg.XUSB_BUTTON.XUSB_GAMEPAD_B,
    "DOWN": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_DOWN,
    "UP": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_UP,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--out", required=True, help="screenshot + timeline folder, outside any repo")
    ap.add_argument("--first-press", type=float, default=50.0, help="seconds after launch before the first press")
    ap.add_argument("--seq", default="START DOWN A START A",
                    help="pad presses in order; WAIT is a step with no press")
    ap.add_argument("--step", type=float, default=STEP_S, help="seconds between presses")
    ap.add_argument("--hold", type=float, default=90.0, help="seconds to sit still after the last press")
    ap.add_argument("--keys", action="store_true", help="also send the Enter key with START and A")
    ap.add_argument("--overlay", action="store_true", help="press F3 before the first press to show the SDK's FPS overlay (needs SDK 0.10+)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    mfr = load_measure_tool()
    pad = vg.VX360Gamepad()
    time.sleep(1.0)
    proc = subprocess.Popen([args.exe], cwd=os.path.dirname(args.exe))
    t0 = time.time()
    print("launched pid %d at %s" % (proc.pid, time.strftime("%H:%M:%S")), flush=True)

    hwnd = None
    while hwnd is None:
        if proc.poll() is not None:
            print("PROCESS EXITED before a window appeared, code %s" % proc.returncode); return 3
        if time.time() - t0 > WINDOW_WAIT_S:
            print("no window after %.0f s" % WINDOW_WAIT_S); proc.terminate(); return 2
        time.sleep(1.0)
        hwnd, title = mfr.window_by_pid(proc.pid)
    print('window "%s" at t=%.1f' % (title, time.time() - t0), flush=True)
    cr = wt.RECT(); user32.GetClientRect(hwnd, ctypes.byref(cr))
    print("client area %dx%d" % (cr.right - cr.left, cr.bottom - cr.top), flush=True)

    timeline = []

    def shot(tag):
        t = time.time() - t0
        rgb = grab(hwnd)
        colours = len(np.unique(rgb.reshape(-1, 3), axis=0))
        name = "%s_t%05.1f.png" % (tag, t)
        Image.fromarray(rgb).save(os.path.join(args.out, name))
        timeline.append({"shot": name, "t": round(t, 1), "clock": time.strftime("%H:%M:%S"),
                         "colours": colours, "foreground": user32.GetForegroundWindow() == hwnd})
        print("  %s  clock %s  colours=%d" % (name, time.strftime("%H:%M:%S"), colours), flush=True)

    def press(label):
        # Two routes at once: the virtual pad, and (optionally) the keyboard.
        button = BUTTONS[label]
        keys = args.keys and label in ("START", "A")
        user32.SetForegroundWindow(hwnd)
        time.sleep(FOCUS_SETTLE_S)
        pad.press_button(button); pad.update()
        if keys:
            user32.keybd_event(VK_RETURN, 0, 0, 0)
        time.sleep(PRESS_HOLD_S)
        pad.release_button(button); pad.update()
        if keys:
            user32.keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0)
        print("press %-5s at t=%.1f" % (label, time.time() - t0), flush=True)

    while time.time() - t0 < args.first_press:
        time.sleep(1.0)
    shot("before")
    if args.overlay and proc.poll() is None:
        user32.SetForegroundWindow(hwnd)
        time.sleep(FOCUS_SETTLE_S)
        user32.keybd_event(VK_F3, 0x3D, 0, 0)
        time.sleep(PRESS_HOLD_S)
        user32.keybd_event(VK_F3, 0x3D, KEYEVENTF_KEYUP, 0)
        print("press F3 (overlay) at t=%.1f" % (time.time() - t0), flush=True)
    for i, label in enumerate(args.seq.split()):
        if proc.poll() is not None:
            print("PROCESS EXITED mid-run, code %s" % proc.returncode); break
        if label != "WAIT":
            press(label)
        time.sleep(args.step)
        shot("step%02d_%s" % (i, label))

    t_hold = time.time()
    while time.time() - t_hold < args.hold and proc.poll() is None:
        time.sleep(HOLD_SHOT_EVERY_S)
        shot("hold")

    with open(os.path.join(args.out, "timeline.json"), "w") as f:
        json.dump(timeline, f, indent=1)
    print("closing at %s" % time.strftime("%H:%M:%S"), flush=True)
    proc.terminate()
    try:
        proc.wait(timeout=15)
    except Exception:
        proc.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main())
