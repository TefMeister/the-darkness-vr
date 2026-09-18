# Per-eye lighting defect (red heads in one eye): what reads P, and the candidate cause

Author: reader (static only), 2026-09-18. Nothing here was run. Sources: generated C++ in
`E:\the-darkness\src\DarknessRecomp\generated\default\`, the unpacked `default.xex`, and the
plain-text shader templates in `game-files\System\Gl\`.

Refines: ENGINE-DOSSIER.md §6 "THE PER-EYE INJECTION POINT" (the P-shift is geometrically exact,
but this renderer makes camera-dependent decisions on the CPU that never see a shift hidden in P).

## Short answer

The eye offset lives only in the copy of P at `RC+17088`. **Only the vertex-constant writer and the
user-clip-plane path ever read that copy.** Everything else that needs the camera - light scissor
rectangles, frustum culling, the world-space eye position given to the lighting shaders - is computed
on the CPU from the **viewport object and the camera matrix, which are unshifted**. So geometry is
drawn from the shifted eye while per-light screen rectangles (and any other CPU camera test) are
computed for the centre eye. In the car interior the mismatch is 47-190 px, which is large enough to
cut a shadow or a light in the wrong place, differently for +e and -e. `[hypothesis]`

The robust fix is architectural: **put the eye translation into the camera matrix (upstream, at the
client view build), and keep P for the projection only** (FOV / asymmetric frustum). Then every CPU
decision sees the true eye. `[hypothesis]`

## (a) Every reader of RC+17088..17151

Exhaustive text scan of all generated functions for D-form displacements 17088-17151, `addi rX,rY,170xx/171xx`
and `li rX,170xx/171xx` (the index form used with `lvx128`/`stvx128`) `[measured 2026-09-18]`:

| function | access |
| --- | --- |
| `sub_82249580` | writes P (copies viewport+0..63, scales columns by 2/width, 2/height) |
| `sub_82248A78` | reads P, writes c0-c7 |
| `sub_82249250` | reads P, user clip planes |

No other function touches the block. The remaining hits for those numbers are `lis -32250/-32251` string
addresses, not RC offsets. Absolute addressing (`lis -32089` + negative offset) was also checked: none.

What `sub_82249580` derives besides P `[inferred-static]`:
- `RC+12568..12580` = far, near, 1/far, 1/near (from viewport +304/+300) - unaffected by an x shift.
- `RC+17152..17164` = viewport rectangle x, y, w, h (feeds the D3D viewport, via the `RC+17180 |= 0x600000` dirty bits).
- `RC+8228 |= 1` - consumed only by the pre-draw commit `sub_82249488` -> `sub_82248A78`.

**The source of P is the first 64 bytes of the 416-byte viewport entry** (`[RC+132]->[24] + 416*[RC+136]`),
filled by `sub_8275EEF8`. That builder has **26 other callers** - these are the CPU-side users of the
projection, and none of them sees the shift `[measured 2026-09-18]`:
`sub_82291850, sub_822951E0, sub_823471F8, sub_823EA0B8, sub_823FBE70, sub_8244B988, sub_824C2348,
sub_824C8A60, sub_824CC528, sub_824D3148, sub_824DC0B8, sub_824EFA78, sub_825AE510, sub_825B66E8,
sub_825C3918, sub_825E5BC8, sub_825E6C18, sub_825EB498, sub_825F54C8, sub_82637D60, sub_827349A0,
sub_8275F9F8, sub_8275FAE0, sub_8275FC50, sub_827646F0`.

## (c) The concrete candidate: `sub_825C3918` computes per-light screen rectangles from the unshifted view

`sub_825C3918(scene, viewctx)` (called once from `sub_825C4C50`) loops over the view's lights. For each
one whose bounding sphere does **not** contain the camera it calls `sub_825BCA80` four times (tangent
lines of the sphere, left/right/top/bottom), then for each tangent computes

`pixel = (t.x / t.z) * viewport[+280 or +284] * halfsize + centre`

rounds with `fctiwz`, clamps to the view rectangle, packs min/max as 16-bit pairs and intersects with the
rectangle already stored for that light (`0x825C4434..0x825C4628`). That is a classic sphere-to-scissor
calculation, done with the eye at the view-space origin - i.e. the **centre** eye. When the camera is
inside the sphere (`x*x + z*z - r*r <= 0`, branch to `0x825C462C`) the full rectangle is kept.
`[inferred-static]`

Why this fits the symptom `[hypothesis]`: the light pass and the stencil-shadow pass for a light are
confined to its rectangle. With geometry displaced 47-190 px and the rectangle not displaced, a shadow's
stencil writes (or the lit pixels) are cut at the wrong place. +e and -e displace the heads in opposite
directions relative to the rectangle, so one eye loses the shadow (heads lit red) and the other keeps it
(heads dark); the unshifted control is self-consistent and shows the true result (dark).

**Honest gap:** a misplaced rectangle on the *light* pass alone can only remove light, never add it. Red
appearing needs the *shadow* to be cut - so either shadow volumes get their own (tighter) rectangle, or the
real culprit is a sibling CPU decision from the same family (z-pass/z-fail choice or near-plane capping of
stencil volumes, decided from the unshifted eye position, which in a cramped cockpit is centimetres from
casters). Both are the same class - "CPU camera test sees the centre eye" - and both are cured by the same
change. I did not find the z-pass/z-fail decision statically.

## Things checked and found consistent (not the cause) `[inferred-static]`

- **The renderer is deferred-texturing, not depth-reconstruction.** `XRShader_DeferredMRT.fp` writes
  diffuse/specular/normal into MRTs; the per-light shaders (`XRShader_FP20Def_*.fp`) re-draw the real
  geometry and fetch those buffers through a projective screen coordinate (`DeferredTexCoord`, divided by w).
- That coordinate comes from `texgen_screen` in `System/Gl/VP.xrg`: `MUL r4,c93,r12.w; MAD r3,r12,c92,r4` -
  built from the **clip-space position**, so it follows the shift automatically and stays aligned with a
  G-buffer drawn with the same shift.
- Lighting itself is in **world space** (`WSPixelPosition`, `WSLightPosition`, `WSEyePosition` =
  `program.env[0..1]`), independent of P. Side effect worth knowing: specular highlights are computed for
  the centre eye in both eyes until the camera itself is moved.
- Stencil volume extrusion (`texgen_shadowvolume`) happens in model space before c0-c3, so it shifts correctly.

## (b) What the three viewport-A applies per frame are `[inferred-static]`

The draw-list executor `sub_825A24D0` tags every batch with a viewport index (byte at batch+40) and, when the
index changes, copies `scene[+5564 + 4*idx]` into the current viewport slot and calls `sub_82249580`. The
other callers re-apply the **same** viewport after a render-target change: `sub_8223EDB8` (selects the MRT
set, then `sub_82249580`), `sub_8223EB80`, `sub_8223FA80`, `sub_82254960` (x2, around resolves). So a burst of
three identical applies is most likely: executor selects A -> switch to the deferred MRT targets -> switch
back to the main target. They are not three cameras. Log 046 shows the per-frame order
V x3, A x3, V x3, B x1-2, where V is P00 0.68725 / P11 1.22177 / near 4.0 / far 2045 (FOV 95 at 4:3 - a third
viewport the dossier does not list yet) `[measured 2026-09-18, n=1 log]`.

## Live tests that would settle it

1. **Move the camera, not P.** Post-call hook on `sub_823F9B00(client, out)`: `out` (r4) is a 4x4 with rows at
   +0 forward, +16 right, +32 up, +48 position (layout per dossier debug-camera section). Do
   `out.pos += e * out.right`, leave P alone. Dark heads in both eyes = class confirmed. If the picture does
   not move at all, the dossier's "last link" (this output reaching the render matrices) is false.
2. **Sweep e from 0.05 to 0.6.** A scissor fault shows straight, axis-aligned cut edges that move smoothly with
   e. A z-pass/near-plane fault switches on at a threshold and has object-shaped edges.
3. `XREngine_VBEShowStencil.fp` exists in the shipped shaders - a stencil debug view is in the engine if a
   switch for it can be found.
