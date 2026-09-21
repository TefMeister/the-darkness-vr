"""Frames a second per game from a PresentMon 2.x CSV, for a wall-clock window.

  python pm_fps.py pm.csv darknessrecomp.exe 22:36:10 22:38:25 3
                                     (local times; last number = hours the CSV runs ahead)

PresentMon must have been run with --date_time, so CPUStartTime is a clock time. Every row is
one frame the game handed to Windows, so rows per second IS the frame rate - measured outside
the game, independently of anything our build logs. Used on 2026-09-21 to cross-check
count_frames.py (which counts from the game's own log) and to measure Condemned 2, whose
builds log nothing per frame.
"""
import collections, csv, shutil, statistics, sys, tempfile

SHIFT_H = 0     # hours the CSV clock runs ahead of local time (5th argument)


def main():
    global SHIFT_H
    path, proc, t_from, t_to = sys.argv[1:5]
    SHIFT_H = int(sys.argv[5]) if len(sys.argv) > 5 else 0
    # PresentMon holds the file open while it runs; read a copy.
    tmp = tempfile.mktemp(suffix=".csv")
    shutil.copyfile(path, tmp)
    per_sec = collections.Counter()
    frame_ms = []
    with open(tmp, newline="", encoding="utf-8-sig", errors="replace") as f:
        for row in csv.DictReader(f):
            if row.get("Application", "").lower() != proc.lower():
                continue
            stamp = row.get("CPUStartDateTime") or row.get("CPUStartTime", "")   # 2.x / 1.x names
            hms = stamp.split(" ")[-1].split("T")[-1].split(".")[0].split(":")
            # 2.6.0 writes "1:35:03" (no padding) and NOT in local time - 3 h ahead here on
            # 2026-09-21. Pad, and shift by --shift hours so windows can be given in local time.
            h = (int(hms[0]) - SHIFT_H) % 24
            clock = "%02d:%02d:%02d" % (h, int(hms[1]), int(hms[2]))
            if t_from <= clock <= t_to:
                per_sec[clock] += 1
                try:
                    frame_ms.append(float(row.get("MsBetweenPresents") or row["FrameTime"]))
                except (KeyError, ValueError):
                    pass
    secs = sorted(per_sec)[1:-1]     # drop the partial first and last second
    if len(secs) < 3:
        print("fewer than 3 whole seconds for %s in that window" % proc); return 2
    fps = [per_sec[s] for s in secs]
    print("%s  %s..%s  %d whole seconds" % (proc, secs[0], secs[-1], len(secs)))
    print("FRAMES A SECOND: mean %.1f  median %.1f  lowest second %d  highest %d"
          % (statistics.mean(fps), statistics.median(fps), min(fps), max(fps)))
    if frame_ms:
        frame_ms.sort()
        print("frame time: median %.2f ms, slowest 1%% %.2f ms"
              % (frame_ms[len(frame_ms) // 2], frame_ms[int(len(frame_ms) * 0.99)]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
