"""QUICKMAP test for The Darkness recompilation.

Question: does `EnvironmentXbox.cfg` with `QUICKMAP=NY1_Tunnel`, placed BESIDE THE EXE,
drop the build straight into the first 3D level and skip the ~90 s logo/menu/trailer chain?

Baseline to beat (2026-09-16, hands-off, no cfg): logos to ~45 s, black at ~50 s,
title screen ~70 s, attract trailer ~94 s. 3D only ever reached by driving the menus.

Also carries the focus guard the board asked for: at every sample it records WHICH
process owns the foreground window, so "the game took focus" can never again be
confused with "Tefa switched windows".
"""
import ctypes, ctypes.wintypes as wt, json, os, subprocess, sys, time
import numpy as np
from PIL import Image

GAME_DIR = r"E:\the-darkness\build-local"
EXE = os.path.join(GAME_DIR, "darknessrecomp.exe")
CFG = os.environ.get("DK_CFG", os.path.join(GAME_DIR, "Assets", "EnvironmentXbox.cfg"))
OUT = r"E:\the-darkness\screens\quickmap"
DURATION = float(sys.argv[1]) if len(sys.argv) > 1 else 100.0
INTERVAL = 4.0
os.makedirs(OUT, exist_ok=True)

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
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
    user32.PrintWindow(hwnd, mdc, 2)  # PW_RENDERFULLCONTENT - captures an occluded D3D window
    bi = BIH()
    bi.biSize = ctypes.sizeof(BIH); bi.biWidth = w; bi.biHeight = -h
    bi.biPlanes = 1; bi.biBitCount = 32; bi.biCompression = 0
    buf = (ctypes.c_ubyte * (w * h * 4))()
    gdi32.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    gdi32.DeleteObject(bmp); gdi32.DeleteDC(mdc); user32.ReleaseDC(hwnd, hdc)
    return np.frombuffer(buf, dtype=np.uint8).reshape(h, w, 4)[:, :, 2::-1].copy()


def foreground_process():
    """Which process owns the foreground window right now - the focus guard."""
    h = user32.GetForegroundWindow()
    if not h:
        return "<none>"
    pid = wt.DWORD()
    user32.GetWindowThreadProcessId(h, ctypes.byref(pid))
    hproc = kernel32.OpenProcess(0x1000, False, pid.value)  # QUERY_LIMITED_INFORMATION
    if not hproc:
        return "<pid %d>" % pid.value
    buf = ctypes.create_unicode_buffer(512)
    size = wt.DWORD(512)
    ok = kernel32.QueryFullProcessImageNameW(hproc, 0, buf, ctypes.byref(size))
    kernel32.CloseHandle(hproc)
    return os.path.basename(buf.value) if ok else "<pid %d>" % pid.value


def proc_stats(pid):
    try:
        out = subprocess.run(
            ["powershell", "-NoProfile", "-Command",
             "$p=Get-Process -Id %d -ErrorAction Stop; "
             "'{0},{1}' -f [int]($p.WorkingSet64/1MB), $p.Threads.Count" % pid],
            capture_output=True, text=True, timeout=10)
        mb, th = out.stdout.strip().split(",")
        return int(mb), int(th)
    except Exception:
        return -1, -1


def main():
    if not os.path.exists(CFG):
        print("FATAL: %s missing - write it before running" % CFG)
        return 2
    print("cfg beside exe:", open(CFG).read().strip())

    before_focus = foreground_process()
    print("foreground before launch:", before_focus)

    proc = subprocess.Popen([EXE], cwd=GAME_DIR)
    print("launched pid", proc.pid)
    hwnd = None
    t0 = time.time()
    samples = []
    while time.time() - t0 < DURATION:
        time.sleep(INTERVAL)
        t = round(time.time() - t0, 1)
        if proc.poll() is not None:
            print("PROCESS EXITED at t=%.1f rc=%s" % (t, proc.returncode))
            break
        if hwnd is None:
            hwnd = find_window(proc.pid)
            if hwnd is None:
                print("t=%5.1f  no window yet" % t)
                continue
            print("window found at t=%.1f" % t)
        rgb = grab(hwnd)
        colours = len(np.unique(rgb.reshape(-1, 3), axis=0))
        mb, th = proc_stats(proc.pid)
        fg = foreground_process()
        path = os.path.join(OUT, "t%05.1f.png" % t)
        Image.fromarray(rgb).save(path)
        samples.append({"t": t, "colours": int(colours), "mem_mb": mb,
                        "threads": th, "foreground": fg})
        print("t=%5.1f  colours=%6d  mem=%5d MB  threads=%3d  focus=%s"
              % (t, colours, mb, th, fg))

    print("closing")
    proc.terminate()
    try:
        proc.wait(timeout=15)
    except Exception:
        proc.kill()
    with open(os.path.join(OUT, "samples.json"), "w") as f:
        json.dump({"before_focus": before_focus, "samples": samples}, f, indent=1)
    print("after focus:", foreground_process())
    return 0


if __name__ == "__main__":
    sys.exit(main())
