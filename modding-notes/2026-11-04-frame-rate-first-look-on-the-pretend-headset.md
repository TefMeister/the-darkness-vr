# Frame rate: the first look, on the pretend headset (2026-11-04)

Home PC, Fable, 17:50-18:00, the OpenXR Simulator (no headset), the hands build (`dk_vr_sim_hands.ps1` = the headset
hands script with `XR_RUNTIME_JSON` pointed at the simulator; new, in `dev-archive/build-scripts/`). Driven by VR Bench
(`dk_sim_drive.py`: the menu button held 1.2 s = START, then A every 4.5 s; the virtual XInput pad does NOT reach the VR
build, the bridge pad replaces it). The always-on collector sampled the run (Lanes, lanes-next); the game's own `[XR]`
lines gave the frame counters.

## Measured `[measured 2026-11-04, simulator, n=1 run each]`

| where | XR frames / s | graphics card | the program's CPU | wait for the runtime |
| --- | --- | --- | --- | --- |
| title screen | 90 (frame time 11.1 ms) | 96-97 % busy, 2775 MHz, 142 W | 3.3 cores | 4.7-5.8 ms a frame |
| gameplay (the save's tunnel) | 74-90 | 74-83 %, 116-120 W | 3.3-3.5 cores | 2.0-3.4 ms a frame |

- **The eye picture is the game's own size, 1280 x 720 per eye, whatever the headset asks for:** `SubmitPair` gets
  `sbs_width_ x sbs_height_` (the guest output) and makes the swapchains at that size (`xr_output.cpp` MakeChains);
  `resolution_scale = 1` in the toml. So a big headset does NOT make the game draw more pixels; the runtime upscales.
  Picture size is ruled out as the 20 fps cause `[inferred-static + measured]`.
- **The graphics card is nearly full even at 720p per eye and even on the title screen.** With vsync off the game draws
  as fast as it can; the card's 97 % at the title is the loop running free, not scene cost.
- **The log is flooded: 283,025 lines, 43 MB, in about 4 minutes (about 1,200 lines a second), written from the GPU
  thread**: 93,016 x `PM4_DRAW_INDX_2 ... Failed in backend`, 93,016 x `Resolve region is empty` (one pair per failed
  draw, about 400 draws a second that never happen), and **95,440 of our own `[VP] PERSP call=...` probe lines**
  (vr_viewport_probe.cpp logs every projection write). The log rotates at 5 MB every 20-30 s. This is the "controls
  laggy + the log flood" row, now with numbers; the probe lines are ours to switch off first.
- Virtual Desktop's runtime (the real headset) was not in this run; the simulator's `xrWaitFrame` answers in 2-5 ms.

## What it means

The headset's 20 fps is not pixels. The suspects left, in order: (1) **the runtime wait**: on Virtual Desktop each
`xrWaitFrame`/`xrEndFrame` round trip is wireless and encoded; the game calls `SubmitPair` on EVERY swap and blocks the
game loop in `xrWaitFrame`, so a slow runtime drags the whole game down to its pace, and with the eyes drawn on
alternate frames (`DK_STEREO_PERIOD=1`) a 40 ms wait gives about 20 pairs a second; (2) **the log flood** (1,200
lines a second of formatting and file writes on the render thread); (3) the 400 failed draws a second (also a picture
fault: something the game wants drawn is dropped).

## Next

1. **One headset run with the collector on and nothing else changed** (Tefa plays 5 minutes): the `[XR]
   wait_ms_mean` line in the log is the number that settles suspect (1). The collector keeps the logs now, so the
   flood cannot rotate the line away as it did with the 2026-10-30 run (those logs are gone).
2. **Switch the `[VP] PERSP` probe lines off** and rate-limit the two backend errors (a build; the flood row).
3. If the wait is the cause: **decouple the headset from the game loop**: submit the latest finished pair from its
   own thread, never block the swap on `xrWaitFrame` (the design row, `MODEL: FABLE`).

Tools fed: the collector learned `collect_dir.<project>` (a port's build folder beside its data) from this run; the
Darkness KNOWN page got the VR Bench route.
