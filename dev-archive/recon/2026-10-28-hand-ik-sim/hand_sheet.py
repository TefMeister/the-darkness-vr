"""hand_sheet.py x0 y0 w h bone [bone ...]: the same frame region from the baseline and each bone picture, 2x, on one sheet."""
import os, sys
from PIL import Image, ImageDraw
D = r"C:\NonSteam\the-darkness\screens\xr-sim-2026-10-28\bones"
x0, y0, w, h = map(int, sys.argv[1:5]); bones = sys.argv[5:]
tiles = [("base", Image.open(os.path.join(D, "baseline.png")))] + [(f"bone {b}", Image.open(os.path.join(D, f"bone{int(b):02d}.png"))) for b in bones]
per = 4; tw, th = w, h
rows = (len(tiles) + per - 1) // per
sheet = Image.new("RGB", (tw * per, (th + 16) * rows), "black"); dr = ImageDraw.Draw(sheet)
for i, (label, im) in enumerate(tiles):
    x, y = (i % per) * tw, (i // per) * (th + 16)
    sheet.paste(im.crop((x0, y0, x0 + w, y0 + h)), (x, y + 16)); dr.text((x + 4, y + 2), label, fill="yellow")
sheet.save(os.path.join(D, "hand_sheet.png")); print("ok", sheet.size)
