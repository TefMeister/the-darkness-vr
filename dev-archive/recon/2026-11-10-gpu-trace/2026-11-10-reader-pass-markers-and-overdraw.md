# Pass markers for the GPU trace, and why a frame is ~28 layers deep (2026-11-10, background reader, static only)

From: the background reader of the live session (b024). To: the modding lane. Nothing launched, built into a
DLL, or committed. Follows `2026-11-10-reader-rov-gpu-cost.md`.

## A. The marker patch

In `~/github-backups/staging/the-darkness-vr/gpu-pass-markers/`: `gpu-pass-markers.patch`, `README.md`
(labels, files and lines), `make_patch.py` (rebuilds the patch without touching the SDK tree).

- The SDK already had the plumbing (`PushDebugMarker` / `PopDebugMarker`, the toml key `gpu_debug_markers`,
  `src/graphics/d3d12/command_processor.cpp:82-115`, replay at `deferred_command_list.cpp:256-270`) **but
  nothing ever called it**, which is why the trace was flat `[inferred-static 2026-11-10]`.
- The patch adds the callers and the switch `DK_GPU_MARKERS=1`: a range per guest render-target layout
  (pitch, samples, depth format, colour formats, "DEPTH-ONLY" when no colour is written), per resolve, per
  texture load, per memory upload, and for our gamma pass, side-by-side copies and headset blit.
- 7 files, 19 hunks. `git apply --check` against `D:/the-darkness-build/rexglue-sdk`: clean
  `[measured 2026-11-10]`. Syntax-only compile of the patched files with the SDK's compiler and flags: no
  errors `[compile-verified 2026-11-10]`. Never run: whether Nsight groups by these ranges is `[hypothesis]`.
- Markers are written as UTF-16, metadata 0 (the tree's unused code wrote ANSI, metadata 1).

## B. What the engine draws: stencil shadow volumes AND shadow maps, a depth pre-pass, a deferred G-buffer

All from names in the game program (`D:/the-darkness-build/unpacked/default.bin`, file offsets) plus the
dossier. Names prove a feature exists in the code; **which of them the alley uses was not checked**.

| Finding | Evidence | Tag |
| --- | --- | --- |
| **Stencil shadow volumes, on by a setting** | console variable `xr_stencilshadows` (0x8f00c), option `XR_STENCILSHADOWS` (0x8eba8); level data chunks `SVFIRSTPL`, `SVPLCOUNT`, `SVFIRSTLIGHT`, `SVVERTICES`, `SVPRIM` and `BSP2:%s:Shadow:%04d` (0x900d4-0x9012c): volumes stored per light in the level. Dossier "THE STENCIL DECISION": driver `sub_825D31E8`, two-sided INCR/DECR, extruded in the vertex shader, per-light scissor rectangles from `sub_825C3918` | `[inferred-static 2026-11-10]`; dossier part `[inferred-static 2026-09-18]` |
| **Shadow maps exist too** | `XRShadowMapManager` (0x90038), `xr_shadowmapmode` (0x8f114), `xr_smm_polygonoffset`, `ShadowMap mode: %s` (0x8edf4), per-level keys `SHADOWMAP_DISABLE` / `SHADOWMAP_TRIMESH` (0x6ce60), `XREngine_ShadowProj`, texture `$SHADOWMASK` | `[inferred-static 2026-11-10]` |
| **A depth pre-pass ("prez")** | `sv_wlod_range_model_bspprez` (0x72240), `sv_wlod_range_model_trimeshprez` (0x72298): a distance inside which world and mesh models get a pre-Z draw | `[inferred-static 2026-11-10]` |
| **Deferred G-buffer** | `XRShader_DeferredMRT`, `XRShader_DeferredNormal` (0x675c8), textures `$DEFERREDDIFFUSE` / `$DEFERREDNORMAL` / `$DEFERREDSPECULAR` / `$DEPTHSTENCILBUFFER` | `[inferred-static 2026-11-10]` |
| **Per-light caps by distance** | `sv_wlod_maxlights_bsp0/1`, `_trimesh0/1`, `sv_wlod_maxshadows_bsp0/1`, `_trimesh0/1` (0x72158-0x7220c), light fade ranges | `[inferred-static 2026-11-10]` |
| **The console culled with Hi-Stencil** | `CXR_PreRenderData_Render_SetHiStencil` and `Render_FlushHiStencil` (0x8bf80, 0x8bfe0) | `[inferred-static 2026-11-10]` |
| **The port does not emulate Hi-Z / Hi-Stencil** | no such code in the D3D12 backend (search of `src/graphics`, non-Vulkan: nothing) | `[inferred-static 2026-11-10]` |

### What that means for the 28 layers `[hypothesis]`

A frame is likely: pre-Z of the near world, the G-buffer pass (3 colour targets), then **for every light**:
its shadow volumes drawn front and back into stencil (depth-only, no colour, usually several layers deep
each), then the light's own pass over its scissor rectangle. On the console, hidden and shadowed pixels were
thrown away by the depth buffer and by Hi-Stencil before any shader ran. Under "rov" there is no depth buffer
on the card and no Hi-Stencil, so every one of those layers runs the pixel shader with a read (and for
volumes a write) of the EDRAM buffer, one overlapping pixel after another. That fits the trace: ~28 layers,
no hardware depth, shader cores mostly waiting.

Two things follow:
- **"rtv" should be far better at exactly this** (real depth/stencil on the card: volumes need no pixel
  shader, hidden pixels die early). That it measured slower suggests its ownership transfers cost more than
  that gain (9 passes per depth transfer on this card, see the earlier inbox file).
- The marker patch will show it directly: count the `DEPTH-ONLY` ranges and their draws.

### Cheap live checks these names offer (the session's call; picture changes, testing only)

- `xr_stencilshadows 0`: shadows gone; the drop in card time is what the volumes cost.
- `sv_wlod_maxshadows_*` / `sv_wlod_maxlights_*` lowered: fewer shadow-casting lights at distance.
- `xr_shadowmapmode`: values not read; may move shadows from volumes to maps.
Not checked: default values, whether these are reachable in the retail build, their effect.
