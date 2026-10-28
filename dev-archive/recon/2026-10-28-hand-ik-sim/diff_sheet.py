"""diff_sheet.py bone [bone ...]: for each bone picture, where did it change from the baseline? Prints the box of
changed pixels and writes diff_sheet.png: per bone a row of baseline crop | nudged crop | difference (whole frame)."""
import os, sys
import numpy as np
from PIL import Image, ImageDraw
D = r"C:\NonSteam\the-darkness\screens\xr-sim-2026-10-28\bones"
base = np.asarray(Image.open(os.path.join(D, "baseline.png")).convert("RGB")).astype(np.float32)
rows = []
for arg in sys.argv[1:]:
    b = int(arg)
    pic = np.asarray(Image.open(os.path.join(D, f"bone{b:02d}.png")).convert("RGB")).astype(np.float32)
    d = np.abs(pic - base).max(axis=2)
    m = d > 24
    ys, xs = np.where(m)
    if len(xs) == 0:
        print(f"bone {b}: no pixel changed by more than 24"); continue
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    print(f"bone {b}: {m.sum()} px changed, box x {x0}-{x1} y {y0}-{y1}, centre ({(x0+x1)//2},{(y0+y1)//2})")
    dimg = Image.fromarray(np.clip(d * 3, 0, 255).astype(np.uint8)).convert("RGB")
    ImageDraw.Draw(dimg).rectangle([x0, y0, x1, y1], outline="red", width=3)
    cx0, cx1 = max(0, (x0+x1)//2 - 200), min(1280, (x0+x1)//2 + 200)
    cy0, cy1 = max(0, (y0+y1)//2 - 150), min(720, (y0+y1)//2 + 150)
    crop_b = Image.fromarray(base.astype(np.uint8)).crop((cx0, cy0, cx1, cy1)).resize((400, 300))
    crop_p = Image.fromarray(pic.astype(np.uint8)).crop((cx0, cy0, cx1, cy1)).resize((400, 300))
    rows.append((b, crop_b, crop_p, dimg.resize((533, 300))))
sheet = Image.new("RGB", (400 + 400 + 533, 316 * len(rows)), "black")
dr = ImageDraw.Draw(sheet)
for i, (b, cb, cp, dd) in enumerate(rows):
    y = i * 316
    sheet.paste(cb, (0, y + 16)); sheet.paste(cp, (400, y + 16)); sheet.paste(dd, (800, y + 16))
    dr.text((4, y + 2), f"bone {b}: baseline crop | nudged crop | difference x3, whole frame", fill="yellow")
sheet.save(os.path.join(D, "diff_sheet.png")); print("sheet written")
