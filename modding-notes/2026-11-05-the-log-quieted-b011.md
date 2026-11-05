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

Not run yet. The next run of any kind (pretend headset or real) should show the log at a small fraction of its size,
and the `[XR] frames=... wait_ms_mean=` lines staying in the log for the whole run: that is the number the frame-rate
row waits for.
