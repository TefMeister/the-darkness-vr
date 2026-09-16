# The Darkness: where the projection and the camera are kept apart before c0-c7

Date: 2026-09-16. Author: PD reader for the `/lm` session. Static only: nothing was launched, and nothing in
`build-local/` or `game-files/` was changed. Follows `2026-09-16-reader-camera-constant-slots.md`.

Method. I unpacked `game-files/default.xex` into a scratch image (with `unpack_xex` from
`src/DarknessRecomp/tools/find_missing_functions.py`). Then I found code references to the engine's `.rdata`
constants and walked the PPC disassembly comments inside `generated/default/darknessrecomp_recomp.*.cpp`. The
scripts were scratch and are not kept. All addresses below are guest addresses; the functions are named
`sub_XXXXXXXX` in the generated code.

## Short answer

- **The projection is a separate matrix, and the whole game has one place that combines it with each object's
  matrix.** The camera is **not** separate at this level: the renderer only ever receives each object's
  already-combined object-to-camera matrix.
- **A per-eye offset can still be applied at one site**, by replacing the projection P with `T(e)·P` (T(e) is a
  sideways shift in camera space). I checked this numerically against an emulation of the combining function; see
  §3.

## 1. The functions that write c0-c7

The render context is a global singleton, `RC = 0x82A69B00`. The D3D device pointer is at `[RC+15748]`
(`0x82A73D84`). The shadow copy of vertex float constant `c[i]` is at `device + 1920 + 16*i`. That layout is
`[inferred-static]` and fits three independent facts:
- `sub_82248C80` writes {0,1,0.5,765.0003},{510.0002,255.0001,0.05,1/256} to `device+2048`. That is c8/c9, the
  "static constants" in `VP.xrg`.
- `sub_8224A2E8` writes texture matrices from `device+2112` with register counter 12, i.e. c12.
- `sub_8224A0A8` uses register 76 at `device+3136`.

| function | role | evidence |
| --- | --- | --- |
| `sub_82248A78` | **writes c0-c7**, the only writer of `device+1920..2047` found by a scan of every generated function | `addi r11,r11,1920` then 8× `stvx128` |
| `sub_82248C80` | writes c8/c9 and other per-draw state | above |
| `sub_82249488` | pre-draw commit: calls `sub_82248A78` only when `[RC+8228] & 3` (matrix-dirty bits) | direct `bl 0x82248a78` |
| callers of `sub_82249488` | the draw functions `sub_8223FA80`, `sub_82247CD8`, `sub_8225D960`, `sub_8225DD40`, `sub_8225DE78`, `sub_8225E218`, `sub_8225E2F0`, `sub_8225E3C8` | direct `bl` |

## 2. The matrices that go in, and what comes out

**Inputs to `sub_82248A78`** `[inferred-static]`:
- **M**, the current object's model-to-view matrix. It is the top of the render context's matrix stack:
  `[RC+8224] + [RC+8232]*656 + 16`, as 4 rows × 4 floats, row-vector convention with the translation in row 3.
  - Written by `sub_82236A80` (Matrix_Set for the current mode, `[RC+8236]`), which also sets dirty bits in
    `[RC+8228]`.
  - Written by `sub_82763D50` (set model and texture matrices).
  - Identity is set by `sub_82763C88`; push is `sub_82236960`.
- **P**, the projection, at **`RC+17088..17151` (`0x82A6DCC0`)**, 4 rows.

**Outputs.** These are `[verified-numerically 2026-09-16, n=50 random M/P]` against a Python emulation of the
function's disassembly, which follows the SDK's own translation of each AltiVec op:
- `c0..c3` = transpose of (M with its translation zeroed) × P. Error 9e-16.
- `c7.xyz` = t · R⁻¹, where t = M's translation and R = its 3x3; `c7.w = 0`. Error 8e-6.
- **`c4..c6` = the first three columns of M, including the translation in `.w`**. That is the full
  model-to-view 3x4, not only a rotation. Error 0.
- The shader path (`add c7; dp4 c0..c3`, and `dp4 c4..c6`) reproduces `v·M·P` and `v·M` to 7e-8.

