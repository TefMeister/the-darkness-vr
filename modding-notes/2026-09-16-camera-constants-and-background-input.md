# 2026-09-16 — Camera constants, a stale plugin, and why the menu route failed

`/lm`, dev PC `DESKTOP-V8GTSIR`, Claude driving, Tefa at work. Job from the board: **find which vertex
shader constants hold The Darkness's camera matrix, by measurement.**

## What was established

### 1. The camera constants are identified — statically, two independent ways
**c0–c3 = object × camera × screen matrix (clip space); c4–c6 = object rotation into view space; c7 =
object translation.** `[verified-live 2026-09-16 — read from the engine file]` + `[verified-numerically 2026-09-16, n=420]`

- The engine's own shader template, `System/Gl/VP.xrg` lines 58–60, labels them exactly that, and lines
  1070–1074 build the position with `ADD R4, R8, c[7]` then `DP4 oPos, c[0..3], R4`. Checked by hand.
- The reader decoded all 420 real vertex shaders in `System/Xenon/ProgramCache.xpc`: every one positions
  geometry from `dp4` against c0–c3 after adding c7.

### 2. A frame-end snapshot of the constants would have been worthless — caught before it was built
The constants live in one persistent array that is overwritten for **every object drawn**. At the swap,
c0–c3 holds only the last draw — probably a HUD or post pass. Sampling has to happen **per draw**, in
`D3D12CommandProcessor::IssueDraw`, just after the vertex shader is fetched. `[inferred-static — reader]`

### 3. The installed graphics plugin had been stale all day
Pre-flight's deployed check found `rexgpu-xenos.dll` did not match any current build. The game build writes
the exe and `rexruntime.dll` into the game's own `out/build` folder but writes **`rexgpu-xenos.dll` into the
shared SDK output folder**, and the deploy step only looked in the first. **Every live result earlier on
2026-09-16 — the first 3D render and the look-stick test — ran on that older plugin.** Now fixed, stamped with
`deployed.sh`, and recorded as a hazard in the control profile. Condemned 2 builds into the same shared
folder, so building one project overwrites the other's plugin. `[verified-live 2026-09-16]`

### 4. The menu route to gameplay depends on the game window having focus
A diagnostic run with a screenshot and focus reading every 5 s: **Microsoft Edge held focus for all 135 s, the
game never did**, memory never rose past ~680 MB (a level needs ~1.1 GB), and the idle attract trailer played.
In every run that reached gameplay, the game had focus. That matches SDL's default of ignoring game
controllers while the window is unfocused. `[verified-live 2026-09-16, n=1 failing, n=3 succeeding — correlational]`

### 5. The per-draw recording works
A new diagnostic setting, `gpu_dump_draw_constants`, writes c0–c7 for every draw. One run produced
**1,064,220 draw records over 140 s** while the pattern script pushed the stick right, left, up and down with
idle gaps, and the game was in gameplay (screenshot: hands, "Use to look around"). `[verified-live 2026-09-16, n=1]`

## What is NOT established — read before building on any of this

- ❌ **No live proof yet that c4–c6 moves with the stick.** Two reductions of the recording, both inconclusive:
  1. **Grouping by the recording's frame field was invalid.** That field is `CommandProcessor::counter()`,
     which increments on every swap **and every vblank** (`GraphicsSystem::MarkVblank`), so it ran at ~136
     per second and split each real frame into many tiny groups. The "most common c4–c6 per group" then just
     picked whichever object repeated. Result: flat. `[verified-static 2026-09-16]`
  2. **Averaging per 100 ms slice** showed a *directional hint* — in about five independent components a right
     turn and a left turn drove the value in opposite directions — but the change during stick moves was only
     ~1.2× the change during idle, because the car and scene move on their own. **This method also failed its
     own synthetic self-test**, so it cannot be relied on. `[hypothesis]`
- ❌ **The background-input fix is built but unproven.** `input_background_events` (sets SDL's
  `JOYSTICK_ALLOW_BACKGROUND_EVENTS` before the gamepad subsystem starts) is compiled in and switched on, but in
  the one run that used it **the game took focus by itself**, so it tested nothing about background input.
- ⚠️ The look-stick result from earlier in the day stands, but it was measured on the **stale** plugin.

## How to settle the constants properly next time
**Record which vertex shader each draw used** (`vertex_shader->ucode_data_hash()`, as the reader suggested)
alongside c0–c7. Then pick one static scenery object by shader and follow **its own** c4–c6 through the stick
pattern. That removes the object-mix noise that sank both reductions above, and needs one rebuild and one run.

## Tools added (`dev-archive/build-scripts/`)
- `camtrace.py` — launches, reaches gameplay via the pad, runs a right/left/up/down pattern with idle gaps,
  logs epoch-ms timings and focus per event, screenshots the pattern start.
- `camanalyse.py` — ranks constants by movement during stick windows versus idle, with a signed-rate
  direction test. **Self-tested on synthetic data with a known answer.**
- `camreduce.py` (modal per group — valid method, but needs real frame boundaries) and `camreduce_time.py`
  (time-slice mean — **failed its self-test**, kept for the record).
- `looktest.py` — the burst-capture look test, verdict measured on the lower half of the frame.

## SDK changes (`dev-archive/sdk-patches/`)
- `04-input-background-events.patch` — off by default; likely worth offering upstream once proven.
- `05-gpu-dump-draw-constants.patch` — diagnostic, off by default (empty path).
