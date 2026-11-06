"""drive the Darkness VR build on the pretend headset through VR Bench: START (menu held), then A every 4.5 s into the car
scene, then idle so the collector and the simulator can measure. Run from anywhere."""
import os, subprocess, sys, time
VB = os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/vrbench")
sys.path.insert(0, VB)
import vb_input as I, vb_settings as S
# Timings from KNOWN.md (routes/the-darkness): the title takes ~50-90 s; START = the menu button held ~1.2 s; then A
# every 4.5 s through the save notice and CONTINUE into the level.
TITLE_WAIT_S = 56
START_HOLD_S = 1.2
A_HOLD_S = 0.2
A_EVERY_S = 4.5
A_PRESSES = 7
IDLE_DEFAULT_S = 210.0
def press(name, hold):
    kw = {name: True}
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **kw)); time.sleep(hold)
    kw = {name: False}
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **kw))
    print(time.strftime("%H:%M:%S"), "pressed", name, "held", hold, flush=True)
# 2026-11-06: the build folder is the first argument (default: the installed build-home-xr), so a test of another
# build cannot launch the installed one by mistake (it did once, after a hand edit of the path missed). Second: idle s.
folder = sys.argv[1] if len(sys.argv) > 1 else r"C:\NonSteam\the-darkness\build-home-xr"
idle = float(sys.argv[2]) if len(sys.argv) > 2 else IDLE_DEFAULT_S
script = os.path.join(folder, "dk_vr_sim_hands.ps1")
if not os.path.isfile(script):
    sys.exit("no dk_vr_sim_hands.ps1 in " + folder)
p = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script],
                   capture_output=True, text=True)
print(time.strftime("%H:%M:%S"), "launched", script, p.stdout.strip(), flush=True)
time.sleep(TITLE_WAIT_S)
press("menu", START_HOLD_S)   # START = the menu button held
for k in range(A_PRESSES):
    time.sleep(A_EVERY_S)
    press("a", A_HOLD_S)
print(time.strftime("%H:%M:%S"), "now idling for the measurement", flush=True)
time.sleep(idle)
print(time.strftime("%H:%M:%S"), "done", flush=True)
