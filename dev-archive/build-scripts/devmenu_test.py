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

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
user32.SetProcessDPIAware()
for fn, res, args in [
    (user32.GetWindowDC, ctypes.c_void_p, [ctypes.c_void_p]),
    (user32.ReleaseDC, ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p]),
    (user32.PrintWindow, wt.BOOL, [ctypes.c_void_p, ctypes.c_void_p, wt.UINT]),
    (user32.GetWindowRect, wt.BOOL, [ctypes.c_void_p, ctypes.POINTER(wt.RECT)]),
    (user32.SetForegroundWindow, wt.BOOL, [ctypes.c_void_p]),
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
        p = wt.DWORD()
        user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(h) and user32.GetWindowTextLengthW(h) > 0:
            found.append(h)
        return True
    user32.EnumWindows(PROC(cb), 0)
    return found[0] if found else None


def grab(hwnd):
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


def send_key(vk, scan):
    """SendInput with a scancode - the route that works on this class of game."""
    class KI(ctypes.Structure):
        _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD),
                    ("time", wt.DWORD), ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong))]

    class II(ctypes.Union):
        _fields_ = [("ki", KI)]

    class INP(ctypes.Structure):
        _fields_ = [("type", wt.DWORD), ("ii", II)]
    for flags in (0x0008, 0x0008 | 0x0002):  # SCANCODE down, SCANCODE|KEYUP
        inp = INP(1, II(KI(0, scan, flags, 0, None)))
        user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INP))
        time.sleep(0.05)


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
