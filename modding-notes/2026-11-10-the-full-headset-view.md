# The full headset view (2026-11-10, b018, Fable, home PC, OpenXR Simulator + VR Bench + the bridge)

Tefa 2026-11-07: *"i'm still looking through the 2d window at a depth gameplay in it"*.

## Why it was a window

The player view is authored as 70 engine degrees: half-width tangents 0.934 x 0.525, an 86 x 55 degree strip. The
headset was handed exactly that strip (`dk_xr_eye_fov`), correct in geometry but a letterbox in front of the face. A
Quest-3-like eye sees about 1.38 x 1.39 (the simulator's profile: left 1.376, right 1.376, up 0.965, down 1.390 both
eyes together). `[measured 2026-11-10]`

## Where the view is widened, and why not in the matrix

In the game's own projection builder `sub_8275EEF8` (new file `vr_wide_view.cpp`), by changing the viewport's own
numbers before it runs: FOV at +260, the two zoom numbers at +224 (x) and +228 (y), "rebuild" byte +336, kind byte
+256 (0 = perspective by FOV). The matrix at RC+17088 is the wrong place: culling, the per-light screen rectangles and
the lighting's eye position are worked out host-side from the viewport (dossier, 2026-09-18).

What the builder does with them `[measured 2026-11-10, n=3 settings]`:
`tan_y = tan(fov / 2) / aspect(+264)`, `tan_x = tan_y * width / height` (pixels stay square whatever the aspect says),
and the focal lengths are multiplied by +224 / +228. Checks: 70 / 1.3333 -> 0.9336 x 0.5252 (the game's own);
110 / 1.2 -> 2.1158 x 1.1901; 123.30 / 1.3333 with +224 = 1.7955 -> 1.3764 x 1.3901.

## What b018 does

- `DK_VIEW_AUTO=1` (bridge `view.auto`): fits the view to the headset by itself from `rex_xr_headset_tangents`:
  `fov = 2 atan(tan_y * aspect)`, `zoom x = tan_y * (w/h) / tan_x`. Both eyes get the same symmetric view (the larger
  side each way); `dk_xr_eye_fov` reports the tangents read back from the built matrix, so the headset shows the
  picture at the angles it was drawn with.
- Only in a level: the camera build (`sub_823F9B00`) must have run in the last half second; otherwise each viewport
  gets its own numbers back, so the menus and films keep the old framed look instead of wrapping round the face
  (`view.level_only`). `[verified-live 2026-11-10, simulator: the save notice at 0.934 x 0.525, the alley at 1.376 x 1.390]`
- Sharper: `draw_resolution_scale_x = 2`, `_y = 3` in `darknessrecomp.toml` (2560 x 2160 for the wide picture; the
  1280 x 720 one was 259 pixels per tangent up-down).
- Dials, live through the bridge: `view.fov`, `view.aspect`, `view.zx`, `view.zy`, `view.auto`, `view.level_only`;
  read-outs `view.tan_x`, `view.tan_y`, `view.game_fov`.

## Seen on the simulator `[verified-live 2026-11-10, n=4 runs]`

The alley after CONTINUE fills both eyes edge to edge; the street lamp's light, the shadows and the scenery are whole
to the edges (no cut lights, no missing objects in this scene). Frame rate in that alley, game frames a second from
the bridge: 50 with the game's own view, 46 wide, 40-43 wide + 2x3; the last run read 46 eye pairs a second at the
headset side (`dk_fps.py`). **Not worn.**

## Left open

- The HUD prompt ("Press LB ...") is not a flat projection through apply-viewport or the pre-draw commit: both shrink
  hooks counted 0 (`view.flat_count`, `view.flat_commit`), so `view.flat_x / flat_y` do nothing yet. The prompt sits
  in the middle and reads fine; where it is drawn from is not found.
- The pause menu inside a level: not seen (the robot's press did not open it).
- Edge popping and cut lights were looked for in one scene only.
- Hands too small / the world's scale: untouched by this (the eye distance, `DK_CAM_EYE`).
- The compiler on the home PC was updated since b017: every precompiled header had to be deleted and the port rebuilt
  whole (`error: PCH file ... built from a different branch`).
