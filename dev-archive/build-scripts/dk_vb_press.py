"""dk_vb_press.py <button> [seconds=0.2] [hand=r|l] -- The Darkness VR on the OpenXR Simulator with VR Bench: ONE press
of one controller button (a, b, x, y, menu, trigger, squeeze, stick_click) or one push of a stick
(up, down, left, right). Default hand: right, except x / y, which are the left hand's.
  START on the title = `dk_vb_press.py menu 1.2`; OK / pick a line = `dk_vb_press.py a`; one line down = `... down l`.
VR Bench must be on for the game (`vrbench.py start <exe>`). One writer at a time: never beside dk_sim_drive.py.
Leaves the controllers free ("pass") afterwards. 2026-11-19."""
import os, sys, time
sys.path.insert(0, os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/vrbench"))
import vb_input as I, vb_settings as S

BUTTONS = ("a", "b", "x", "y", "menu", "trigger", "squeeze", "stick_click")
STICK = {"up": (0.0, 1.0), "down": (0.0, -1.0), "left": (-1.0, 0.0), "right": (1.0, 0.0)}
if len(sys.argv) < 2 or sys.argv[1] not in BUTTONS + tuple(STICK):
    sys.exit(__doc__)
what = sys.argv[1]
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 0.2
side = sys.argv[3] if len(sys.argv) > 3 else ("l" if what in ("x", "y") else "r")
hand = S.HAND_LEFT if side == "l" else S.HAND_RIGHT


def put(on):
    kw = {}
    if what in STICK:
        kw["stick"] = list(STICK[what]) if on else [0.0, 0.0]
    elif what in ("trigger", "squeeze"):
        kw[what] = 1.0 if on else 0.0
    else:
        kw[what] = bool(on)
    I.write(I.Input().mode(poses="pass", buttons="set").hand(hand, **kw))


put(True); time.sleep(secs); put(False); time.sleep(0.15)
print(time.strftime("%H:%M:%S"), "pressed", what, "hand", side, "for", secs, "s", flush=True)
