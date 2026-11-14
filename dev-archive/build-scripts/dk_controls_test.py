"""dk_controls_test.py -- Tefa's Quest layout (job 1, 2026-11-14) on the pretend headset, in a TEST copy.

Reaches gameplay the dk_sim_drive.py way (VR Bench: START, then A), then makes each gesture of the layout with VR Bench
and reads back, from the game's log, the pad the game was sent (`[DKXR] quest pad 0x....` lines, one per change).
Pictures of the game's window go to the picture folder (outside every repo: game content). Closes the game with
dk_close.py. The thumb rest cannot be pressed by VR Bench: that part is for the headset.

  python dk_controls_test.py <test build folder> <picture folder>

Refuses a folder whose name has no "test" in it, and refuses while another game holds this PC (dk_clash.py).
"""
import datetime as dt, glob, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dk_clash, dk_try as T
VB = os.path.dirname(T.VRBENCH)
sys.path.insert(0, VB)
import vb_input as I, vb_settings as S

TITLE_WAIT_S = 56     # KNOWN.md: the title is up by then
START_HOLD_S = 1.2
A_EVERY_S = 4.5
A_PRESSES = 7
SETTLE_S = 20         # in the level, before the first gesture
GAP_S = 3.0           # between gestures
PAD_LINE = re.compile(r"^\[(.{23})\].*\[DKXR\] quest pad 0x([0-9A-F]{4}) \(raw 0x([0-9A-F]{4}), (level|menu)\)")
L, R = S.HAND_LEFT, S.HAND_RIGHT

# (name, hand, what to set, seconds held, the pad bit expected while / after, take a picture after?)
GESTURES = [
    ("A = use",                 R, {"a": True},             0.2, 0x1000, False),
    ("stick up 0.6 s = jump",   R, {"stick": (0.0, 0.9)},   0.6, 0x8000, False),
    ("stick up 0.15 s = none",  R, {"stick": (0.0, 0.9)},   0.15, None,  False),
    ("turn right 1 s, no jump", R, {"stick": (0.9, 0.5)},   1.0, None,   False),
    ("stick down 0.6 s = crouch", R, {"stick": (0.0, -0.9)}, 0.6, 0x0040, True),
    ("stick down again",        R, {"stick": (0.0, -0.9)},  0.6, 0x0040, True),
    ("Y = the Darkness",        L, {"y": True},             0.2, 0x0100, True),
    ("left trigger = power",    L, {"trigger": 0.9},        0.4, 0x0200, True),
    ("Y = the Darkness away",   L, {"y": True},             0.2, 0x0100, False),
    ("B tap = reload",          R, {"b": True},             0.15, 0x2000, False),
    ("B held 1.2 s = nothing",  R, {"b": True},             1.2, None,   False),
    ("X = darklings",           L, {"x": True},             0.2, 0x4000, False),
    ("menu tap = journal",      L, {"menu": True},          0.15, 0x0020, True),
    ("menu tap = journal off",  L, {"menu": True},          0.15, 0x0020, False),
    ("menu hold = pause",       L, {"menu": True},          0.6, 0x0010, True),
    ("menu hold = pause off",   L, {"menu": True},          0.6, 0x0010, False),
]
REST = {"a": False, "b": False, "x": False, "y": False, "menu": False, "trigger": 0.0, "stick": (0.0, 0.0)}


def send(hand, values):
    I.write(I.Input().mode(poses="pass", buttons="set").hand(hand, **values))


def press(name, hold):   # the dk_sim_drive way, right hand
    send(R, {name: True}); time.sleep(hold); send(R, {name: False})


def rest():
    I.write(I.Input().mode(poses="pass", buttons="set").hand(L, **REST).hand(R, **REST))


def pad_lines(folder, since):
    out = []
    for path in sorted(glob.glob(os.path.join(folder, "logs", "*.log")), key=os.path.getmtime):
        if os.path.getmtime(path) < since.timestamp() - 5:
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = PAD_LINE.match(line)
                if m:
                    t = dt.datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S.%f")
                    if t >= since:
                        out.append((t, int(m.group(2), 16), m.group(4)))
    return out


def main():
    folder, pics = sys.argv[1], sys.argv[2]
    if "test" not in os.path.basename(os.path.normpath(folder)).lower():
        sys.exit("refused: the folder name has no 'test' in it")
    os.makedirs(pics, exist_ok=True)
    dk_clash.take_or_exit()
    exe = os.path.join(folder, "darknessrecomp.exe")
    subprocess.run([sys.executable, T.VRBENCH, "start", exe], capture_output=True, text=True)
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    os.path.join(folder, "dk_vr_sim_hands.ps1")], capture_output=True, text=True)
    time.sleep(TITLE_WAIT_S)
    press("menu", START_HOLD_S)
    for _ in range(A_PRESSES):
        time.sleep(A_EVERY_S)
        press("a", 0.2)
    time.sleep(SETTLE_S)
    T.take_shot(os.path.join(pics, "00-level.png"))
    results = []
    for i, (name, hand, values, hold, bit, shot) in enumerate(GESTURES, 1):
        began = dt.datetime.now()
        send(hand, values); time.sleep(hold); rest()
        time.sleep(1.0)
        if shot:
            T.take_shot(os.path.join(pics, "%02d-%s.png" % (i, re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-"))))
        time.sleep(GAP_S - 1.0)
        results.append((name, bit, began))
    time.sleep(2.0)
    lines = pad_lines(folder, results[0][2] - dt.timedelta(seconds=1))
    print("\n%-28s %-8s %s" % ("gesture", "expected", "seen in the game's pad"))
    ok_all = True
    for k, (name, bit, began) in enumerate(results):
        end = results[k + 1][2] if k + 1 < len(results) else dt.datetime.now()
        seen = 0
        for t, b, mode in lines:
            if began <= t < end:   # 2026-11-14: to the millisecond; the first run's +1 s spilled into the next gesture
                seen |= b
        good = (seen & bit) != 0 if bit else seen == 0
        ok_all &= good
        print("%-28s %-8s 0x%04X %s" % (name, "0x%04X" % bit if bit else "none", seen, "ok" if good else "CHECK"))
    closed = subprocess.run([sys.executable, os.path.join(HERE, "dk_close.py")]).returncode == 0
    subprocess.run([sys.executable, T.VRBENCH, "stop"], capture_output=True, text=True)
    print("closed" if closed else "STILL RUNNING")
    return 0 if ok_all and closed else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    finally:   # never leave VR Bench's layer on: it spoiled a Fallout headset run once (2026-11-14)
        subprocess.run([sys.executable, T.VRBENCH, "stop"], capture_output=True, text=True)
