"""ik_test.py <out-folder> [fwd right up]: does the game's hand IK follow a target of ours? The game must be running
in the level. Takes an untouched picture, then ik.on=1 with hand A, hand B, both, then off again; a picture each
time, plus ik.calls / ik.set / ik.seen and the [IK] log lines. Writes <out-folder>/sheet.png: each step's left
half (scaled) beside its difference from the untouched picture."""
import os, sys, time

sys.path.insert(0, os.path.expanduser(r"~\github-backups\staging\the-darkness-vr\bridge-2026-10-23"))
sys.path.insert(0, os.path.expanduser(r"~\github-backups\mcp-game-bridge-cubed\server"))
import numpy as np                                      # noqa: E402
from PIL import Image, ImageDraw                        # noqa: E402
import run_pair_test_bridge as rp                       # noqa: E402
from bridge_client import Bridge                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)
fwd, right, up = (sys.argv[2:5] if len(sys.argv) >= 5 else ("24", "8", "-6"))
SETTLE_S = 2.0   # the IK blends over ik.time seconds

pid = int(open(os.path.join(HERE, "dk_xr.pid")).read())
b = Bridge(pid, connect_wait=5.0)

def left(pic):
    h, w = pic.shape[:2]
    return pic[:, :w // 2] if w > 2 * h else pic

def grab():
    return left(rp.picture(b))

def counters():
    g = b.get("ik.calls", "ik.set", "ik.seen")
    return f"calls {g.get('ik.calls')} set {g.get('ik.set')} seen {g.get('ik.seen')}"

steps = []
try:
    b.set("ik.on", "0"); b.set("ik.fwd", fwd); b.set("ik.right", right); b.set("ik.up", up)
    time.sleep(SETTLE_S)
    print("before:", counters(), flush=True)
    base = grab(); steps.append(("off", base))
    for label, hand in (("hand A", "0"), ("hand B", "1"), ("both", "2")):
        b.set("ik.hand", hand); b.set("ik.on", "1"); time.sleep(SETTLE_S)
        pic = grab(); steps.append((label, pic))
        print(f"{label}: {counters()}", flush=True)
    b.set("ik.on", "0"); time.sleep(SETTLE_S)
    steps.append(("off again", grab()))
    print("after:", counters(), flush=True)
    for ln in b.log(4000):
        if "[IK]" in ln:
            print("   ", ln[ln.find("[IK]"):][:160])
finally:
    try:
        b.set("ik.on", "0")
    except Exception:
        pass
    b.close()

base = steps[0][1].astype(np.float32)
tw, th = 640, 360
sheet = Image.new("RGB", (tw * 2, (th + 16) * len(steps)), "black"); dr = ImageDraw.Draw(sheet)
for i, (label, pic) in enumerate(steps):
    d = np.abs(pic.astype(np.float32) - base)
    y = i * (th + 16)
    Image.fromarray(pic).save(os.path.join(OUT, f"{i}_{label.replace(' ', '_')}.png"))
    sheet.paste(Image.fromarray(pic).resize((tw, th)), (0, y + 16))
    sheet.paste(Image.fromarray(np.clip(d.max(axis=2) * 3, 0, 255).astype(np.uint8)).convert("RGB").resize((tw, th)), (tw, y + 16))
    dr.text((4, y + 2), f"{label}: picture | difference from off (mean {d.mean():.2f})", fill="yellow")
sheet.save(os.path.join(OUT, "sheet.png")); print("sheet:", os.path.join(OUT, "sheet.png"))
