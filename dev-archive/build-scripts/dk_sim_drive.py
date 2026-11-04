"""drive the Darkness VR build on the pretend headset through VR Bench: START (menu held), then A every 4.5 s into the car
scene, then idle so the collector and the simulator can measure. Run from anywhere."""
import os, subprocess, sys, time
VB = os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/vrbench")
sys.path.insert(0, VB)
import vb_input as I, vb_settings as S
def press(name, hold):
    kw = {name: True}
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **kw)); time.sleep(hold)
    kw = {name: False}
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **kw))
    print(time.strftime("%H:%M:%S"), "pressed", name, "held", hold, flush=True)
p = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    r"C:\NonSteam\the-darkness\build-home-xr\dk_vr_sim_hands.ps1"], capture_output=True, text=True)
print(time.strftime("%H:%M:%S"), "launched", p.stdout.strip(), flush=True)
time.sleep(56)
press("menu", 1.2)            # START = the menu button held
for k in range(7):
    time.sleep(4.5)
    press("a", 0.2)
print(time.strftime("%H:%M:%S"), "now idling for the measurement", flush=True)
time.sleep(210)
print(time.strftime("%H:%M:%S"), "done", flush=True)