**Where P comes from:**
- `sub_82249580` (the Xenon "apply viewport" step) takes the current `CRC_Viewport` (`[[RC+132]+24] + [RC+136]*416`).
  - It calls `sub_8275EEF8` to rebuild that viewport's matrix.
  - It copies viewport `+0..63` to `RC+17088` and sets dirty bit `[RC+8228] |= 1`.
  - It then scales column 0 by 2/width and column 1 by 2/height (pixel units to device coordinates).
  - It also copies the front and back planes (`+304`/`+300`) to `RC+12568/12572`.
- **`sub_8275EEF8` = `CRC_Viewport` update** (lazy; dirty byte at viewport `+336`):
  - **FOV in degrees at viewport `+260`**, clamped to 0.1..179.
  - The half-angle is `fov·π/360` and goes through a trig helper, `sub_82894FF0`.
  - Aspect is at `+264`.
  - It writes a D3D-style row-vector perspective matrix at `+0..63`: `+0` x-scale, `+20` −y-scale, `+40`/`+56` depth
    terms from the planes, `+44 = 1`. An orthographic branch writes `+44 = 0`, `+60 = 1`.
  - It also builds frustum planes at `+76..+220` (used for culling; they do not come from `RC+17088`).
  - `[inferred-static]`
- **Callers of `sub_82249580`:** `sub_8223E268`, `sub_8223EB80`, `sub_8223EDB8`, `sub_8223FA80`,
  `sub_82254960` (×2), `sub_8259E050`, `sub_825A24D0`, `sub_82762790`.
- **Where the FOV number itself is set is not pinned down.** The strings `vp_fov`/`vp_zoom`/`vp_aspectratio`/
  `vp_frontplane`/`vp_backplane` are registered as script/entity keys in `sub_82389D18`, which is probably a
  camera or viewport entity. `[hypothesis]`

**Where the camera is multiplied in:** upstream of the render context, when the engine builds each object's M
(object-to-world × world-to-camera). **Not located.** There are many callers and indirect dispatch; tracing it would
be long, and the P-side route below does not need it.

## 3. Once per frame or per object?

- **P:** once per viewport apply (`sub_82249580`), i.e. a few times per frame (main view plus any other passes).
  `[inferred-static]`
- **M, and the c0-c7 combination:** per object. `sub_82248A78` re-runs at every draw whose matrix or projection
  changed. `[inferred-static]`
- **Consequence for VR:** replacing P with `T(e)·P` (row-vector convention, `T` = identity with row 3 =
  `(e_x, e_y, e_z, 1)`) moves the eye in view space for **every** object. It is **one change per viewport apply**,
  at the end of `sub_82249580` after the 2/width and 2/height scaling. Alternatively it can be applied inside
  `sub_82248A78`.
  - Emulation check: feeding `T(e)·P` gives exactly `P((v·M) + e)`, error 2e-6.
    `[verified-numerically 2026-09-16, n=1 case, plus 50 random cases for the base path]`
  - Only apply it to perspective viewports: `RC+17132 == 1.0` and `RC+17148 == 0`. GUI and orthographic
    viewports should be left alone.
  - Culling uses the viewport's own planes, so objects right at the screen edge may cull slightly early for one
    eye. `[hypothesis]`
  - User clip planes (`sub_82249250`) use the same P, so they stay consistent.

## 4. Hypothesis (a) from the previous note, updated

- `c4..c6` is **not only a rotation**. It is the full model-to-view 3x4 (rotation × scale in xyz, translation in
  w). `[verified-numerically]`, via the emulation.
- For geometry whose object matrix is the identity, `c4..c6` would therefore be the **whole camera view matrix**,
  position included. Whether static world geometry is drawn with an identity object matrix is still `[hypothesis]`.
  One live per-draw capture would settle it: many draws in a frame sharing the same `c4..c6`, which changes with
  the stick.

## What a live check would add

Not needed to act on §3. As a sanity check, log `RC+17088..17151` and `RC+17132` at each `sub_82249580` return
(expect a small number of values per frame), and confirm that the perspective one changes when FOV-affecting events
happen and not when the stick turns.
