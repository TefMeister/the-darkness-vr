"""dk_place.py -- look at ANOTHER level in a TEST copy of the VR build on the pretend headset (2026-11-11).

Starts the test copy with the developer-menu level jump (DK_DEVMENU=1 and the menu's own jump line with the story
state the map has a load set for, see STATE_BITS), drives the title and the
save notice with VR Bench, presses the game's X on the main menu (the doorway), waits for the level, saves pictures of
the game's window at the given moments, reads the game's frames a second from its log, and closes the game by its own
window (dk_close.py).

  python dk_place.py <test build folder> <map> <rtv|rov> <picture prefix> [--at 15,30] [--press a@20] [--state ny2+layer1]

The picture prefix is a path outside every repo (the pictures are game content). --at: seconds after the level is live.
--press: a button of the right hand at a moment after the level is live (to get past a film), e.g. a@50.
Refuses a folder whose name has no "test" in it.
"""
import argparse, datetime as dt, glob, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dk_try as T                       # set_toml, readings, rate, take_shot, the log line patterns
VB = os.path.dirname(T.VRBENCH)
sys.path.insert(0, VB)
import vb_input as I, vb_settings as S

TITLE_WAIT_S = 56        # KNOWN.md: the title is up by then
START_HOLD_S = 1.2       # the menu button held = START
STEP_S = 4.5             # between menu presses
TAP_HOLD_S = 0.2         # how long a tapped button is held (KNOWN.md: 150 ms registers on every menu)
POLL_S = 1.0             # between looks at the log
DOORWAY_TRIES = 5        # presses of the doorway button before giving up
DOORWAY_BUTTON = "b"     # the Quest's B is the game's X (the b009 map)
LEVEL_WAIT_S = 90        # how long a level may take to load
WIDE_LINE = re.compile(r"^\[(.{23})\].*\[VIEW\] a perspective view built.*(\d+)x\d+ fov.*\(widened\)")
RATE_WINDOW_S = 20.0
STATE_DIGITS = 8         # <map>_00002020_Load.XDF
# The story state (the game key DEFAULTSPAWNFLAGS) as bits, read from the game's flag table: easy, medium, hard,
# commentary are bits 0-3, ny1-ny3 bits 4-6 (ow1 / ow2 are other names for ny1 / ny2), layer1-layer4 bits 12-15.
STATE_BITS = ((0x10, "ny1"), (0x20, "ny2"), (0x40, "ny3"),
              (0x1000, "layer1"), (0x2000, "layer2"), (0x4000, "layer3"), (0x8000, "layer4"))
# Word for word the developer menu's own jump line (CubeWnd.xcr), with the state and the map swapped in.
JUMP_LINE = ("setgamekey(\"DEFAULTSPAWNFLAGS\",\"%s\"); cachecommand(\"cg_blackloading(); campaignmap('%s')\"); "
             "cg_rootmenu(\"begin_loadtransform\")")


def press(name, hold=TAP_HOLD_S, hand=None):
    hand = S.HAND_RIGHT if hand is None else hand
    I.write(I.Input().mode(poses="pass", buttons="set").hand(hand, **{name: True}))
    time.sleep(hold)
    I.write(I.Input().mode(poses="pass", buttons="set").hand(hand, **{name: False}))
    print(time.strftime("%H:%M:%S"), "pressed", name, flush=True)


def state_names(number):
    """'00002020' -> 'ny2+layer2': the load set's number is the story state as bits (the flag table at 0x82A3EF40)."""
    bits = int(number, 16)
    names = [n for bit, n in STATE_BITS if bits & bit]
    return "+".join(names) if names and not bits & ~sum(b for b, _ in STATE_BITS) else None


def states_of(folder, map_name):
    """The story states this map has a packed load set for, lowest number first."""
    found = glob.glob(os.path.join(folder, "Assets", "Content", "Xdf", map_name + "_0000*_Load.XDF"))
    numbers = sorted(os.path.basename(f)[len(map_name) + 1:][:STATE_DIGITS] for f in found)
    return [n for n in (state_names(x) for x in numbers) if n]


