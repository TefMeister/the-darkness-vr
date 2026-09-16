"""Rank The Darkness's vertex-shader float constants by how much they move WITH the look stick
versus while idle.

Dump format (little-endian), one record per sample - per frame or per draw, it does not matter:
    u64 epoch_ms, u32 frame, u32 count, then count x 4 f32  (constant i, components x y z w)

A constant belongs to the camera if it changes during stick moves and holds still during idle gaps.
The view matrix should appear as a small contiguous run of constants that light up together, and
a turn right versus a turn left should push its values in opposite directions.
"""
import json, struct, sys
import numpy as np

def load_dump(path):
    raw = open(path, "rb").read()
    recs, off = [], 0
    while off + 16 <= len(raw):
        t, frame, count = struct.unpack_from("<QII", raw, off); off += 16
        n = count * 4
        if off + 4 * n > len(raw):
            break
        vals = np.frombuffer(raw, dtype="<f4", count=n, offset=off); off += 4 * n
        recs.append((t, frame, vals))
    if not recs:
        raise SystemExit("dump is empty")
    t = np.array([r[0] for r in recs], dtype=np.float64)
    V = np.stack([r[2] for r in recs])          # samples x (count*4)
    return t, V

def window_mask(t, events, labels):
    m = np.zeros(len(t), dtype=bool)
    for e in events:
        if e["label"] in labels:
            m |= (t >= e["start_ms"]) & (t <= e["end_ms"] + 150)   # small tail for the ramp-down
    return m

def main(dump, timeline, top=24):
    t, V = load_dump(dump)
    tl = json.load(open(timeline, encoding="utf-8"))
    ev = tl["events"]
    if len(t) < 3:
        raise SystemExit("too few samples")
    V = np.nan_to_num(V, nan=0.0, posinf=0.0, neginf=0.0)
    dV = np.abs(np.diff(V, axis=0))               # change between consecutive samples
    tm = t[1:]
    move = window_mask(tm, ev, {"right", "left", "up", "down"})
    idle = window_mask(tm, ev, {"idle"})
    print(f"samples={len(t)}  span={(t[-1]-t[0])/1000:.1f}s  components={V.shape[1]}  "
          f"stick-samples={int(move.sum())}  idle-samples={int(idle.sum())}")
    if move.sum() == 0 or idle.sum() == 0:
        raise SystemExit("timeline does not overlap the dump - check clocks / that gameplay was reached")

    m_move = dV[move].mean(axis=0)
    m_idle = dV[idle].mean(axis=0)
    score = m_move / (m_idle + 1e-6)
    active = m_move > 1e-4
    order = np.argsort(-(score * active))

    # Direction test uses the SIGNED rate of change inside each window, not the mean level:
    # after a turn the camera stays turned, so levels drift and cannot show direction.
    sV = np.diff(V, axis=0)
    def signed_rate(labels):
        mk = window_mask(tm, ev, labels)
        return sV[mk].mean(axis=0) if mk.any() else np.full(V.shape[1], np.nan)
    d_right = signed_rate({"right"})
    d_left = signed_rate({"left"})

    print("\nrank  const.comp   move/idle   mean|d|move  mean|d|idle   right-vs-idle  left-vs-idle  opposite?")
    rows = []
    for rank, k in enumerate(order[:top]):
        i, c = divmod(int(k), 4)
        opp = bool(np.sign(d_right[k]) != np.sign(d_left[k]) and abs(d_right[k]) > 1e-4 and abs(d_left[k]) > 1e-4)
        rows.append({"const": i, "comp": "xyzw"[c], "ratio": float(score[k]), "move": float(m_move[k]),
                     "idle": float(m_idle[k]), "right": float(d_right[k]), "left": float(d_left[k]), "opposite": opp})
        print(f"{rank:>4}  c{i:<3}.{'xyzw'[c]}   {score[k]:>9.1f}   {m_move[k]:>10.5f}  {m_idle[k]:>10.5f}   "
              f"{d_right[k]:>+12.5f}  {d_left[k]:>+12.5f}   {opp}")

    # group: which whole constants light up
    per_const = (score * active).reshape(-1, 4).max(axis=1)
    hot = np.argsort(-per_const)[:12]
    print("\nconstants most tied to the stick (max component ratio):",
          ", ".join(f"c{int(i)}={per_const[i]:.0f}" for i in hot if per_const[i] > 0))
    json.dump({"rows": rows, "hot_constants": [int(i) for i in hot if per_const[i] > 0]},
              open(dump + ".analysis.json", "w"), indent=1)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
