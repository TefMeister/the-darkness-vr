"""hands_test.py <out-folder>: do the motion controllers drive the hands? The game must be running in the level on the
OpenXR simulator. Moves the simulated controllers by writing the simulator's controller_pose_command.json (hand 0/1,
posX/posY/posZ in metres, yaw/pitch in radians [hypothesis from the DLL's strings]), with ik.on=1 ik.src=1, and takes
a picture per pose; prints ik.last (the world points written) and the [XR] hands / [DKXR] controller log lines.
Writes <out-folder>/sheet.png."""
import json, os, sys, time

sys.path.insert(0, os.path.expanduser(r"~\github-backups\staging\the-darkness-vr\bridge-2026-10-23"))
sys.path.insert(0, os.path.expanduser(r"~\github-backups\mcp-game-bridge-cubed\server"))
import numpy as np                                      # noqa: E402
from PIL import Image, ImageDraw                        # noqa: E402
import run_pair_test_bridge as rp                       # noqa: E402
from bridge_client import Bridge                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)
SIM = os.path.join(os.environ["LOCALAPPDATA"], "OpenXR-Simulator")
CMD = os.path.join(SIM, "controller_pose_command.json")
SETTLE_S = 1.5
HEAD_Y = 1.7   # the simulator's default head height (its set_head_pose default)

b = Bridge(int(open(os.path.join(HERE, "dk_xr.pid")).read()), connect_wait=5.0)

def grab():
    p = rp.picture(b); h, w = p.shape[:2]
    return p[:, :w // 2] if w > 2 * h else p

def controller(hand, x, y, z, yaw=0.0, pitch=0.0):
    tmp = CMD + ".tmp"
    with open(tmp, "w") as f:
        json.dump({"hand": hand, "posX": x, "posY": y, "posZ": z, "yaw": yaw, "pitch": pitch,
                   "trigger": 0.0, "buttonA": False}, f)
    os.replace(tmp, CMD)
    for _ in range(40):          # the runtime consumes the file when it reads it
        if not os.path.exists(CMD):
            return True
        time.sleep(0.05)
    return False

def last():
    g = b.get("ik.last", "ik.set", "ik.no_hands")
    return f"set {g.get('ik.set')} no_hands {g.get('ik.no_hands')} last {g.get('ik.last')}"

steps = []
try:
    b.set("ik.on", "0"); b.set("ik.src", "1"); time.sleep(SETTLE_S)
    steps.append(("off", grab()))
    print("seen:", b.get("ik.seen").get("ik.seen"), flush=True)
    # both controllers to a resting place in front of the chest, then the hook on
    print("left  consumed:", controller(0, -0.20, HEAD_Y - 0.35, -0.35))
    print("right consumed:", controller(1, 0.20, HEAD_Y - 0.35, -0.35))
    b.set("ik.on", "1"); time.sleep(SETTLE_S)
    steps.append(("both at chest", grab())); print("chest:", last(), flush=True)
    for label, hand, pos in (("right up high", 1, (0.20, HEAD_Y + 0.05, -0.45)),
                             ("right far right", 1, (0.55, HEAD_Y - 0.30, -0.30)),
                             ("right back to chest", 1, (0.20, HEAD_Y - 0.35, -0.35)),
                             ("left up high", 0, (-0.20, HEAD_Y + 0.05, -0.45)),
                             ("left far left", 0, (-0.55, HEAD_Y - 0.30, -0.30))):
        print(f"{label}: consumed {controller(hand, *pos)}", flush=True)
        time.sleep(SETTLE_S)
        steps.append((label, grab())); print("   ", last(), flush=True)
    b.set("ik.on", "0"); time.sleep(SETTLE_S)
    steps.append(("off again", grab()))
    for ln in b.log(6000):
        if "[XR] hands" in ln or "controller sampling" in ln or "[IK]" in ln:
            print("   ", ln[ln.find("[", 30):][:170])
finally:
    try:
        b.set("ik.on", "0")
    except Exception:
        pass
    b.close()

base = steps[0][1].astype(np.float32)
tw, th = 480, 270
sheet = Image.new("RGB", (tw * 2, (th + 16) * len(steps)), "black"); dr = ImageDraw.Draw(sheet)
for i, (label, pic) in enumerate(steps):
    d = np.abs(pic.astype(np.float32) - base); y = i * (th + 16)
    Image.fromarray(pic).save(os.path.join(OUT, f"{i}_{label.replace(' ', '_')}.png"))
    sheet.paste(Image.fromarray(pic).resize((tw, th)), (0, y + 16))
    sheet.paste(Image.fromarray(np.clip(d.max(axis=2) * 3, 0, 255).astype(np.uint8)).convert("RGB").resize((tw, th)), (tw, y + 16))
    dr.text((4, y + 2), f"{label}: picture | difference from off (mean {d.mean():.2f})", fill="yellow")
sheet.save(os.path.join(OUT, "sheet.png")); print("sheet:", os.path.join(OUT, "sheet.png"))
