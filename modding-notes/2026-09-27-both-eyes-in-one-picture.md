# Both eyes in one picture (2026-09-27)

**The game was not launched for this change, and nothing in it has been run.** It is built and
installed, off by default, waiting for one test run.

## Why

Every stereo measurement so far has needed to know which eye a screenshot caught, and a screenshot
taken on a timer cannot know: the game swaps about 140 times a second, the capture takes about 18 ms,
and the window route sometimes hands back the same picture twice (42% of back-to-back captures were
identical with the world moving, `[measured 2026-09-27, n=1]`). So the freeze could be seen engaging
in the log but never proved on screen.

## What was built

`dev-archive/sdk-patches/07-side-by-side-stereo-pair.patch`, in the SDK's D3D12 swap
(`D3D12CommandProcessor::IssueSwap`, "Seam A"). With `DK_SBS=1`:

- the picture handed to the window is twice as wide;
- this swap's image goes on one half, the previous swap's image on the other (even swaps on the
  left), so **one screenshot holds two consecutive frames**;
- two small textures keep the previous frame; they are made again if the size changes, and the old
  ones are released only after the GPU has finished with them.

With `DK_SBS` unset nothing changes: the doubled size and the copy only happen when it is on.
`[compile-verified 2026-09-27]` on the home PC (SDK v0.10.0 release + patches 01/04/05/06/07). Only
`rexgpu-xenos.dll` changed (the program and runtime are byte-identical to the proven home build),
which is the module this code lives in. The patch was checked to reproduce the built source exactly.

Installed separately at `C:\NonSteam\the-darkness\build-home-sbs\`; the proven `build-home\` is untouched.

## The test it enables

`dev-archive/build-scripts/sbs_pair_test.py ON|OFF <out dir>` drives to the opening car scene with
`DK_SBS=1`, `DK_STEREO_PERIOD=1`, `DK_CAM_EYE=0`, and measures how often the two halves of a picture
are identical. With the eye offset at zero the halves can differ only because the world moved.
Its measuring function was checked on made-up pictures (identical halves 0.0, a shifted half 10.0)
`[verified-numerically 2026-09-27, n=2]`.

Prediction, written before any run:
- **Freeze ON:** about half the pictures have identical halves (a pinned second eye beside its first).
- **Freeze OFF:** few or none while the car scene moves.
- **About the same both ways:** the freeze does not hold what reaches the screen. First check that
  the OFF run's halves differ at all, or the capture is stale and proves nothing.

## Not established

- That the doubled picture displays correctly (letterboxed 2:1 in the 16:9 window) - not run.
- Which SDK swap parity matches the game's left eye. The SDK counts its own swaps; the game flips
  the eye in its own hook. Irrelevant to the freeze test (the offset is zero), needed before a
  headset: compare the SDK's `[SBS]` log line with the game's `[VP] EYE=` lines on the first run.
- The Inspector watches the repo, not the SDK working tree on D:, so this code reached it only as a
  patch file, which it does not inspect.
