"""dk_fps.py -- The Darkness VR's frame rate, read from the running game's own log (2026-11-07, Tefa: "can you track the game's fps?").

The port logs three counters every few seconds (darknessrecomp_NNN.log in the build folder's logs/):
  [XR] frames=N ... wait_ms_mean=W   pictures handed to the headset (one per eye pair) and the mean wait in xrWaitFrame
  [VP] FREEZE frames=N               the game's own frames (it draws the eyes on alternate frames)
  [SBS] swaps=N                      swaps of the side-by-side picture
Rates come from the counter's change between two lines over their time apart.

  python dk_fps.py [build folder] [--last N] [--watch SECONDS]
The build folder defaults to C:/NonSteam/the-darkness/build-home-xr-b017. --watch reprints every SECONDS until Ctrl+C.
"""
import argparse
import datetime as dt
import glob
import os
import re
import time

PATTERNS = {
    "headset (eye pairs/s)": re.compile(r"^\[(.{23})\].*\[XR\] frames=(\d+).*wait_ms_mean=([\d.]+)"),
    "game frames/s": re.compile(r"^\[(.{23})\].*\[VP\] FREEZE frames=(\d+)"),
    "SBS swaps/s": re.compile(r"^\[(.{23})\].*\[SBS\] swaps=(\d+)"),
}


def newest_log(folder):
    logs = glob.glob(os.path.join(folder, "logs", "darknessrecomp_*.log"))
    return max(logs, key=os.path.getmtime) if logs else None


def read(path):
    data = {k: [] for k in PATTERNS}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            for k, p in PATTERNS.items():
                m = p.search(line)
                if m:
                    t = dt.datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S.%f")
                    extra = float(m.group(3)) if m.lastindex and m.lastindex >= 3 else None
                    data[k].append((t, int(m.group(2)), extra))
    return data


def report(path, last):
    data = read(path)
    print(f"{os.path.basename(path)}  ({dt.datetime.now():%H:%M:%S})")
    for k, v in data.items():
        rates = [(b[1] - a[1]) / (b[0] - a[0]).total_seconds() for a, b in zip(v, v[1:]) if (b[0] - a[0]).total_seconds() > 0.5]
        if not rates:
            print(f"  {k:24} no readings yet")
            continue
        tail = rates[-last:]
        med = sorted(rates)[len(rates) // 2]
        wait = f", headset wait {v[-1][2]:.2f} ms" if v[-1][2] is not None else ""
        print(f"  {k:24} now {tail[-1]:6.1f}  last {len(tail)}: {min(tail):.0f}-{max(tail):.0f}  whole run median {med:.0f}{wait}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder", nargs="?", default="C:/NonSteam/the-darkness/build-home-xr-b017")
    ap.add_argument("--last", type=int, default=10)
    ap.add_argument("--watch", type=float, default=0.0)
    a = ap.parse_args()
    while True:
        path = newest_log(a.folder)
        if path is None:
            print("no darknessrecomp_*.log in", os.path.join(a.folder, "logs"))
        else:
            report(path, a.last)
        if a.watch <= 0:
            break
        time.sleep(a.watch)


if __name__ == "__main__":
    main()
