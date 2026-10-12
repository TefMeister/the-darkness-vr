"""Measure side-by-side pairs on an already-running Darkness window (2026-10-12)."""
import json, os, sys, time
sys.path.insert(0, r"D:\claude video game stuff\github-backups\staging\the-darkness-vr\eye-by-packet-2026-10-12")
import run_pair_test_dev as r  # noqa: E402
from PIL import Image  # noqa: E402
import numpy as np  # noqa: E402

hwnd, label, n = int(sys.argv[1]), sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 200
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dkcont")
box = r.client_box(hwnd)
shifts = []
for i in range(n):
    pic = r.grab(hwnd)
    s, _ = r.best_shift(pic, box)
    if s is not None:
        shifts.append(s)
    if i < 6:
        Image.fromarray(pic[:, :, :3]).save(os.path.join(out, f"{label}_{i:02d}.png"))
    time.sleep(0.05)
a = np.array(shifts)
res = {"label": label, "pictures": len(a), "one_eye_share": round(float((abs(a) <= r.SAME_EYE_SHIFT_PX).mean()), 3),
       "positive": round(float((a > r.SAME_EYE_SHIFT_PX).mean()), 3), "negative": round(float((a < -r.SAME_EYE_SHIFT_PX).mean()), 3),
       "median_abs": float(np.median(abs(a))), "hist": {int(k): int(v) for k, v in zip(*np.unique(a, return_counts=True))}}
print(json.dumps(res))
