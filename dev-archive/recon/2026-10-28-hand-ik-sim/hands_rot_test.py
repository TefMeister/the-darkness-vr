"""hands_rot_test.py <out>: does the hand turn with the controller? Game launched with DK_HANDS=1 DK_HANDS_ROT=1.
Right controller held out in front (posY measured from head height), calibrated there, then turned by yaw / pitch
(radians); a picture each, lower-right quarter enlarged. Also: does DK_HANDS alone switch the hands on (ik.on, ik.set)?"""
import json, os, sys, time
sys.path.insert(0, os.path.expanduser(r"~\github-backups\staging\the-darkness-vr\bridge-2026-10-23"))
sys.path.insert(0, os.path.expanduser(r"~\github-backups\mcp-game-bridge-cubed\server"))
import numpy as np
from PIL import Image, ImageDraw
import run_pair_test_bridge as rp
from bridge_client import Bridge
HERE = os.path.dirname(os.path.abspath(__file__)); OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
CMD = os.path.join(os.environ["LOCALAPPDATA"], "OpenXR-Simulator", "controller_pose_command.json")
S = 1.5
POS = (0.20, -0.15, -0.61)
b = Bridge(int(open(os.path.join(HERE, "dk_xr.pid")).read()), connect_wait=5.0)
def grab():
    p = rp.picture(b); h, w = p.shape[:2]; return p[:, :w // 2] if w > 2 * h else p
def ctl(hand, x, y, z, yaw=0.0, pitch=0.0):
    with open(CMD + ".tmp", "w") as f:
        json.dump({"hand": hand, "posX": x, "posY": y, "posZ": z, "yaw": yaw, "pitch": pitch, "trigger": 0.0, "buttonA": False}, f)
    os.replace(CMD + ".tmp", CMD)
    for _ in range(40):
        if not os.path.exists(CMD): return
        time.sleep(0.05)
g = b.get("ik.on", "ik.src", "ik.rot", "ik.set", "ik.rot_set")
print("started with:", g, flush=True)
steps = []
ctl(0, -0.25, -0.70, -0.10); ctl(1, *POS); time.sleep(S)
b.set("ik.calib", "1"); time.sleep(S)
steps.append(("calibrated, straight", grab()))
for label, yaw, pitch in (("yaw +0.6", 0.6, 0.0), ("yaw -0.6", -0.6, 0.0), ("pitch +0.6", 0.0, 0.6), ("pitch -0.6", 0.0, -0.6), ("straight again", 0.0, 0.0)):
    ctl(1, *POS, yaw, pitch); time.sleep(S); steps.append((label, grab()))
    print(label, b.get("ik.rot_set").get("ik.rot_set"), flush=True)
b.set("ik.rot", "0"); ctl(1, *POS, 0.6, 0.0); time.sleep(S); steps.append(("rot OFF, yaw +0.6", grab()))
b.set("ik.rot", "1"); ctl(1, *POS, 0.0, 0.0)
for ln in b.log(8000):
    if "[IK]" in ln and ("calibrated" in ln or "DK_HANDS" in ln): print("   ", ln[ln.find("[IK]"):][:260])
b.close()
B = steps[0][1].astype(np.float32); tw, th = 480, 270
sheet = Image.new("RGB", (tw * 2, (th + 16) * len(steps)), "black"); dr = ImageDraw.Draw(sheet)
for i, (label, pic) in enumerate(steps):
    y = i * (th + 16)
    sheet.paste(Image.fromarray(pic).resize((tw, th)), (0, y + 16))
    sheet.paste(Image.fromarray(pic).crop((560, 300, 1280, 720)).resize((tw, th)), (tw, y + 16))
    dr.text((4, y + 2), f"{label}: picture | lower right, enlarged (change vs calibrated {np.abs(pic.astype(np.float32) - B).mean():.2f})", fill="yellow")
sheet.save(os.path.join(OUT, "sheet.png")); print("sheet ok")
