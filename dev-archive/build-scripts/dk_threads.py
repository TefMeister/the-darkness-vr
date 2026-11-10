"""dk_threads.py -- what are the port's busiest threads doing? (2026-11-10)

Finds the threads of darknessrecomp.exe that use the most processor over a few seconds, then samples each one's
instruction pointer (the thread is held for well under a millisecond per sample) and says which module it was in:
the game's own recompiled code and the port (darknessrecomp.exe), the graphics layer (rexgpu-xenos.dll), the runtime
(rexruntime.dll), Direct3D, the graphics driver, or Windows (ntdll: waiting or spinning on a lock).

    python dk_threads.py [--top 3] [--samples 400] [--map darknessrecomp.map] [--names 12]

With --map (the linker's map of the SAME build: CMAKE_EXE_LINKER_FLAGS=-Xlinker /MAP:darknessrecomp.map) the samples
inside darknessrecomp.exe are also counted per function (sub_XXXXXXXX = the game's own function at that address).
"""
import argparse
import bisect
import collections
import ctypes
import ctypes.wintypes as wt
import re
import subprocess
import time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)
THREAD_ALL = 0x1FFFFF
PROCESS_QUERY_READ = 0x0410
CONTEXT_CONTROL = 0x100001
PREFERRED_BASE = 0x140000000
BUSY_WINDOW_S = 4.0        # how long the threads' processor time is watched to find the busiest
SAMPLE_GAP_S = 0.003       # between two samples of one thread
MAX_MODULES = 1024         # room for the process's module list
NAME_CHARS = 260           # a module's base name
MODULES_SHOWN = 6          # modules listed per thread
CONTEXT_REST_BYTES = 976   # the part of the thread context after Rip that is not read
FILETIME_PER_MS = 10000.0  # Windows counts thread time in 100 ns steps
SUSPEND_FAILED = 0xFFFFFFFF
MS_PER_S = 1000.0
PERCENT = 100
DEFAULT_SAMPLES = 400      # samples taken per thread unless --samples says otherwise
FUNC_NAME_CHARS = 90       # a function name is cut to this in the report
MAP_LINE = re.compile(r"^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s")


class CONTEXT(ctypes.Structure):
    _pack_ = 16
    _fields_ = [("pad0", ctypes.c_ulonglong * 6), ("ContextFlags", wt.DWORD), ("MxCsr", wt.DWORD),
                ("seg", wt.WORD * 6), ("EFlags", wt.DWORD), ("dr", ctypes.c_ulonglong * 6),
                ("Rax", ctypes.c_ulonglong), ("Rcx", ctypes.c_ulonglong), ("Rdx", ctypes.c_ulonglong),
                ("Rbx", ctypes.c_ulonglong), ("Rsp", ctypes.c_ulonglong), ("Rbp", ctypes.c_ulonglong),
                ("Rsi", ctypes.c_ulonglong), ("Rdi", ctypes.c_ulonglong), ("R8_15", ctypes.c_ulonglong * 8),
                ("Rip", ctypes.c_ulonglong), ("rest", ctypes.c_byte * CONTEXT_REST_BYTES)]


class MODULEINFO(ctypes.Structure):
    _fields_ = [("lpBaseOfDll", ctypes.c_void_p), ("SizeOfImage", wt.DWORD), ("EntryPoint", ctypes.c_void_p)]


k32.OpenThread.restype = wt.HANDLE
k32.OpenProcess.restype = wt.HANDLE
k32.GetThreadTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(wt.FILETIME)] * 4
k32.GetThreadContext.argtypes = [wt.HANDLE, ctypes.POINTER(CONTEXT)]
psapi.EnumProcessModules.argtypes = [wt.HANDLE, ctypes.POINTER(wt.HMODULE), wt.DWORD, ctypes.POINTER(wt.DWORD)]
psapi.GetModuleInformation.argtypes = [wt.HANDLE, wt.HMODULE, ctypes.POINTER(MODULEINFO), wt.DWORD]
psapi.GetModuleBaseNameW.argtypes = [wt.HANDLE, wt.HMODULE, wt.LPWSTR, wt.DWORD]


def pid_and_threads():
    out = subprocess.run(["powershell", "-NoProfile", "-Command",
                          "$p=Get-Process darknessrecomp; $p.Id; $p.Threads | ForEach-Object { $_.Id }"],
                         capture_output=True, text=True).stdout.split()
    return int(out[0]), [int(x) for x in out[1:]]


