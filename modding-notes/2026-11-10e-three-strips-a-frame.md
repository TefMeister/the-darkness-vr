# Why it is slow: every frame is drawn three times, in strips (2026-11-10, Fable, simulator; b022 = b021 + a report line)

Tefa 2026-11-07: *"the game still runs like crap in VR"*. Board: the log flood of `PM4_DRAW_INDX_2 ... Failed in backend`
and `Resolve region is empty` (about 3,000 a second in play), cause unknown; suspected of breaking the game's exposure.

## What the "failed" calls are `[measured 2026-11-10, simulator, n=3 runs]`

SDK patch 15 adds one line to the empty-resolve report: the rectangle's vertices, the window offset, the scissor, the
surface. Every one of them reads like these:

    vertices (0 0) (640 0) (640 184) -> rect y 0..184; window offset 0 -256 -> y -256..-72; scissor y 0..256; msaa 2
    vertices (640 0) (1280 0) (1280 360) -> rect y 0..360; window offset 0 -512 -> y -512..-152; scissor y 0..208; msaa 2

Window offsets 0 / -256 / -512 with scissors 256 / 256 / 208 high: **the game draws its 1280 x 720 picture in three
horizontal strips** (256 + 256 + 208 = 720), the Xbox 360's "predicated tiling": with 2x anti-aliasing the picture does
not fit the console's 10 MB of video memory in one go. The game's graphics library replays the whole frame's commands
once per strip; a copy whose rectangle lies in another strip comes out empty in this one. So:

- **the empty copies are harmless**: each copy is made in the strip it belongs to; the other two strips' turns are
  the "failures". They are not why the exposure misbehaved. `[disproved]` for that board hypothesis.
- **the cost is real**: every draw of every frame is issued three times (the port runs the game's own command stream as
  it is), and with one eye per frame that is six passes over the scene for one stereo pair.

## What would make it one pass

Without anti-aliasing 1280 x 720 fits (1280 x 720 x 8 bytes = 7.4 MB), and the game would draw each frame once. The
host does not need the game's anti-aliasing: b018 already draws 2 x 3 larger.

- **Tried: the game's own setting.** `R_ANTIALIAS=0` and `=1` in `EnvironmentXbox.cfg` (the game's disc root; the file
  is read, dossier 2026-09-18): still `msaa 2`, still three strips, the alley still about 40 game frames a second
  `[measured, n=1 each, one game running]`. The setting exists (startup `sub_820E4CD8` reads `R_ANTIALIAS`, default 4,
  into the option `VIDEO_DISPLAY_ANTIALIAS`; `sub_820E5730` turns the option into the console command
  `r_antialias(%d)`), but the 360 renderer does not follow it. `[inferred-static]` for the two functions.
- **Next (static, no game):** find where the renderer picks the sample count. Leads: the other readers of the option's
  name (`sub_823ACC08`, `sub_820E3D98`); the debug text "TxtID %d, Slice %d, iMRT %d, ContinueTiling %d" at 0x8208BC58
  (the tiling code is beside whoever prints it); the render-target creation that passes 2 samples. Then force 1 sample
  there (a hook like the others) and count the strips again with patch 15's line: one strip = done. Expect the alley
  to go from ~40 game frames a second toward 100 `[hypothesis]`.

## A trap met on the way

`cp -r` of a build folder copies its `Assets` folder, and `Assets` is the whole game (6.7 GB, 499 files): b016 to b022
each carried a full copy. Tonight's five (b018-b022) are now links (junctions) to `game-files`, checked file-for-file
first (27 GB back). A settings file put in `game-files` is only seen by builds whose `Assets` is the link; b016 and
b017 still have their own copies. New build folders: make `Assets` a junction (`mklink /J`), never a copy.
