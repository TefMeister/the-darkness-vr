# 2026-10-25 — first run in the real headset (home PC)

## What was done
- Condemned 2's two SDK patches, `09-rexglue-sdk-openxr-per-eye` and `09b-...-headset-tangents` (condemned-2-vr/patches),
  applied to the home PC's rexglue-sdk (v0.10.0 + 04..08c); both applied cleanly `[compile-verified 2026-10-25]`.
- Game side, `port-project/src/vr_xr_head.cpp` (from Condemned 2's `vr_head_pose.cpp`): `DK_XR_HEAD=1` samples the head
  (`rex_xr_sample_head`) on the left eye's camera build and turns the camera rows (forward +0, right +16, up +32) in the
  `sub_823F9B00` hook before the eye shift; the right eye reuses the sample. `dk_xr_eye_fov` exports the player view's
  tangents 0.9336 x 0.5252 (P00 = 1.07111 at 16:9, dossier). `DK_XR_HEAD_MIRROR=1` flips the turn if it runs the wrong way.
- The compiler had moved to clang 23.1.4: stale precompiled headers deleted in the SDK and game builds, whole rebuild.
- Run folder `C:/NonSteam/the-darkness/build-home-xr/` (exe a8e7c97644f1, rexruntime 69f1390dc127, rexgpu-xenos
  e7517ca48fd3, openxr_loader 97a8e5f74fc2), started by `dk_vr_headset.ps1`. Archived in the private builds repo as
  `the-darkness-vr/headset/v0.1.0-b001` (the game program stays on the home PC, hash in the manifest).

## What was seen
- Tefa, Quest 3: "compared to condemned 2 it's all wrong, the controls are unresponsive and lag a lot, when turning my
  head the camera pans around in a curve, stereo is off but not just left and right but also up and down, but it's in VR
  and even though not playable, it's now playable broken in VR" `[verified-live 2026-10-25, n=1]`.
- Log: headset session focused, 42,300 headset frames, 0 draw errors, wait 3.7 ms; about 105 game frames a second,
  about 52 pairs `[measured 2026-10-25]`.

## What is NOT established
- Whether the head turn ran at all: the `[DKXR] head samples` lines rotated out of the log (20 x 5 MB in 13 minutes).
- Why the turn curves: wrong pivot or axis, or the picture turned twice (no `dk_xr_pair_sample` export yet) `[hypothesis]`.
- The up-down stereo error = the 2026-10-24 "eyes look different ways" fault, cause unknown.
- The lag: held pairs and the freeze add frames; the log flood (`PM4_DRAW_INDX_2 ... Failed in backend` and `Resolve region
  is empty`, about 345,000 each in 13 minutes, also in the 2026-10-24 flat build) is a suspect `[hypothesis]`.