def log_has(folder, since, text):
    for p in glob.glob(os.path.join(folder, "logs", "darknessrecomp_*.log")):
        if os.path.getmtime(p) >= since.timestamp():
            with open(p, encoding="utf-8", errors="replace") as f:
                if any(text in line for line in f):
                    return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("map")
    ap.add_argument("path", choices=["rtv", "rov"])
    ap.add_argument("prefix")
    ap.add_argument("--at", default="15,30")
    ap.add_argument("--press", default="")
    ap.add_argument("--state", default="", help="the story state, e.g. ny2+layer1; default: the map's lowest load set")
    a = ap.parse_args()
    if "test" not in os.path.basename(os.path.normpath(a.folder)).lower():
        sys.exit("refused: the folder name has no 'test' in it")
    if "darknessrecomp" in subprocess.run(["tasklist"], capture_output=True, text=True).stdout:
        sys.exit("refused: a copy of the game is already running")
    moments = [("shot", float(s)) for s in a.at.split(",") if s]
    for p in [s for s in a.press.split(",") if s]:
        name, when = p.split("@")
        moments.append((name, float(when)))
    moments.sort(key=lambda m: m[1])

    T.set_toml(os.path.join(a.folder, "darknessrecomp.toml"), [("render_target_path_d3d12", '"%s"' % a.path)])
    have = states_of(a.folder, a.map)
    state = a.state or (have[0] if have else "")
    if not state:
        sys.exit("refused: no load set found for %s (look in Content/Xdf), and no --state given" % a.map)
    print("story state:", state, "| this map has:", ", ".join(have) or "none found", flush=True)
    env = dict(os.environ, DK_DEVMENU="1", DK_DEVMENU_MAIN_CMD=JUMP_LINE % (state, a.map))
    exe = os.path.join(a.folder, "darknessrecomp.exe")
    subprocess.run([sys.executable, T.VRBENCH, "start", exe], capture_output=True, text=True)
    began = dt.datetime.now()
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    os.path.join(a.folder, "dk_vr_sim_hands.ps1")], env=env, capture_output=True, text=True)
    time.sleep(TITLE_WAIT_S)
    press("menu", START_HOLD_S)
    time.sleep(STEP_S)
    # The doorway first, then A: the doorway button does nothing on the save notice, A there is OK, and A is never
    # pressed on the main menu unless the doorway failed there (A on the main menu is CONTINUE).
    jumped = False
    for k in range(DOORWAY_TRIES):
        time.sleep(STEP_S)
        press(DOORWAY_BUTTON)
        time.sleep(POLL_S)
        if log_has(a.folder, began, "[DEVMENU] cheat"):
            jumped = True
            break
        time.sleep(STEP_S)
        press("a")
    print("doorway:", "taken" if jumped else "NOT taken (no [DEVMENU] line)", flush=True)
    # The level is live when the widened view is built after the jump (the log's "(widened)" line); the moments
    # count from there, since the loading time differs from run to run by tens of seconds.
    live = False
    if jumped:
        mark = dt.datetime.now()
        for _ in range(LEVEL_WAIT_S):
            time.sleep(POLL_S)
            if T.readings(a.folder, WIDE_LINE, mark):
                live = True
                break
    print("level:", "live" if live else "NOT live after %d s" % LEVEL_WAIT_S, flush=True)
    t0 = time.time()
    n = 0
    if live:
        for what, when in moments:
            time.sleep(max(0.0, t0 + when - time.time()))
            if what == "shot":
                n += 1
                path = "%s-%s-%s-%d.png" % (a.prefix, a.map, a.path, n)
                T.take_shot(path)
                print(time.strftime("%H:%M:%S"), "picture", path, flush=True)
            else:
                press(what)
    since = dt.datetime.now() - dt.timedelta(seconds=RATE_WINDOW_S)
    frames = T.rate(T.readings(a.folder, T.FRAME_LINE, since))
    pairs = T.rate(T.readings(a.folder, T.PAIR_LINE, since))
    closed = subprocess.run([sys.executable, os.path.join(HERE, "dk_close.py")]).returncode == 0
    subprocess.run([sys.executable, T.VRBENCH, "stop"], capture_output=True, text=True)
    fmt = lambda x: "none" if x is None else "%.1f" % x
    line = "\t".join([dt.datetime.now().strftime("%Y-%m-%d %H:%M"), "place-%s-%s" % (a.map, a.path),
                      "state=%s" % state, fmt(frames), fmt(pairs), "-", "closed" if closed else "STILL RUNNING"])
    with open(T.RESULTS, "a", encoding="utf-8") as f:
        f.write(line + "\n")
    print(line)
    return 0 if live and closed else 1


if __name__ == "__main__":
    sys.exit(main())
