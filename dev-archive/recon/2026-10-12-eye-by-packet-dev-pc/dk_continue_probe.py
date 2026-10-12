"""Find out why A on CONTINUE does not load the car scene on the dev PC (2026-10-12).
Launch with the B switches, reach the menu like the runner, then press A with a picture before and after,
retrying with longer waits. Leaves the game running if it reaches the level (for the next test)."""
import os, sys, time
RUNNER = r"D:\claude video game stuff\github-backups\staging\the-darkness-vr\eye-by-packet-2026-10-12"
sys.path.insert(0, RUNNER)
import run_pair_test_dev as r  # noqa: E402
from PIL import Image  # noqa: E402
import numpy as np  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dkcont")
os.makedirs(OUT, exist_ok=True)

def shot(hwnd, name):
    pic = r.grab(hwnd)
    Image.fromarray(pic[:, :, :3]).save(os.path.join(OUT, name + ".png"))
    print("  shot", name, flush=True)

env = dict(os.environ, **r.BASE_ENV, DK_EYE_BY_PACKET="1")
for kv in sys.argv[1:]:
    k, v = kv.split("=", 1); env[k] = v
print("extra", sys.argv[1:], flush=True)
pad = r.vg.VX360Gamepad(); time.sleep(1.0)
t0 = time.time()
proc = r.subprocess.Popen([r.EXE], cwd=r.GAME_DIR, env=env)
hwnd = None
while hwnd is None and time.time() - t0 < r.WINDOW_WAIT_S:
    time.sleep(1.0); hwnd = r.find_window(proc.pid)
t_title = None
while t_title is None and time.time() - t0 < r.TITLE_LATEST_S:
    time.sleep(2.0)
    t = time.time() - t0
    if t > r.TITLE_EARLIEST_S and len(np.unique(r.grab(hwnd).reshape(-1, 3), axis=0)) > r.TITLE_COLOURS:
        t_title = t
print("title at", t_title, flush=True)
for label in ["START", "A", "START"]:
    time.sleep(r.STEP_S); r.press(pad, hwnd, label); time.sleep(1.0); shot(hwnd, "after_" + label + str(int(time.time() - t0)))
time.sleep(8); ref = r.grab(hwnd).astype(float); shot(hwnd, "menu_ref")
for attempt in range(6):
    r.press(pad, hwnd, "A"); time.sleep(6.0)
    cur = r.grab(hwnd).astype(float)
    d = float(abs(cur - ref).mean()); print(f"  after A{attempt}: diff from menu {d:.1f}", flush=True)
    shot(hwnd, f"after_A{attempt}")
    if d > 12.0:
        break
    ref = cur
time.sleep(30); shot(hwnd, "after_30s")
print("pid", proc.pid, "hwnd", hwnd, flush=True)
