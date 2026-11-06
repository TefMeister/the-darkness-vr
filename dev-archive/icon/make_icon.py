import math, sys
from PIL import Image, ImageDraw, ImageFilter
N = 1024
def bez(p, t):
    p0, p1, p2, p3 = p; u = 1 - t
    return tuple(u**3*a + 3*u*u*t*b + 3*u*t*t*c + t**3*d for a, b, c, d in zip(p0, p1, p2, p3))
def stroke(d, p, r0, r1, fill, steps=500):
    for i in range(steps + 1):
        t = i / steps; x, y = bez(p, t); r = r0 + (r1 - r0) * t
        d.ellipse([x - r, y - r, x + r, y + r], fill=fill)
def xf(pts, cx, cy, ang, sx, sc=1.0):
    ca, sa = math.cos(ang), math.sin(ang)
    return [(cx + (x * sx * ca - y * sa) * sc, cy + (x * sx * sa + y * ca) * sc) for x, y in pts]
# head shapes (local: facing +x, origin at the neck end)
UPPER = [(-40, -52), (40, -70), (120, -58), (205, -30), (235, -4), (150, 2), (60, 10), (-40, 30)]
LOWER = [(-10, 30), (70, 30), (150, 44), (200, 70), (150, 76), (60, 70), (-20, 60)]
TEETH_U = [[(a, 2 + (a - 60) * 0.0), (a + 14, 2), (a + 7, 30)] for a in (80, 112, 144, 176)]
TEETH_L = [[(a, 44), (a + 14, 46), (a + 7, 20)] for a in (90, 122, 154)]
EYE = [(105, -40), (175, -32), (120, -24)]
def build(scale):
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    base = Image.new("RGBA", (N, N), (5, 3, 7, 255))
    # deep red heart
    halo = Image.new("RGBA", (N, N), (0, 0, 0, 0)); h = ImageDraw.Draw(halo)
    for k in range(70, 0, -1):
        r = 520 * k / 70; a = int(170 * (1 - k / 70) ** 1.8)
        h.ellipse([512 - r, 600 - r, 512 + r, 600 + r], fill=(110, 6, 30, a))
    base = Image.alpha_composite(base, halo)
    tendrils = []
    for s in (-1, 1):
        cx = 512
        path = [(cx + 60 * s, 1120), (cx + 500 * s, 900), (cx + 470 * s, 330), (cx + 300 * s, 380)]
        neck_end = path[3]
        ang = 0.0 if s < 0 else math.pi   # left head faces right, right head faces left
        tendrils.append((path, neck_end, s, ang))
    # glow layer
    glow = Image.new("RGBA", (N, N), (0, 0, 0, 0)); g = ImageDraw.Draw(glow)
    for path, ne, s, ang in tendrils:
        stroke(g, path, 100, 52, (200, 30, 60, 255))
        sx = 1 if s < 0 else -1
        g.polygon(xf(UPPER, ne[0], ne[1], 0.18 * -sx * -1, sx, 0.86), fill=(200, 30, 60, 255))
        g.polygon(xf(LOWER, ne[0], ne[1], 0.18 * -sx * -1, sx, 0.86), fill=(200, 30, 60, 255))
    base = Image.alpha_composite(base, glow.filter(ImageFilter.GaussianBlur(34)))
    # black bodies with a dark rim
    body = Image.new("RGBA", (N, N), (0, 0, 0, 0)); b = ImageDraw.Draw(body)
    for path, ne, s, ang in tendrils:
        sx = 1 if s < 0 else -1
        stroke(b, path, 88, 46, (70, 14, 28, 255))
        b.polygon(xf(UPPER, ne[0], ne[1], 0.18 * sx, sx, 0.82), fill=(70, 14, 28, 255))
        b.polygon(xf(LOWER, ne[0], ne[1], 0.18 * sx, sx, 0.82), fill=(70, 14, 28, 255))
    base = Image.alpha_composite(base, body)
    inner = Image.new("RGBA", (N, N), (0, 0, 0, 0)); i = ImageDraw.Draw(inner)
    for path, ne, s, ang in tendrils:
        sx = 1 if s < 0 else -1
        stroke(i, [(x + 4 * -s, y + 8) for x, y in path], 78, 38, (7, 5, 9, 255))
        i.polygon(xf(UPPER, ne[0], ne[1] + 6, 0.18 * sx, sx, 0.76), fill=(7, 5, 9, 255))
        i.polygon(xf(LOWER, ne[0], ne[1] + 4, 0.18 * sx, sx, 0.74), fill=(7, 5, 9, 255))
    base = Image.alpha_composite(base, inner)
    # teeth
    teeth = Image.new("RGBA", (N, N), (0, 0, 0, 0)); t = ImageDraw.Draw(teeth)
    for path, ne, s, ang in tendrils:
        sx = 1 if s < 0 else -1
        for tri in TEETH_U + TEETH_L:
            t.polygon(xf(tri, ne[0], ne[1], 0.18 * sx, sx, 0.82), fill=(225, 205, 175, 255))
    base = Image.alpha_composite(base, teeth)
    # eyes: amber glow + hot core
    eg = Image.new("RGBA", (N, N), (0, 0, 0, 0)); e = ImageDraw.Draw(eg)
    for path, ne, s, ang in tendrils:
        sx = 1 if s < 0 else -1
        e.polygon(xf([(70, -66), (205, -40), (95, -4)], ne[0], ne[1], 0.18 * sx, sx, 0.82), fill=(255, 140, 10, 255))
    glow_e = eg.filter(ImageFilter.GaussianBlur(22))
    base = Image.alpha_composite(base, glow_e); base = Image.alpha_composite(base, glow_e)
    core = Image.new("RGBA", (N, N), (0, 0, 0, 0)); c = ImageDraw.Draw(core)
    for path, ne, s, ang in tendrils:
        sx = 1 if s < 0 else -1
        c.polygon(xf([(95, -48), (185, -34), (110, -20)], ne[0], ne[1], 0.18 * sx, sx, 0.82), fill=(255, 238, 160, 255))
    base = Image.alpha_composite(base, core)
    mask = Image.new("L", (N, N), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, N - 1, N - 1], radius=190, fill=255)
    img.paste(base, (0, 0), mask)
    return img
out = build(1.0)
out.save(sys.argv[1] + ".png")
big = out.resize((256, 256), Image.LANCZOS)
big.save(sys.argv[1] + ".ico", sizes=[(s, s) for s in (256, 128, 64, 48, 32, 24, 16)])
out.resize((48, 48), Image.LANCZOS).save(sys.argv[1] + "-48.png")
