"""Reduce a PER-DRAW constant dump to ONE camera sample per frame.

Consecutive draws are different objects, so raw per-draw values jump about and cannot be diffed.
For static world geometry the object's own transform is identity, so its c4..c6 (model rotate into
view space) IS the camera rotation - and static scenery is most of a frame's draws. So per frame we
take the MOST COMMON c4..c6 across that frame's draws as the camera, and keep the share it covered.

In:  u64 epoch_ms, u32 frame, u32 count(=8), 8*4 f32   (per draw)
Out: same format, one record per frame, values from a draw carrying the modal c4..c6
"""
import struct, sys
from collections import Counter, defaultdict
import numpy as np

def read(path):
    raw = open(path, "rb").read(); off = 0; out = []
    while off + 16 <= len(raw):
        t, fr, n = struct.unpack_from("<QII", raw, off); off += 16
        k = n * 4
        if off + 4 * k > len(raw): break
        out.append((t, fr, np.frombuffer(raw, "<f4", k, off).copy())); off += 4 * k
    return out

def main(src, dst, decimals=3):
    recs = read(src)
    if not recs: raise SystemExit("empty dump")
    frames = defaultdict(list)
    for t, fr, v in recs: frames[fr].append((t, v))
    with open(dst, "wb") as f:
        shares, sizes = [], []
        for fr in sorted(frames):
            draws = frames[fr]
            keys = [tuple(np.round(v[16:28], decimals)) for _, v in draws]   # c4.xyzw..c6.xyzw
            key, hits = Counter(keys).most_common(1)[0]
            rep = next(v for (_, v), k in zip(draws, keys) if k == key)
            t = int(np.median([d[0] for d in draws]))
            f.write(struct.pack("<QII", t, fr, 8)); f.write(rep.astype("<f4").tobytes())
            shares.append(hits / len(draws)); sizes.append(len(draws))
    print(f"draw records={len(recs)}  frames={len(frames)}  draws/frame median={int(np.median(sizes))}  "
          f"modal c4-c6 share median={np.median(shares):.0%} (min {min(shares):.0%})")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
