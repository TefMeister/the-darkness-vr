"""ik_test2.py <out-folder>: (1) which IK object is the player's: feed each object seen, one at a time, hand A, picture;
(2) does the hand follow the target: three different offsets with every object fed. Sheet of pictures + differences."""
import os, sys, time
sys.path.insert(0, os.path.expanduser(r"~\github-backups\staging\the-darkness-vr\bridge-2026-10-23"))
sys.path.insert(0, os.path.expanduser(r"~\github-backups\mcp-game-bridge-cubed\server"))
import numpy as np
from PIL import Image, ImageDraw
import run_pair_test_bridge as rp
from bridge_client import Bridge
HERE = os.path.dirname(os.path.abspath(__file__)); OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
SETTLE_S = 2.0
b = Bridge(int(open(os.path.join(HERE, "dk_xr.pid")).read()), connect_wait=5.0)
def grab():
    p = rp.picture(b); h, w = p.shape[:2]; return p[:, :w // 2] if w > 2 * h else p
steps = []
try:
    b.set("ik.on", "0"); b.set("ik.hand", "0"); b.set("ik.only", "0")
    b.set("ik.fwd", "24"); b.set("ik.right", "8"); b.set("ik.up", "-6"); time.sleep(SETTLE_S)
    base = grab(); steps.append(("off", base))
    seen = b.get("ik.seen").get("ik.seen").split()
    print("objects:", seen, flush=True)
    for obj in seen:
        b.set("ik.only", str(int(obj, 16))); b.set("ik.on", "1"); time.sleep(SETTLE_S)
        steps.append((f"only {obj}", grab())); b.set("ik.on", "0"); time.sleep(SETTLE_S)
        print("done", obj, flush=True)
    b.set("ik.only", "0")
    for label, f, r, u in (("fwd24 right-12 up-6", 24, -12, -6), ("fwd24 right8 up10", 24, 8, 10), ("fwd40 right0 up-6", 40, 0, -6), ("fwd12 right16 up-12", 12, 16, -12)):
        b.set("ik.fwd", str(f)); b.set("ik.right", str(r)); b.set("ik.up", str(u)); b.set("ik.on", "1"); time.sleep(SETTLE_S)
        steps.append((label, grab())); print("done", label, flush=True)
    b.set("ik.on", "0")
finally:
    try: b.set("ik.on", "0")
    except Exception: pass
    b.close()
B = base.astype(np.float32); tw, th = 480, 270
sheet = Image.new("RGB", (tw * 2, (th + 16) * len(steps)), "black"); dr = ImageDraw.Draw(sheet)
for i, (label, pic) in enumerate(steps):
    d = np.abs(pic.astype(np.float32) - B); y = i * (th + 16)
    Image.fromarray(pic).save(os.path.join(OUT, f"{i}_{label.replace(' ', '_')}.png"))
    sheet.paste(Image.fromarray(pic).resize((tw, th)), (0, y + 16))
    sheet.paste(Image.fromarray(np.clip(d.max(axis=2) * 3, 0, 255).astype(np.uint8)).convert("RGB").resize((tw, th)), (tw, y + 16))
    dr.text((4, y + 2), f"{label}: picture | difference from off (mean {d.mean():.2f})", fill="yellow")
    print(f"{label}: mean change {d.mean():.2f}")
sheet.save(os.path.join(OUT, "sheet.png")); print("sheet:", os.path.join(OUT, "sheet.png"))
