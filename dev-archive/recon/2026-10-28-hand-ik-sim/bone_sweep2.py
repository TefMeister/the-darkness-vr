"""bone_sweep2.py <out-folder> <mode> <d> bone [bone ...]: like bone_sweep.py but drift-proof. Per bone: an untouched
picture A, the nudged picture B, an untouched picture C right after. Reports diff(A,B) = effect (+ drift) and
diff(A,C) = drift alone, saves A/B/C, and writes <out-folder>/sheet.png: per bone a row A | B | difference(A,B) x3
of the arm region (x 640-880, y 0-240, 2x)."""
import os, sys, time

sys.path.insert(0, os.path.expanduser(r"~\github-backups\staging\the-darkness-vr\bridge-2026-10-23"))
sys.path.insert(0, os.path.expanduser(r"~\github-backups\mcp-game-bridge-cubed\server"))
import numpy as np                                      # noqa: E402
from PIL import Image, ImageDraw                        # noqa: E402
import run_pair_test_bridge as rp                       # noqa: E402
from bridge_client import Bridge                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1]
mode = int(sys.argv[2]); d = float(sys.argv[3]); bones = [int(x) for x in sys.argv[4:]]
os.makedirs(OUT, exist_ok=True)
SETTLE_S = 0.8
REGION = (640, 0, 880, 240)

pid = int(open(os.path.join(HERE, "dk_xr.pid")).read())
b = Bridge(pid, connect_wait=5.0)

def left(pic):
    h, w = pic.shape[:2]
    return pic[:, :w // 2] if w > 2 * h else pic

def grab():
    return left(rp.picture(b))

def diff(a, c):
    return float(np.abs(a.astype(np.float32) - c.astype(np.float32)).mean())

rows = []
try:
    b.set("skin.bone", "-1"); b.set("skin.mode", str(mode))
    for k in ("skin.dx", "skin.dy", "skin.dz"):
        b.set(k, str(d))
    time.sleep(SETTLE_S)
    for bone in bones:
        A = grab()
        b.set("skin.bone", str(bone)); time.sleep(SETTLE_S)
        B = grab()
        b.set("skin.bone", "-1"); time.sleep(SETTLE_S)
        C = grab()
        e, drift = diff(A, B), diff(A, C)
        for n, p in (("A", A), ("B", B), ("C", C)):
            Image.fromarray(p).save(os.path.join(OUT, f"bone{bone:02d}_{n}.png"))
        print(f"bone {bone:2d}: effect {e:5.2f}  drift {drift:5.2f}  -> {'EFFECT' if e > 3 * drift + 0.3 else 'nothing clear'}", flush=True)
        rows.append((bone, A, B, e, drift))
finally:
    try:
        b.set("skin.bone", "-1")
    except Exception:
        pass
    b.close()

x0, y0, x1, y1 = REGION
tw, th = (x1 - x0) * 2, (y1 - y0) * 2
sheet = Image.new("RGB", (tw * 3, (th + 16) * len(rows)), "black"); dr = ImageDraw.Draw(sheet)
for i, (bone, A, B, e, drift) in enumerate(rows):
    y = i * (th + 16)
    a = Image.fromarray(A).crop(REGION).resize((tw, th)); bb = Image.fromarray(B).crop(REGION).resize((tw, th))
    dd = np.abs(B.astype(np.float32) - A.astype(np.float32)).max(axis=2)[y0:y1, x0:x1]
    di = Image.fromarray(np.clip(dd * 3, 0, 255).astype(np.uint8)).convert("RGB").resize((tw, th), Image.NEAREST)
    sheet.paste(a, (0, y + 16)); sheet.paste(bb, (tw, y + 16)); sheet.paste(di, (2 * tw, y + 16))
    dr.text((4, y + 2), f"bone {bone} mode {mode} d {d}: before | nudged | difference  (effect {e:.2f}, drift {drift:.2f})", fill="yellow")
sheet.save(os.path.join(OUT, "sheet.png")); print("sheet:", os.path.join(OUT, "sheet.png"))
