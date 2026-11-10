# Where the card's time goes in one frame, and b025: 114 game frames a second in the alley (2026-11-10 evening, Fable, home PC, simulator)

Tefa switched on the NVIDIA GPU performance counters; this is the board row "WHERE THE CARD'S TIME GOES IN ONE FRAME".
Nobody in a headset. Alley, drawn 2 x 3 (2560 x 2160), `rov`.

## What changed for the player

**The per-eye brightness log is off by default: alley 73 -> 81-83 game frames a second (36 -> 41 eye pairs)**
`[measured 2026-11-10, simulator, n=2: test copy 81.4, installed b024 82-83; baseline 72.7 and 73.0 the same hour]`.
Patch 13's log (2026-11-06) copied BOTH full eye pictures (2 x 22 MB at 2 x 3) into read-back memory every pair and
read them on the processor: in the trace about 10 % of the card's time for the pair, all on the copy engine, plus
~70 MB over PCIe. It was on unless `DK_XR_EYE_LIGHT=0`.

- Now: `$env:DK_XR_EYE_LIGHT = "0"` in every start script of the installed b024 folder (no file of the build itself
  changed; `deployed.sh check` still OK), and the code default is flipped for the next build (`xr_output.cpp`: on only
  with `DK_XR_EYE_LIGHT=1`). Not worn.

## How the trace was taken (works unattended)

- Nsight GPU Trace (`ngfx_gputrace_launched`: the exe, the start script's settings as `env_pairs`, `start_after_ms`
  115000, `limit_to_frames` 4, `auto_export`, the output folder made first), and at the same moment
  `DK_DRIVE_NO_LAUNCH=1 python dk_sim_drive.py x 5` for the presses (new switch: presses only). VR Bench switched on for
  that exe first. Nsight ends the game itself when the trace is written.
