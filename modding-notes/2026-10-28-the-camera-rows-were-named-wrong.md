# 2026-10-28 — the camera rows were named wrong (home PC, `/lm`, Fable, OpenXR simulator, no headset)

## The question
Tefa's first headset run (2026-10-25): "when turning my head the camera pans around in a curve, stereo is off but not
just left and right but also up and down". Why?

## How it was tested
The 2026-10-25 headset build (`build-home-xr`, exe a8e7c97644f1) run on the **OpenXR simulator** instead of the Quest
(`XR_RUNTIME_JSON` to the simulator, the H stereo set + `DK_XR=1 DK_XR_HEAD=1`), driven to the opening car scene by the
bridge (`staging/the-darkness-vr/bridge-2026-10-23/run_pair_test_bridge.py`'s route, 85 s). The scene stands still there,
so one picture per head pose is a fair comparison. The simulator's head was set to four poses and the engine's own
side-by-side frame taken through the bridge each time (pictures: `C:/NonSteam/the-darkness/screens/xr-sim-2026-10-28/`,
game content, this PC only):

| head pose set | what the picture did | so the rotation was applied about the camera's |
| --- | --- | --- |
| straight (identity) | baseline | — |
| yaw 30° (turn) | the picture ROLLED about 30° in its own plane | **forward** axis |
| pitch 20° (nod up) | the picture PANNED sideways (content moved right, no roll) | **up** axis |
| roll 20° (tilt) | the picture PANNED up and down | **right/left** axis |

`[verified-live 2026-10-28, n=1 each, by eye]`.

## What it means
`sub_823F9B00`'s view buffer is laid out **+0 right, +16 up, +32 forward, +48 position** — not "+0 forward, +16 right,
+32 up" as every file said since 2026-09-18. The head hook handed `TurnCamera` the rows in the wrong order, so a yaw
turned about the forward row (a roll), a nod about the right row named "up"... every head movement landed one axis off.
That is "pans in a curve".

The same mistake explains the up-down stereo: the eye shift (`DK_CAM_EYE`) moved the camera along row +16, which is
**up**, so the two eyes were 2.5 units apart VERTICALLY and not at all sideways. The 2026-10-24 "two eyes look different
ways" pictures (hands up by the face in one half, low in the other) are what a vertical eye offset looks like
`[hypothesis 2026-10-28]`: consistent, not separately measured.

Why the 2026-09-18 "the row layout was a guess and it is right" was wrong: the one-shot log printed ONE unit vector and
a plausible position. A unit vector proves the row is an axis, not which axis. Its value that day, (−0.03, 0.95, −0.32),
reads naturally as an UP vector of a camera pitched 18.6° down at the hands in a Y-up world, which is exactly the scene.
Today's first build printed (0, 0, −1) for the same row.

(The dossier's free-camera reading, "+8880 forward, +8896 right", was read from `sub_823FABC0`'s inputs and may have the
input names swapped in the same way; not re-checked today.)

## The fix (this session)
`vr_viewport_probe.cpp`: named row offsets `kRowRight=0 kRowUp=16 kRowFwd=32 kPos=48`; `TurnCamera` gets the rows by
name; the eye shift goes along the right row; the one-shot log now prints all four rows. Rebuilt (exe 9287160cb840),
installed in `build-home-xr` (old exe kept in `_backup-2026-10-28-before-rows-fix/`).

## Second run, with the rows renamed (exe 9287160cb840)
The four rows at the first build: right=(0,−1,0), "up"=(0,0,−1), fwd=(1,0,0), pos=(−1992, −1108, −202).
| head pose | picture | verdict |
| --- | --- | --- |
| yaw 30° | panned sideways, content moved right, no roll | ✅ the right way |
| pitch 20° (nod up) | panned, content moved UP | ❌ the wrong way |
| roll 20° (right ear up) | turned 20° counter-clockwise (measured: the baseline rotated +20° matches, diff 14 vs 54) | ❌ the wrong way |
| the halves | near hand further right in the RIGHT half (best shift −36 px) | ❌ halves swapped: the left half was the right eye |

Turn right, nod and tilt both wrong: only a flipped UP axis does that (a mirror would flip two of the three, and an
up row pointing down leaves the turn alone, since the turn is about that very axis). **So the +16 row points screen-DOWN.**
And the first eye of a pair (sign +1, even serial, the left half) was shifted +e along right, which is the right eye's
place.

## Third run, both fixed (exe 9b65741779a7, installed in `build-home-xr`)
`TurnCamera` gets minus the +16 row (and minus is written back); the eye shift is `pos − sign·e·right`.
| head pose | picture | verdict |
| --- | --- | --- |
| yaw 30° | content moved right, no roll | ✅ |
| pitch 20° (nod up) | content moved DOWN (dy +160 px) | ✅ |
| roll 20° (right ear up) | turned 20° clockwise (baseline rotated −20° matches, diff 24 vs 61) | ✅ |
| the halves | near hand further right in the LEFT half (best shift +36 px) | ✅ left half = left eye |

`[verified-live 2026-10-28, n=1 run per pose, simulator, one scene]`. Every head axis now moves the picture the way a
headset needs, and the stereo pair is the right way round. Not yet seen in the real headset.

## What is NOT established
- Whether the picture at identity is the game's own cinematic camera (it was on its side in all three runs' first
  pictures; the first-build rows say the camera's down vector was horizontal, so the game itself had it on its side at
  that moment `[inferred 2026-10-28]`) or something of ours: a `DK_XR_HEAD=0` control at the same moment would settle it.
