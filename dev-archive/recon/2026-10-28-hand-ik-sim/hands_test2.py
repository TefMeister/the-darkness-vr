"""hands_test2.py <out>: right controller held out in front (reach), moved left/right/up/down; compared with the src=0
test point at the same head-relative place (24 fwd, 8 right, 6 down = 0.61 m, 0.20 m, 0.15 m)."""
import json, os, sys, time
sys.path.insert(0, os.path.expanduser(r"~\github-backups\staging\the-darkness-vr\bridge-2026-10-23"))
sys.path.insert(0, os.path.expanduser(r"~\github-backups\mcp-game-bridge-cubed\server"))
import numpy as np
from PIL import Image, ImageDraw
import run_pair_test_bridge as rp
from bridge_client import Bridge
HERE = os.path.dirname(os.path.abspath(__file__)); OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
CMD = os.path.join(os.environ["LOCALAPPDATA"], "OpenXR-Simulator", "controller_pose_command.json")
S = 1.5; HY = 0.0   # the simulator measures controller height FROM HEAD HEIGHT (posY 1.55 came back as 3.25 with the head at 1.70) [measured 2026-10-28]
b = Bridge(int(open(os.path.join(HERE, "dk_xr.pid")).read()), connect_wait=5.0)
def grab():
    p = rp.picture(b); h, w = p.shape[:2]; return p[:, :w // 2] if w > 2 * h else p
def ctl(hand, x, y, z):
    with open(CMD + ".tmp", "w") as f:
        json.dump({"hand": hand, "posX": x, "posY": y, "posZ": z, "yaw": 0.0, "pitch": 0.0, "trigger": 0.0, "buttonA": False}, f)
    os.replace(CMD + ".tmp", CMD)
    for _ in range(40):
        if not os.path.exists(CMD): return
        time.sleep(0.05)
steps = []
try:
    b.set("ik.on", "0"); ctl(0, -0.25, HY - 0.6, -0.1); time.sleep(S)   # left hand parked low, out of view
    steps.append(("off", grab()))
    b.set("ik.src", "0"); b.set("ik.hand", "0"); b.set("ik.fwd", "24"); b.set("ik.right", "8"); b.set("ik.up", "-6"); b.set("ik.on", "1"); time.sleep(S)
    steps.append(("TEST POINT 24/8/-6 on chain A", grab()))
    b.set("ik.src", "1")
    for label, p in (("right ctl same place", (0.20, HY - 0.15, -0.61)), ("right ctl 15cm left", (0.05, HY - 0.15, -0.61)),
                     ("right ctl 15cm up", (0.20, HY, -0.61)), ("right ctl 15cm down", (0.20, HY - 0.30, -0.61)),
                     ("right ctl 20cm nearer", (0.20, HY - 0.15, -0.41))):
        ctl(1, *p); time.sleep(S); steps.append((label, grab()))
        print(label, b.get("ik.last").get("ik.last"), flush=True)
    b.set("ik.right_chain", "1"); time.sleep(S); steps.append(("same, right_chain=1", grab()))
    b.set("ik.right_chain", "0"); b.set("ik.on", "0"); time.sleep(S); steps.append(("off again", grab()))
finally:
    try: b.set("ik.on", "0")
    except Exception: pass
    b.close()
B = steps[0][1].astype(np.float32); tw, th = 480, 270
sheet = Image.new("RGB", (tw * 2, (th + 16) * len(steps)), "black"); dr = ImageDraw.Draw(sheet)
for i, (label, pic) in enumerate(steps):
    d = np.abs(pic.astype(np.float32) - B); y = i * (th + 16)
    sheet.paste(Image.fromarray(pic).resize((tw, th)), (0, y + 16))
    sheet.paste(Image.fromarray(pic).crop((560, 360, 1280, 720)).resize((tw, th)), (tw, y + 16))
    dr.text((4, y + 2), f"{label}: picture | lower right quarter, 1.3x (mean change {d.mean():.2f})", fill="yellow")
sheet.save(os.path.join(OUT, "sheet.png")); print("sheet ok")
