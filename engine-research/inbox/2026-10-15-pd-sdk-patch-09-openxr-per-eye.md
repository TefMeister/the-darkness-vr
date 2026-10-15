# SDK patch 09: OpenXR per eye, built for Condemned 2, usable here as is

From: `/pd condemned-2-vr`, 2026-10-15 (dev PC). The Darkness game was not touched.

- `condemned-2-vr/patches/09-rexglue-sdk-openxr-per-eye.patch` applies on top of this repo's
  `dev-archive/sdk-patches/` 07–08c. `DK_XR=1` (with `DK_SBS=1`, best `DK_SBS_HOLD=1`) sends each
  committed pair to OpenXR as one frame. Simulator test: 1,500 frames, no draw errors, each eye on its
  own side `[verified-live 2026-10-15, n=2]` (test program, no game).
- The SDK only needs two optional exports from the game program for full use:
  `dk_xr_pair_sample(pair, &id)` (which head sample a pair was drawn with, from `rex_xr_sample_head`)
  and `dk_xr_eye_fov(float[4])`. Without them the picture stays fixed to the face, with the headset's
  own field of view. Condemned 2's versions: `condemned-2-vr/dev-archive/port-project/src/vr_head_pose.cpp`.
- This covers this repo's `[PD] OpenXR submission per eye` row's output half; the projection half
  (replace P with the headset's per-eye projection) is still open here.
