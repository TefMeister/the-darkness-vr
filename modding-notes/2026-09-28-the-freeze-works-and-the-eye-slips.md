# The freeze works, and the eye slips (2026-09-28)

`/lm` on the home PC, Claude driving, Tefa at work. The side-by-side build (`sdk-patches/07`,
`DK_SBS=1`) was run for the first time.

## In one paragraph

The side-by-side picture displays correctly (2:1, letterboxed). **The freeze does hold the world
still across a pair:** on the moving 3D main-menu background, with the eye offset at zero, no pair
had matching halves with the freeze off, and about half did with it on. At the real eye separation
the hands land about 42 px apart between the eyes (full resolution), which matches the 2026-09-18
numbers. **But a third to a half of the pairs show the same eye on both halves**, and the cause is
new: **the camera is built on a different thread from the one that ends frames, and the two are not
in step.** A frame can be drawn with no new camera (so it repeats the previous eye), or with two.

## What was measured

### 1. The measuring script had a bug (fixed)

`sbs_pair_test.py` cropped only the title bar. `winshot.grab` takes the whole window, including the
invisible ~8 px resize borders on the left and right, so the two halves were compared 8 px out of
line and no picture could ever score as identical. Found by looking at the first picture, and found
independently by the background reader. Fixed: it now crops to the window's client area
(`client_box`), and it skips 2 columns either side of the seam. Checked on a real picture (1.80) and
on the same picture with one half copied onto the other (0.0) `[verified-numerically 2026-09-28, n=2]`.
The two runs made before the fix (`off2`, `on1`) are void.

### 2. The freeze test (eye offset 0, eye flips every frame)

Both valid pairs of runs landed on the **main menu**, not the car: the first run of the day made a
save, so the menu route changed (see "Menu route" below). The menu background is a moving 3D scene,
so it serves.

| run | freeze | pictures with halves under 0.4 | median difference |
| --- | --- | --- | --- |
| off3 | OFF | 0 of 200 (lowest 0.35) | 0.64 |
| on3 | ON | 97 of 200 | 0.73 |
| off4 | OFF | 0 of 200 (lowest 0.69) | 1.24 |
| on4 | ON | 74 of 200 | 0.81 |

`[measured 2026-09-28, n=2 runs each way]`. ON splits cleanly into two groups: near-identical pairs
and pairs that differ more than OFF does. That is what a held clock predicts, because the next free
frame catches up two frames' worth of motion. **So the freeze reaches the screen.**

⚠️ The "identical" ON pairs are not exactly zero (0.02 to 0.3). A difference map shows fine grain
across the picture and the glowing embers at the edges. Something still moves between the eyes on a
clock we do not hold: a film-grain effect or a particle timer is likely `[hypothesis]`.

⚠️ **The car scene reached via CONTINUE is no use for this test.** It is still fading in and nothing
moves, so even OFF gives mostly identical pairs (118 of 200 under 0.05). It is fine for measuring
the eye separation, where stillness helps.

### 3. The real eye separation (`DK_CAM_EYE=1.25`)

In pairs that do show two eyes, the hands move 20 to 24 px between halves in the 640-wide window
halves, so **about 42 px at full resolution**. The 2026-09-18 run gave 133 px at offset 4, which
scales to 41.6 at 1.25 `[measured 2026-09-28, n=20 pictures]`. The upper part of the picture
(the car interior further away) shows no clean shift.

**But 14 of 20 pictures with the freeze on, and 30 of 60 with it off, had the same eye on both
halves** `[measured 2026-09-28]`. So the freeze is not the cause.

### 4. Why the eye slips: two threads, not in step

- **Lead tested and ruled out:** the reader found that the frame-end function (`sub_82867620`) can
  end a frame without calling VdSwap when `device+21508` is set. The probe now counts those and does
  not flip the eye on them. **Live count: 0 of ~38,700 frames** `[measured 2026-09-28, n=1 run]`.
  The guard stays in (harmless, and it will show if a later scene does it).
- **The cause:** the probe now logs which thread runs each hook and how many camera builds
  (`sub_823F9B00`) happen between two frame ends. **The camera is built on a different thread from
  the frame end** `[measured 2026-09-28, n=1]`. Over the last 600 frames in the car scene: no camera
  build 125 times, one build 350 times, two builds 125 times `[measured 2026-09-28, n=1]`.
- So the eye flips per rendered frame, but the offset only lands when the other thread next builds
  the camera. About one frame in five is drawn with the previous frame's camera, so it shows the
  previous eye. That matches the 30-50% same-eye pairs. The spread of hand shifts (about 11 px as
  well as 22 px) also fits: some pairs mix frames from different camera builds.

## What this means for the design

"One eye per guest frame" assumed one camera per frame. **It is not one-to-one here.** The eye
offset has to be tied to the frame the render thread actually draws. The options, not yet chosen:

1. **Lock the two threads one-to-one** (the camera thread waits for a frame end, and the frame end
   waits for a camera build). This is the simplest idea, but it could stall or deadlock the engine.
2. **Apply the offset on the render thread**, where it first reads the camera for a frame. That
   needs the handoff between the threads mapped; the reader is on it.
3. **Pick the eye on the camera thread** (flip per camera build) and hold the swap until a build has
   happened.

This is a design decision everything later builds on, so it is `MODEL: FABLE` work.

## Menu route (changed today)

With a save present there is no "No Gamer Profile" dialogue, and NEW GAME asks "Starting a new game
will delete all your saves" (cursor on NO). The route that works now: at ~52 s press **START, A**
(title, then save notice), then **A** on CONTINUE; the car scene is up ~25-40 s later
`[verified-live 2026-09-28, n=4]`. The old `START A DOWN A …` route now stops at that delete
question (safely, on NO). `sbs_pair_test.py` takes `DK_TEST_SEQ` to override the route.

## Installed now

`C:/NonSteam/the-darkness/build-home-sbs/darknessrecomp.exe` is the new probe build (skip guard,
thread and camera-build logging), sha256 `b3dbbaae4a53b7cf…` (stamped in `claude-memory/deployed/RTX/the-darkness-vr.tsv`). `rexgpu-xenos.dll` is patch 07 as
before. `build-home/` is untouched and still holds the previous program, byte-identical to what
`build-home-sbs` had this morning.

## Not established

- Which of the three designs above works, or whether any of them stalls the engine.
- What the leftover grain/ember difference in frozen pairs is.
- Which SDK swap parity is the left eye. There was no point checking while the eye slips.
- Whether the camera/frame ratio differs in real gameplay (only the car scene's opening was counted).
