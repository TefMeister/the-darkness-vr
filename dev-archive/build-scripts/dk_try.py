"""dk_try.py -- one unattended frame-rate try of a TEST copy of the VR build on the pretend headset (2026-11-10).

Sets the given settings in the test folder's darknessrecomp.toml (keys given as key=value; "key=" removes the key),
switches VR Bench on for that exe, drives to the alley (dk_sim_drive.py), measures the game's frames a second from the
game's own log over the idle time and the graphics card's load, closes the game by its window, and appends one line
to the results table.

  python dk_try.py <test build folder> <label> [key=value ...] [--idle 45] [--env NAME=VALUE ...]

Refuses a folder whose name has no "test" in it, so the installed build's settings are never touched.
"""
import argparse, ctypes, ctypes.wintypes as wt, datetime as dt, glob, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
VRBENCH = os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/vrbench/vrbench.py")
RESULTS = "D:/the-darkness-build/gputrace/tries.tsv"
SETTLE_S = 12.0          # after the last press, before counting
IDLE_DEFAULT_S = 45.0    # how long to sit in the level and measure
CLOSE_POLLS = 20         # half-second looks for the game to be gone after the close message
WM_CLOSE = 0x0010
FRAME_LINE = re.compile(r"^\[(.{23})\].*\[VP\] FREEZE frames=(\d+)")
PAIR_LINE = re.compile(r"^\[(.{23})\].*\[XR\] frames=(\d+)")


def set_toml(path, pairs):
    lines = open(path, encoding="utf-8").read().splitlines()
    for key, value in pairs:
        lines = [l for l in lines if not re.match(r"\s*%s\s*=" % re.escape(key), l)]
        if value != "":
            lines.append("%s = %s" % (key, value))
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")


def line_reading(line, pattern, since):
    """(time, counter) from one log line, or None."""
    m = pattern.search(line)
    if not m:
        return None
    t = dt.datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S.%f")
    return (t, int(m.group(2))) if t >= since else None


def readings(folder, pattern, since):
    out = []
    logs = [p for p in glob.glob(os.path.join(folder, "logs", "darknessrecomp_*.log"))
            if os.path.getmtime(p) >= since.timestamp()]
    for p in logs:
        with open(p, encoding="utf-8", errors="replace") as f:
            out.extend(r for r in (line_reading(line, pattern, since) for line in f) if r)
    return sorted(out)


def rate(v):
    if len(v) < 2 or (v[-1][0] - v[0][0]).total_seconds() < 5:
        return None
    return (v[-1][1] - v[0][1]) / (v[-1][0] - v[0][0]).total_seconds()


def card_load(seconds):
    vals = []
    end = time.time() + seconds
    while time.time() < end:
        r = subprocess.run(["nvidia-smi", "--query-gpu=utilization.gpu", "--format=csv,noheader,nounits"],
                           capture_output=True, text=True)
        try:
            vals.append(int(r.stdout.strip().splitlines()[0]))
        except (ValueError, IndexError):
            pass
        time.sleep(1.0)
    return sum(vals) / len(vals) if vals else None


def close_game():
    user32 = ctypes.windll.user32
    found = []

    def each(hwnd, _):
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq darknessrecomp.exe", "/FO", "CSV", "/NH"],
                       capture_output=True, text=True)
    pids = {int(l.split('","')[1]) for l in r.stdout.splitlines() if l.startswith('"darknessrecomp')}
    user32.EnumWindows(ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(each), 0)
    for h in found:
        user32.PostMessageW(h, WM_CLOSE, 0, 0)
    for _ in range(CLOSE_POLLS):
        time.sleep(0.5)
        r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq darknessrecomp.exe", "/NH"], capture_output=True, text=True)
        if "darknessrecomp" not in r.stdout:
            return True
    return False


def take_shot(path):
    sys.path.insert(0, HERE)
    import winshot
    from PIL import Image
    r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq darknessrecomp.exe", "/FO", "CSV", "/NH"],
                       capture_output=True, text=True)
    pid = int(r.stdout.splitlines()[0].split('","')[1])
    hwnd = winshot.find_window(pid)
    Image.fromarray(winshot.grab(hwnd)).save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("label")
    ap.add_argument("settings", nargs="*")
    ap.add_argument("--idle", type=float, default=IDLE_DEFAULT_S)
    ap.add_argument("--env", action="append", default=[])
    ap.add_argument("--shot", default="", help="save a picture of the game's window here before closing (outside every repo)")
    a = ap.parse_args()
    if "test" not in os.path.basename(os.path.normpath(a.folder)).lower():
        sys.exit("refused: the folder name has no 'test' in it")
    pairs = [s.split("=", 1) for s in a.settings]
    set_toml(os.path.join(a.folder, "darknessrecomp.toml"), pairs)
    env = dict(os.environ)
    for e in a.env:
        k, v = e.split("=", 1)
        env[k] = v
    exe = os.path.join(a.folder, "darknessrecomp.exe")
    subprocess.run([sys.executable, VRBENCH, "start", exe], capture_output=True, text=True)
    drive = subprocess.Popen([sys.executable, os.path.join(HERE, "dk_sim_drive.py"), a.folder, str(a.idle)],
                             stdout=subprocess.PIPE, text=True, env=env)
    idle_from = None
    for line in drive.stdout:
        if "now idling" in line:
            idle_from = dt.datetime.now() + dt.timedelta(seconds=SETTLE_S)
            break
    if idle_from is None:
        sys.exit("the drive script ended before the level")
    time.sleep(SETTLE_S)
    load = card_load(max(5.0, a.idle - SETTLE_S - 3.0))
    drive.wait()
    frames = rate(readings(a.folder, FRAME_LINE, idle_from))
    pairs_s = rate(readings(a.folder, PAIR_LINE, idle_from))
    if a.shot:
        take_shot(a.shot)
    closed = close_game()
    fmt = lambda x: "none" if x is None else "%.1f" % x
    line = "\t".join([dt.datetime.now().strftime("%Y-%m-%d %H:%M"), a.label, " ".join(a.settings + a.env) or "-",
                      fmt(frames), fmt(pairs_s), fmt(load), "closed" if closed else "STILL RUNNING"])
    os.makedirs(os.path.dirname(RESULTS), exist_ok=True)
    with open(RESULTS, "a", encoding="utf-8") as f:
        f.write(line + "\n")
    print("when\tlabel\tsettings\tgame frames/s\teye pairs/s\tcard %\tgame")
    print(line)


if __name__ == "__main__":
    main()
