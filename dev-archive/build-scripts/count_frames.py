"""Count frames a second from a ReXGlue game's own log.

  python count_frames.py <logs folder> [--from HH:MM:SS] [--to HH:MM:SS]

Our build logs one "[VP] PERSP" line each time the projection matrix changes, and in a 3D
scene the game applies two different ones per frame in strict alternation (the world one
and a second, narrower one) -- see ENGINE-DOSSIER section 6. So the number of times ONE
given matrix appears in a second is the number of frames drawn in that second.

Rather than trust "two per frame", this counts each distinct matrix separately and looks
for two different ones that appear at the same rate; that shared rate is the frame rate.
(A third one at double the rate is applied twice a frame.) If no two agree, it says so
instead of printing a wrong number.

The log rotates (name.log is newest, name.1.log older, ...), so the whole folder is read
and sorted by timestamp.
"""
import argparse, collections, glob, os, re, statistics, sys

LINE = re.compile(r"^\[\d{4}-\d\d-\d\d (\d\d:\d\d:\d\d)\.\d{3}\] \[info\] \[gpu\] \[t\d+\] \[VP\] PERSP call=\d+ \S+ \|\s*(\S+)")
AGREE_TOLERANCE = 0.05      # the two busiest matrices must agree within 5 % to call it a frame count


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--from", dest="t_from", default="00:00:00")
    ap.add_argument("--to", dest="t_to", default="23:59:59")
    args = ap.parse_args()

    per_second = collections.defaultdict(collections.Counter)   # second -> first matrix cell -> count
    for path in glob.glob(os.path.join(args.logs, "*.log")):
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                if "[VP] PERSP" not in line:
                    continue
                m = LINE.match(line)
                if m and args.t_from <= m.group(1) <= args.t_to:
                    per_second[m.group(1)][m.group(2)] += 1

    seconds = sorted(per_second)
    if len(seconds) < 3:
        print("fewer than 3 seconds of PERSP lines in that window"); return 2
    seconds = seconds[1:-1]          # the first and last second are partial
    totals = collections.Counter()
    for s in seconds:
        totals.update(per_second[s])
    print("window %s .. %s, %d whole seconds" % (seconds[0], seconds[-1], len(seconds)))
    print("distinct projection matrices (by first cell), lines a second:")
    for key, n in totals.most_common(6):
        print("   %-10s %8.1f" % (key, n / len(seconds)))

    # The frame rate is the rate that two DIFFERENT matrices share: that is the per-frame
    # pair. A matrix at twice that rate is applied twice a frame, which is normal too.
    ranked = totals.most_common(4)
    pair = None
    for i in range(len(ranked)):
        for j in range(i + 1, len(ranked)):
            a, b = ranked[i][1], ranked[j][1]
            if abs(a - b) / max(a, b) <= AGREE_TOLERANCE:
                pair = (ranked[i][0], ranked[j][0])
                break
        if pair:
            break
    if not pair:
        print("** no two matrices share a rate, so there is no frame count here (a film or a load?)")
        return 1
    fps = [per_second[s][pair[0]] for s in seconds]
    print()
    print("FRAMES A SECOND: mean %.1f  median %.1f  lowest second %d  highest %d"
          % (statistics.mean(fps), statistics.median(fps), min(fps), max(fps)))
    print("(matrices %s and %s both appear this often, so this is one per frame)" % pair)
    return 0


if __name__ == "__main__":
    sys.exit(main())
