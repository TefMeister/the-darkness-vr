"""bone_sweep.py [first last] [mode] [d]: which bone is the hand? The game must be running (dk_xr_launch.py left it in
the level). For each bone index, set skin.bone through the bridge, wait, grab the engine frame, measure how much the
LEFT half changed from the untouched baseline, and save the half. Prints a ranked table and writes a contact sheet of
the most-changed bones (sweep_sheet.png) so the pictures can be looked at. Ends with skin.bone = -1.
mode 1 collapses the bone, mode 2 (default) shifts it by d units on each axis (default 10)."""
import os, sys, time

sys.path.insert(0, os.path.expanduser(r"~\github-backups\staging\the-darkness-vr\bridge-2026-10-23"))
sys.path.insert(0, os.path.expanduser(r"~\github-backups\mcp-game-bridge-cubed\server"))
import numpy as np                                      # noqa: E402
from PIL import Image, ImageDraw                        # noqa: E402
import run_pair_test_bridge as rp                       # noqa: E402
from bridge_client import Bridge                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = r"C:\NonSteam\the-darkness\screens\xr-sim-2026-10-28\bones"
os.makedirs(OUT, exist_ok=True)
SETTLE_S = 0.6          # after setting the bone, before the picture (the pair must be redrawn with it)
SHOTS = 2               # pictures per bone; the one that differs MOST from baseline is kept (a stale frame differs 0)
TOP = 12                # bones on the contact sheet

first = int(sys.argv[1]) if len(sys.argv) > 1 else 0
last = int(sys.argv[2]) if len(sys.argv) > 2 else 52
mode = int(sys.argv[3]) if len(sys.argv) > 3 else 2
d = float(sys.argv[4]) if len(sys.argv) > 4 else 10.0

pid = int(open(os.path.join(HERE, "dk_xr.pid")).read())
b = Bridge(pid, connect_wait=5.0)

def left(pic):
    h, w = pic.shape[:2]
    return pic[:, :w // 2] if w > 2 * h else pic

def grab():
    return left(rp.picture(b))

def diff(a, c):
    return float(np.abs(a.astype(np.float32) - c.astype(np.float32)).mean())

results = []
try:
    b.set("skin.bone", "-1")
    b.set("skin.mode", str(mode))
    for k in ("skin.dx", "skin.dy", "skin.dz"):
        b.set(k, str(d))
    time.sleep(SETTLE_S)
    base = grab()
    base2 = grab()
    noise = diff(base, base2)
    Image.fromarray(base).save(os.path.join(OUT, "baseline.png"))
    print(f"baseline {base.shape[1]}x{base.shape[0]}, scene noise between two untouched pictures: {noise:.2f}", flush=True)
    print(f"tables so far: {b.get('skin.tables').get('skin.tables')}", flush=True)
    for bone in range(first, last + 1):
        b.set("skin.bone", str(bone))
        time.sleep(SETTLE_S)
        best, best_pic = -1.0, None
        for _ in range(SHOTS):
            pic = grab()
            dd = diff(base, pic)
            if dd > best:
                best, best_pic = dd, pic
            time.sleep(0.15)
        hits = b.get('skin.hits').get('skin.hits')
        Image.fromarray(best_pic).save(os.path.join(OUT, f"bone{bone:02d}.png"))
        results.append((bone, best))
        print(f"bone {bone:2d}: change {best:6.2f}  (hits so far {hits})", flush=True)
    b.set("skin.bone", "-1")
    time.sleep(SETTLE_S)
    after = grab()
    print(f"restored: change vs baseline {diff(base, after):.2f} (noise was {noise:.2f})", flush=True)
finally:
    try:
        b.set("skin.bone", "-1")
    except Exception:
        pass
    b.close()

results.sort(key=lambda r: -r[1])
print("\nmost changed first:")
for bone, dd in results[:TOP]:
    print(f"  bone {bone:2d}  {dd:6.2f}")

# contact sheet: baseline + the TOP most-changed bones, 4 per row, each thumb 320x180
tiles = [("base", os.path.join(OUT, "baseline.png"))] + [(f"b{bone} {dd:.1f}", os.path.join(OUT, f"bone{bone:02d}.png")) for bone, dd in results[:TOP]]
tw, th, per = 320, 180, 4
rows = (len(tiles) + per - 1) // per
sheet = Image.new("RGB", (tw * per, (th + 16) * rows), "black")
dr = ImageDraw.Draw(sheet)
for i, (label, path) in enumerate(tiles):
    im = Image.open(path).resize((tw, th))
    x, y = (i % per) * tw, (i // per) * (th + 16)
    sheet.paste(im, (x, y + 16))
    dr.text((x + 4, y + 2), label, fill="yellow")
sheet.save(os.path.join(OUT, "sweep_sheet.png"))
print("sheet:", os.path.join(OUT, "sweep_sheet.png"))
