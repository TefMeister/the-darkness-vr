"""diff_crops.py x0 y0 w h bone [bone ...]: difference (x3) of each bone picture against the baseline in one frame region,
2x, on one sheet (diff_crops.png); prints the changed-pixel count, centroid and box per bone within the region."""
import os, sys
import numpy as np
from PIL import Image, ImageDraw
D = r"C:\NonSteam\the-darkness\screens\xr-sim-2026-10-28\bones"
x0, y0, w, h = map(int, sys.argv[1:5]); bones = [int(b) for b in sys.argv[5:]]
base = np.asarray(Image.open(os.path.join(D, "baseline.png")).convert("RGB")).astype(np.float32)[y0:y0+h, x0:x0+w]
per = 5; tw, th = w * 2, h * 2
rows = (len(bones) + per - 1) // per
sheet = Image.new("RGB", (tw * per, (th + 16) * rows), "black"); dr = ImageDraw.Draw(sheet)
for i, b in enumerate(bones):
    pic = np.asarray(Image.open(os.path.join(D, f"bone{b:02d}.png")).convert("RGB")).astype(np.float32)[y0:y0+h, x0:x0+w]
    d = np.abs(pic - base).max(axis=2); m = d > 24
    ys, xs = np.where(m)
    if len(xs):
        print(f"bone {b:2d}: {m.sum():5d} px, centroid ({x0+int(xs.mean())},{y0+int(ys.mean())}), box x {x0+xs.min()}-{x0+xs.max()} y {y0+ys.min()}-{y0+ys.max()}")
    else:
        print(f"bone {b:2d}: nothing in the region")
    img = Image.fromarray(np.clip(d * 3, 0, 255).astype(np.uint8)).convert("RGB").resize((tw, th), Image.NEAREST)
    x, y = (i % per) * tw, (i // per) * (th + 16)
    sheet.paste(img, (x, y + 16)); dr.text((x + 4, y + 2), f"bone {b}", fill="yellow")
sheet.save(os.path.join(D, "diff_crops.png")); print("ok", sheet.size)
