"""Reduce a PER-DRAW constant dump to one sample per TIME SLICE, by averaging.

Why not group by the dump's 'frame' field: in ReXGlue that is CommandProcessor::counter(), which is
incremented on every swap AND on every vblank (GraphicsSystem::MarkVblank), so it runs far faster
than real frames and splits one frame into many tiny groups.

Why the mean works: for a world object, c4..c6 = R_view x R_model. Averaged over many objects that is
R_view x mean(R_model), and the object mix is roughly steady, so the average tracks the camera's
rotation. Objects locked to the view (first-person hands, HUD) contribute a constant, not noise.

In/out format: u64 epoch_ms, u32 frame, u32 count(=8), 8*4 f32. Out 'frame' = slice index.
"""
import struct, sys
import numpy as np

def main(src, dst, slice_ms=100):
    raw = np.fromfile(src, dtype=np.uint8)
    rec = 16 + 32 * 4
    n = len(raw) // rec
    raw = raw[: n * rec].reshape(n, rec)
    t = raw[:, 0:8].copy().view("<u8").ravel()
    vals = raw[:, 16:].copy().view("<f4").reshape(n, 32)
    vals = np.nan_to_num(vals, nan=0.0, posinf=0.0, neginf=0.0)
    b = (t // slice_ms).astype(np.int64)
    order = np.argsort(b, kind="stable")
    b, t, vals = b[order], t[order], vals[order]
    uniq, start, counts = np.unique(b, return_index=True, return_counts=True)
    sums = np.add.reduceat(vals.astype(np.float64), start, axis=0)
    means = (sums / counts[:, None]).astype("<f4")
    tmid = (uniq * slice_ms + slice_ms // 2).astype("<u8")
    with open(dst, "wb") as f:
        for i in range(len(uniq)):
            f.write(struct.pack("<QII", int(tmid[i]), i, 8)); f.write(means[i].tobytes())
    print(f"draws={n}  slices={len(uniq)} @ {slice_ms} ms  draws/slice median={int(np.median(counts))}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 100)