- The first trace had whole-frame numbers only: the port named nothing. The SDK had marker plumbing that nothing
  called; **sdk-patch 16** (`16-gpu-pass-markers.patch`, written by the session's reader, `git apply` clean, built)
  names every pass, resolve, texture load, memory upload and our swap steps when `DK_GPU_MARKERS=1` (off: one test per
  call site). `build-scripts/gputrace_passes.py <BASE_UNLOCKED folder> [--seq]` adds the export up by kind of pass.
- The labelled graphics library sits only in the test copy `C:/NonSteam/the-darkness/build-test-gpu` (a copy of b024
  with its own link to the game data). The installed b024 has the unlabelled one.
- `build-scripts/dk_try.py <test folder> <label> [toml key=value ...] [--env N=V]`: one unattended try (drive, measure
  from the game's log, card load, close by the window, a line in `D:/the-darkness-build/gputrace/tries.tsv`). Refuses a
  folder without "test" in its name.
- `home_build_sdk.bat` no longer builds: its own tree's precompiled headers are from the 10-25 compiler. The
  graphics library is built in the GAME's tree: `cmake --build out\build\win-amd64-release --target rexgpu-xenos` in
  `D:\the-darkness-build\src\DarknessRecomp` (1 minute); the result lands in `rexglue-sdk\out\win-amd64\`.

## One eye frame on the card, `rov`, 2 x 3 `[measured 2026-11-10, simulator, n=1 labelled trace of one pair, n=1 unlabelled]`

2,731 draws, 430,000 triangles, about 155 million pixels rasterised onto a 5.5-million-pixel picture (28 layers),
120-136 million pixel-shader threads, **no pixel goes through the card's own depth test** (`rov` does depth and stencil
in the shader). Shader cores at about 20 % of their peak throughput; 37 % of the time they hold work that waits.

| Share of the pair's card time | What |
| --- | --- |
| 43 % | depth-only passes at full size (the pre-pass and the stencil shadow volumes): 2,268 draws, 139 M pixels a pair |
| 28 % + 8 % | the lit colour passes (two formats of the same surface at EDRAM 720) |
| 11 % | our headset hand-off, of which 10 was the brightness log's copy (now off) |
| ~10 % | time inside ~560 small memory uploads an eye (4-8 KB each, between draws) |
| 5 % | three "640 wide, 4 samples" clears an eye (the console library's fast clear) |
| under 5 % together | resolves, texture loads from resolves, gamma, the side-by-side copies |

- **The pre-pass alone (979 draws, 12.7 M pixels = 2.3 layers) is 10.5 % an eye, about 3 ms**, and under `rtv`, where
  it has no pixel shader at all, still about 2 ms. So about **2-3 microseconds of card time per draw whatever the
  path**; times 2,700 draws that is the ~7 ms that does not shrink with the picture size (b024's 8 / 11 / 13 ms at
  1 x 1 / 2 x 2 / 2 x 3 fit "7 ms + 1 ms per 1 x 1 of pixels") `[hypothesis on measured]`.
- The shadow volumes are cheap per pixel (one range: 21 draws, 29 M pixels, 3.2 %).

## Why the other method (`rtv`) is slower, seen `[measured, n=1 trace]`

`rtv` 59.0 and, with `gamma_render_target_as_unorm16 = false` + `depth_float24_convert_in_pixel_shader = true`, 60.1,
against 73 (`[measured, n=1 each]`). In its trace the pixel count explodes: 832 M + 1,507 M + 769 M a pair against
139 M + 110 M, in ranges with 0-4 draws carrying 35 M pixels (6.4 whole pictures) and colour ranges of ~78 draws
carrying 92-119 M: the port redraws whole surfaces each time the game changes how it looks at the same console video
memory (the reader's static read: nine passes per depth hand-over on this card). The game does that dozens of times
an eye (depth-only <-> colour, the 640 x 4-sample clears, two colour formats at one address).
**With those hand-overs gone, `rtv` gives the card's own depth and stencil rejection to 28 layers of overdraw: done
the same evening, b025, below.**

## b025: the console library's fast clear done plainly, and `rtv` (the same evening)

The reader's third report (`dev-archive/recon/2026-11-10-gpu-trace/2026-11-10-reader-rtv-transfer-free-and-per-draw.md`)
named the one switch that costs: `sub_828621D0`, the console graphics library's fast clear (three call sites, all in
`sub_82862420`). It clears the 80 x 16 aligned middle of the rectangle as a 4-sample surface of half the pitch (the
same video memory seen another way) and the edge strips plainly. Under `rtv` each such clear made the port hand the
depth surface over and back: a stencil clear, one depth draw and eight stencil-bit draws each way on this card. The
other switches seen in the trace are free (the two colour formats at EDRAM 720 share one host surface; colour writes
off and on with the same depth is no change) `[inferred-static 2026-11-10]`.

- **The hook** (`port-project/src/vr_plain_clear.cpp`, `REX_HOOK_RAW(sub_828621D0)`, on unless `DK_VR_PLAIN_CLEAR=0`):
  always does what the routine's own plain branch (`loc_828623D8`) does: one clear of the whole rectangle through
  `sub_82861EE0` with the surface as it is. Arguments checked against the generated code (r7 is untouched on that
  branch; the three caller-stack words at sp+84, +92, +108 are passed on as the original passes them).
- **Measured, alley, 2 x 3, simulator** `[measured 2026-11-10, n=1 each unless said]`:

| | game frames a second | eye pairs |
| --- | --- | --- |
| b024 as it was this afternoon (`rov`, brightness log on) | 73 | 36 |
| b024, brightness log off (`rov`) | 81-83 | 41 |
| b025 exe, `rov` | 78.6, 79.0 | 39 |
| b025 exe, `rtv` (test copy) | 113.8, 113.8 | 57 |
| **b025 as installed (`rtv`), 150 s in the alley** | **112-114, median 114** | **57** |

- **The picture**: the alley under `rtv` and under `rov`, one still each from the simulator's window, look the same to
  the eye (lamp, wall shadows, the graffiti, the body, both arms) `[verified-live 2026-11-10, simulator, n=1]`. Stills
  kept outside the repo in `D:/the-darkness-build/gputrace/shots/`.
- **b025** = `C:/NonSteam/the-darkness/build-home-xr-b025`: the new exe (a0813f287a3d), the graphics library with
  sdk-patch 16 (labels off unless `DK_GPU_MARKERS=1`) and the brightness log off by default, the runtime library built
  with it, the toml with `render_target_path_d3d12 = "rtv"`, the b024 start scripts. b024 stays beside it untouched.
  The Desktop icon starts the highest number, so it starts b025. Stamped with `deployed.sh record`.
- **Back to the old method**: `render_target_path_d3d12 = "rov"` in b025's toml; or start b024.
- **Builds store**: saved as `headset/v0.1.0-b025` at 22:1x, after the dev PC's all-clear and the move of the home PC's builds copy onto the cleaned history (b024's save checked: still there).

### NOT established for b025

- Only the alley, the menus and the films on the way to it were seen under `rtv`, on the simulator. Other levels,
  the darkness powers, water, glass, the pause menu: not seen. `rtv` is the port's less exact method (colour formats,
  blending and depth precision are the host's), so look for banding in dark gradients, wrong blending and missing
  decals.
- Whether `640 4x` ranges are gone from a labelled trace was not checked (the speed says the hand-overs are); two
  other places carry the same "set 4 samples" instruction (`sub_829E0D68`, `sub_829E2B20`, reader).
- With the card no longer the only limit (97.5 % against 99), what limits b025 was not measured.

## Tried and of no use `[measured 2026-11-10, simulator, n=1 each]`

`anisotropic_override = -1` (the game's own texture filtering): 73.1 against 72.7, nothing. `rtv` with the two
settings above: 60.1 against 59.0, nothing. No setting touches `rov`'s per-pixel work (reader, static).

## NOT established

- Nothing here is worn. The simulator's own cost per pair is in every number.
- "2-3 microseconds a draw" is one pass's time divided by its draws, not a per-draw timing; what inside a draw costs
  it (state changes, the interleaved uploads) is not measured. Nsight's "time every action" was not used.
- Whether the 28 layers are mostly shadow volumes was read from names and pass shapes, not switched off and measured.
- The labelled build's frame (30.8 ms a pair in the trace) is slower than an untraced one; shares, not milliseconds,
  are the result.