- The right scale of `DK_CAM_EYE` (1.25 units gives ~36 px of hand disparity at 1280 wide): untouched today.
- The lag: the SDK log says `sample_hits=0 sample_misses=8400`: the game exports no `dk_xr_pair_sample`, so the headset
  gets each picture with the pose of the moment it is SENT, not the pose it was DRAWN with. That is Condemned 2's
  `vr_head_pose.cpp` pair ring, not ported yet; the port is the next step for the lag.
- Nothing here has been seen in the real headset.

## Later the same session (Opus): the picture lagging the head, fixed
The SDK showed every pair with the NEWEST head pose (`sample_hits=0`): the game never told it which sample a pair was
drawn with, so the world swam behind a moving head. Ported Condemned 2's pair ring into `vr_xr_head.cpp`: the head is
sampled once per pair (pair = the game packet's sequence / (2 x `DK_STEREO_PERIOD`), the number the SDK pairs on; the
camera thread now carries `t_packet.game_seq`), both eyes use that sample, and `dk_xr_pair_sample` tells the SDK which
one it was. Exe a7667e7cea10, installed in `build-home-xr` (previous in `_backup-2026-10-28-before-pair-sample/`), stamped.
- On the simulator, in the car scene: **every headset frame matched its pair's sample, 300 of 300 per log line, two lines
  running** (18:07:33 and 18:07:44); the misses before that were all menus and films, where no 3D camera is built
  `[verified-live 2026-10-28, simulator, n=1 run]`.
- Head yaw 30° re-measured: the same picture change as the previous build (content dx −104, dy −160 px), so the turn
  is unharmed `[verified-live 2026-10-28, n=1]`.
- Not established: how it FEELS (that needs the headset), and the controls lag (a separate row).

## Later the same session: motion-controlled hands, first static look (Tefa's "go")
Tefa: *"actual body and IK and hands moving with motion controls, we have never done that."* Plan on the board (five
steps, cheapest first); step 1 (find the skeleton) started, findings in dossier §6e. Short form: the game already has a
hand IK with a switch (`DISABLE_HAND_IK` → flags word `this+1856` in `sub_821B22E0`), the skeleton classes are named,
weapons attach through `ATTACHPOINTS`, and the vertex shader skins from a matrix palette of three constants per bone
(`System/Gl/VP.xrg`). Not found yet: the IK solver, the palette writer, the `$MP` base. All `[inferred-static 2026-10-28]`.
Second pass on Fable: the constant shadow (`RC+1920`), the pre-draw program setup (`sub_82248C80`) and its eight-slot
matrix filler (`sub_8224A2E8`) read; the bone palette is NOT copied there, and no function copies a count × 48 bytes, so
the next step is a live dump at the pre-draw commit rather than more reading (dossier §6e, second pass).
⚠️ The RE tools in `dev-archive/tools/` still point at the dev PC's `E:` folder (`idx.GEN`); on this PC set
`idx.GEN = r"D:\the-darkness-build\src\DarknessRecomp\generated\default"` before use (done by hand today; worth a
settings line).
