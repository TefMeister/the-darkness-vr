# 2026-09-21 — the frame rate on the home PC

**Result: about 284 frames a second in the opening car scene** `[measured 2026-09-21, n=2]`.

Home PC (RTX): i5-14600K, RTX 5080. The build is the 2026-09-18 one in
`C:\NonSteam\the-darkness\build-local`, in a 1280×720 window with vsync off, so the dev PC's numbers
compare directly. The dev PC gets about 15 in the same scene.

| Scene | Frames a second | How it was counted |
| --- | --- | --- |
| Main menu (3D rock-hand background) | ~181 | game log, one run |
| Opening car scene, run 3 | 284.5 (lowest second 274) | game log, 79 s |
| Opening car scene, run 6 | 284.3 (lowest second 272) | game log, 80 s |

**What it means:** stereo made by alternating frames (one eye per frame) gives about 142 pairs a second.
So the fallback design is not needed on this machine.

## How it was counted

`dev-archive/build-scripts/count_frames.py` reads the `[VP] PERSP` lines our build writes. In a 3D scene
two different projection matrices each appear once per frame, so the rate they share is the frame rate.
The script refuses to give a number unless two different matrices agree within 5 %.

`dev-archive/build-scripts/framerate_run.py` drives the game (virtual Xbox pad) and takes a timestamped
screenshot after every press, so each counted stretch can be matched to what was on screen.

## Routes that did NOT work (so nobody retries them)

- **`claude-memory/tools/measure-frame-rate.py`** (counts picture changes by copying the window from the
  screen) saw a *frozen* picture on this D3D12 window. Every sample was identical, even during the intro
  film `[measured 2026-09-21]`. Only `PrintWindow` sees the live picture, and it is too slow to count
  with.
- **The SDK's F3 debug overlay** ("Guest: N FPS") opens but stays empty, in our builds and in the
  Condemned 2 release. Its frame-stats feed is never connected in these apps `[measured 2026-09-21]`.
- **Tracy**: `TracyClient.dll` ships beside the game, but the game never opens Tracy's port, so the
  profiler is compiled out `[measured 2026-09-21]`.
- **NVIDIA FrameView's `PresentMon_x64.exe`** exits with code 1 and no message, even run as
  administrator. The official **GameTechDev/PresentMon 2.6.0** release works when elevated, and without
  elevation it at least says what it needs.

## Menu route on the home PC (no save, no profile)

Title at ~45 s → START → save-notice **OK** (A) → "No Gamer Profile… Continue?" with **NO** preselected →
**DOWN**, then A → main menu with **NEW GAME** on top (no CONTINUE without a save) → A → car scene by
~120 s. As a command:
`framerate_run.py --first-press 50 --seq "START A DOWN A WAIT A WAIT A WAIT A WAIT" --step 5`.
Pressing DOWN too early (while the save notice is up) does nothing, and A then picks NO and drops
back to the title.
