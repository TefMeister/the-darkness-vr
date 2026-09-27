"""winshot.py - the window helpers every test script here shares: find the game's window, screenshot
it (even behind other windows), and send a key by scancode.

Until 2026-09-27 each script carried its own copy of this block (devmenu_test, framerate_run,
looktest, quickmap_test, stereo_probe_run), and the copies had already started to drift. Moved here
unchanged; the only difference is that the Win32 signatures below are the union of what the five
copies declared, which only adds type information to calls the scripts already make.
"""
import ctypes
import ctypes.wintypes as wt
import time

import numpy as np

KEY_GAP_S = 0.05   # seconds between a key's down and up events; shorter and some games miss the press

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
user32.SetProcessDPIAware()
for fn, res, args in [
    (user32.GetWindowDC, ctypes.c_void_p, [ctypes.c_void_p]),
    (user32.ReleaseDC, ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p]),
    (user32.PrintWindow, wt.BOOL, [ctypes.c_void_p, ctypes.c_void_p, wt.UINT]),
    (user32.GetWindowRect, wt.BOOL, [ctypes.c_void_p, ctypes.POINTER(wt.RECT)]),
    (user32.GetClientRect, wt.BOOL, [ctypes.c_void_p, ctypes.POINTER(wt.RECT)]),
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
    """Whole-window screenshot via PrintWindow, which works with the window behind others."""
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
        time.sleep(KEY_GAP_S)
