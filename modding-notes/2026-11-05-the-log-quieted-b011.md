# 2026-11-05: the log quieted (b011)

Home PC, Opus, no game run. Board row: the frame-rate row's step (b), and the "controls laggy + the log flood" row.

On the pretend headset (2026-11-04) the log grew at about 1,200 lines a second: 93,016 `PM4_DRAW_INDX_2 ... Failed in
backend`, 93,016 `Resolve region is empty` and 95,440 of our own `[VP] PERSP` lines in 4 minutes, so the 5 MB log files
rotated every 20-30 s and pushed the useful lines out `[measured 2026-11-04, simulator, n=1]`.

What changed:

- **Our `[VP] PERSP` / `ortho` lines are off unless `DK_VP_LOG=1`** (`vr_viewport_probe.cpp`, `PerspLog()`). The stereo
  shift itself is untouched: only the logging returns early. `count_frames.py` counts frames from these lines, so
  `framerate_run.py` now sets `DK_VP_LOG=1` for its runs.
- **SDK patch 12 (`sdk-patches/12-quiet-repeated-gpu-errors.patch`):** each of the two GPU errors is logged in full the
  first 20 times, then once per 10,000 with the running count (`seen N times`). The failing draws still fail exactly as
  before: only the message is thinned. Why they fail is still open (board row).
- Built `[compile-verified 2026-11-05]`: exe b4eb808a71de, rexgpu-xenos.dll 8b6906b2aa89; installed in `build-home-xr`
  with the old pair in `_backup-2026-11-05-b010-before-quiet-log`; builds archive
  `motion-controlled hands/v0.1.0-b011`.

Before the run (below), the expectation was: the log at a small fraction of its size,
and the `[XR] frames=... wait_ms_mean=` lines staying in the log for the whole run: that is the number the frame-rate
row waits for.

## Run, 15:05-15:10 (pretend headset, VR Bench drove it to gameplay, Claude alone)

`[measured 2026-11-05, simulator, n=1]`, b011, `dk_sim_drive.py` (START, then A every 4.5 s, then 3.5 minutes in play):

| | 2026-11-04 (b010) | 2026-11-05 (b011) |
| --- | --- | --- |
| log | 283,025 lines, 43 MB in 4 min, rotating every 20-30 s | **1,777 lines, 260 KB in 5 min 20 s, one file, no rotation** |
| `[VP] PERSP` | 95,440 | 0 |
| failed draws / empty resolves | 93,016 each, one line each | about 90,000 each, 29 lines and 3 lines |
| `[XR] frames=` lines | rotated away | every one kept: 24,900 frames in 318 s (about 78 a second), wait_ms_mean 1.2 |

New fact from the counts: nearly all the failed draws and empty resolves happen BEFORE gameplay (90,000 by 15:06:16, the
title and the loading; under 10,000 more in 3.5 minutes of play) `[measured 2026-11-05, n=1]`. So the flood was mostly
a front-end and loading flood, and in play the failures are rare.

Closing: the window's close message shut it down cleanly, but it took about 10 s in the VR build (KNOWN.md says about 1 s,
measured on the flat build).