def cpu_ms(h):
    c, e, kt, ut = wt.FILETIME(), wt.FILETIME(), wt.FILETIME(), wt.FILETIME()
    k32.GetThreadTimes(h, ctypes.byref(c), ctypes.byref(e), ctypes.byref(kt), ctypes.byref(ut))
    return sum(((t.dwHighDateTime << 32) | t.dwLowDateTime) / FILETIME_PER_MS for t in (kt, ut))


def modules(pid):
    hp = k32.OpenProcess(PROCESS_QUERY_READ, False, pid)
    arr = (wt.HMODULE * MAX_MODULES)()
    need = wt.DWORD()
    psapi.EnumProcessModules(hp, arr, ctypes.sizeof(arr), ctypes.byref(need))
    mods = []
    for i in range(need.value // ctypes.sizeof(wt.HMODULE)):
        mi = MODULEINFO()
        name = ctypes.create_unicode_buffer(NAME_CHARS)
        psapi.GetModuleInformation(hp, arr[i], ctypes.byref(mi), ctypes.sizeof(mi))
        psapi.GetModuleBaseNameW(hp, arr[i], name, NAME_CHARS)
        mods.append((mi.lpBaseOfDll or 0, (mi.lpBaseOfDll or 0) + mi.SizeOfImage, name.value))
    k32.CloseHandle(hp)
    return mods


def load_map(path):
    pairs = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = MAP_LINE.match(line)
            if m:
                pairs.append((int(m.group(2), 16), m.group(1)))
    pairs.sort()
    return [a for a, _ in pairs], [n for _, n in pairs]


def rip_of(h, ctx):
    """the thread's instruction pointer, the thread held only for the read; None if it could not be read"""
    ctx.ContextFlags = CONTEXT_CONTROL
    if k32.SuspendThread(h) == SUSPEND_FAILED:
        return None
    ok = k32.GetThreadContext(h, ctypes.byref(ctx))
    k32.ResumeThread(h)
    return ctx.Rip if ok else None


def sample(h, count, mods, exe, addrs, names):
    """(samples per module, samples per game function) for one thread"""
    ctx = CONTEXT()
    seen = collections.Counter()
    funcs = collections.Counter()
    for _ in range(count):
        rip = rip_of(h, ctx)
        time.sleep(SAMPLE_GAP_S)
        if rip is None:
            continue
        seen[next((n for lo, hi, n in mods if lo <= rip < hi), "(no module)")] += 1
        if addrs and exe[0] <= rip < exe[1]:
            i = bisect.bisect_right(addrs, rip - exe[0] + PREFERRED_BASE) - 1
            funcs[names[i] if i >= 0 else "?"] += 1
    return seen, funcs


def main():
    ap = argparse.ArgumentParser(description=__doc__.split(chr(10) * 2)[0])
    ap.add_argument("--top", type=int, default=3)
    ap.add_argument("--samples", type=int, default=DEFAULT_SAMPLES)
    ap.add_argument("--map", default=None)
    ap.add_argument("--names", type=int, default=12)
    a = ap.parse_args()
    pid, tids = pid_and_threads()
    hs = {t: k32.OpenThread(THREAD_ALL, False, t) for t in tids}
    hs = {t: h for t, h in hs.items() if h}
    before = {t: cpu_ms(h) for t, h in hs.items()}
    time.sleep(BUSY_WINDOW_S)
    percent_of_a_core = BUSY_WINDOW_S * MS_PER_S / PERCENT
    busy = sorted(((cpu_ms(h) - before[t]) / percent_of_a_core, t) for t, h in hs.items())[::-1][:a.top]
    mods = modules(pid)
    exe = next(((lo, hi) for lo, hi, n in mods if n.lower() == "darknessrecomp.exe"), (0, 0))
    addrs, names = load_map(a.map) if a.map else ([], [])
    for share, t in busy:
        seen, funcs = sample(hs[t], a.samples, mods, exe, addrs, names)
        total = sum(seen.values()) or 1
        parts = ", ".join(f"{n} {c * PERCENT // total} %" for n, c in seen.most_common(MODULES_SHOWN))
        print(f"thread {t}: {share:.0f} % of a core | {parts}")
        for n, c in funcs.most_common(a.names):
            print(f"      {c * PERCENT / total:5.1f} %  {n[:FUNC_NAME_CHARS]}")
    for h in hs.values():
        k32.CloseHandle(h)


if __name__ == "__main__":
    main()
