# What the "rov" drawing path costs on the card, and which settings could cut it (2026-11-10, background reader, static only)

From: the one background reader of the live session (b024). To: the modding lane.
Method: reading the runtime's source only. Nothing was launched, built or changed.
Source read: `D:/the-darkness-build/rexglue-sdk` (rexglue-sdk v0.10.0, c94f5eb, plus our patches), the tree
`dev-archive/build-scripts/home_build_sdk.bat` builds. All `file:line` below are relative to that folder.
Settings file read: `build-home-xr-b024/darknessrecomp.toml`. Card facts from that build's newest log.

Every claim is tagged. "Read from code" = `[inferred-static 2026-11-10]`. Anything about how MUCH time a
thing takes is a guess (`[hypothesis]`): the code says what work is issued, not what it costs.

## 0. One thing the session's own numbers say `[hypothesis]`, built on `[measured]` (n=1 each, note 2026-11-10g)

Card time per eye: 8 ms at 1 x 1, 11 ms at 2 x 2, 13 ms at 2 x 3. That is a straight line:
**about 7 ms that does not change with size, plus about 1 ms per "one 1280 x 720 worth" of pixels**
(7+1, 7+4, 7+6). So at 2 x 3 roughly half the card time is NOT pixel work, and no size-related setting can
touch that half. The Nsight trace should be read with this in mind: look for what is the same at every size
(vertex work, per-draw overhead of the ordered-view path, fixed-size compute passes, barriers), not only for
the big full-screen passes.

## 1. What "rov" adds on the card each frame

Path choice: `src/graphics/d3d12/render_target_cache.cpp:176-203`. The code's own comment at `:186`:
"The ROV path is currently much slower generally" (`[reported]`, upstream author).

| # | Work | Where | Scales with size? |
| --- | --- | --- | --- |
| 1 | **No depth buffer on the card at all.** The pipeline's depth/stencil state is only filled in for rtv (`src/graphics/d3d12/pipeline_cache.cpp:2926`), so the card cannot throw hidden pixels away before the pixel shader. Every rasterised pixel starts a shader that reads the old depth from the EDRAM buffer, converts, compares, and only then returns (`src/graphics/pipeline/shader/dxbc_translator_om.cpp:364-1059`, load at `:609`, quad-level early return at `:1019-1051`) | shader | yes (pixels x overdraw) |
| 2 | **Depth-only draws still run a pixel shader.** With rov a draw without a pixel shader gets `depth_only_pixel_shader_` (`pipeline_cache.cpp:2838-2840`); with rtv the same draw has no pixel shader | shader | yes |
| 3 | **Ordered access.** Every shader touches the one EDRAM buffer as a rasterizer-ordered view (`dxbc_translator.cpp:3244-3250`), so overlapping pixels of different triangles must run one after another on the card. Cost depends on overlap and triangle count as much as on size | card | partly; a likely part of the fixed 7 ms `[hypothesis]` |
| 4 | **Address maths per pixel**: two integer divisions and several multiplies to find the pixel in the EDRAM (`dxbc_translator_om.cpp:25-362`, divisions at `:73` and `:323`, one more per colour target at `:1950`) | shader | yes |
| 5 | **Depth conversion per pixel**: always float24, rounded, in the shader (`render_target_cache.cpp:873-875`, forced, no setting) | shader | yes |
| 6 | **Colour write per target, per pixel**: nothing is specialised; format, blend mode and write mask are run-time branches on constants. Load old colour (1 or 2 reads, `:2072`, `:2092`), unpack, blend, clamp, pack, store (`:2559`, `:2578`) (`dxbc_translator_om.cpp:1820-2620`). The upstream TODO at `render_target_cache.cpp:187-189` names exactly this | shader | yes |
| 7 | **Four-sample loops even with anti-aliasing off**: both the depth test and the colour write loop over 4 samples and skip the uncovered ones by branch (`dxbc_translator_om.cpp:468`, `:2023`) | shader | yes (small) |
| 8 | **Resolve = up to three full passes** per "copy the picture to a texture": compute copy EDRAM -> scaled resolve buffer (`render_target_cache.cpp:1287-1303`), then when the game samples it, a texture-load compute pass and a buffer-to-texture copy (`src/graphics/d3d12/texture_cache.cpp:1873`, `:1927`). Same in rtv | compute | yes |
| 9 | **Resolve clears** are compute passes over the EDRAM range (`render_target_cache.cpp:1352-1408`) | compute | yes |
| 10 | **Barriers**: one when the game's surface layout changes over an area already drawn (`src/graphics/pipeline/render_target/cache.cpp:661-678`), two state changes per resolve (`render_target_cache.cpp:1287`, `:1367`). Not one per draw (`:1077-1083`, `:1742-1754`) | card | no |
| 11 | **End of frame, ours**: gamma compute pass at full size (`src/graphics/d3d12/command_processor.cpp:2209-2211`), then with `DK_SBS=1` three to five full-size texture copies (`:2387`, `:2413`, `:2484-2486`), the headset blit (`src/graphics/d3d12/xr_blit.cpp:230`) and the desktop window | copies | yes; small `[hypothesis]`, maybe 0.3-0.5 ms |
| 12 | Game textures (load compute `texture_cache.cpp:1873`) and memory uploads (`src/graphics/d3d12/shared_memory.cpp:330`) | compute/copy | **no**, same at every size; only when memory changed |

Not in rov at all: EDRAM <-> texture ownership transfers (the base class returns before them,
`pipeline/render_target/cache.cpp:693-696`), sample-rate shading (`dxbc_translator.cpp:3346-3350`),
host-depth store passes. `[inferred-static 2026-11-10]` for the whole table.

## 2. Settings that could lower card cost

"Set" = present in b024's toml. Defaults are from the `REXCVAR_DEFINE_*` lines cited.

| Setting | Default | b024 | What it does | Picture risk | Expected saving |
| --- | --- | --- | --- | --- | --- |
| `draw_resolution_scale_x` / `_y` (`pipeline/texture/cache.cpp:66-72`) | 1 / 1 | **2 / 3** | size of everything drawn | sharpness | `[measured]` ~1 ms per step of 1 x area; 2 x 2 saves ~2 ms |
| `resolution_scale` (`:74`) | 1 | 1 (= default, so x/y win, `:236-244`) | both axes at once | | none |
| `anisotropic_override` (`pipeline/texture/cache.cpp:52`, used `d3d12/texture_cache.cpp:973-977`) | **3 = force 4x** on every mip-mapped, linear-filtered texture | not set | -1 = the game's own filtering, 0/1 = off | floors and walls at a flat angle a little softer (as on the console) | unknown, probably small on this card; free to try `[hypothesis]` |
| `render_target_path_d3d12` (`d3d12/render_target_cache.cpp:44`) | "" (rtv on NVIDIA) | **"rov"** | section 3 | | `[measured]` rtv slower at 2 x 3 |
| `gamma_render_target_as_unorm16` (`flags.cpp:24`) | true | not set | **rtv only** (rov forces false, `render_target_cache.cpp:871`). true = gamma surfaces as 16-bit-per-channel (twice the memory traffic, and a separate host surface from the plain 8-bit view of the same EDRAM, so switching views is a transfer; `:1428`, `:1711-1713`). false = 8-bit sRGB | dark tones and blends very slightly different from the console's curve | rtv only; could be large if the game draws to gamma surfaces `[hypothesis]` |
| `depth_float24_convert_in_pixel_shader` (`flags.cpp:20`) | false | not set | **rtv only** (rov forces true). false = host depth kept in a different encoding, which adds "host depth store" compute passes and a second depth source to every depth transfer (`render_target_cache.cpp:1703-1709`, `:3885-3913`). true = no such passes, but depth-writing shaders output depth themselves (`pipeline_cache.cpp:876-883`) | none expected (more exact) | rtv only, and only if the game's depth format is the float one (not checked) `[hypothesis]` |
| `depth_float24_round` (`flags.cpp:19`) | false | not set | rtv only, with the one above: rounding instead of truncating; truncating keeps the cheaper "less-equal" depth output (`dxbc_translator.cpp:3380-3385`) | leave false | none |
| `native_2x_msaa` (`flags.cpp:18`) | true | **false** | rtv only (`render_target_cache.cpp:388`); with `DK_VR_NO_AA=1` nothing asks for 2x anyway | | none |
| `direct_host_resolve` (`pipeline/render_target/cache.cpp:40`) | true | not set | rtv only: resolve straight from the host surface, skipping the dump to EDRAM (`render_target_cache.cpp:1198-1220`) | | already best |
| `mrt_edram_used_range_clamp_to_min` (`pipeline/render_target/cache.cpp:31`, used `:605`) | true | not set | each surface claims less EDRAM, so fewer overlaps: fewer barriers (rov) / transfers (rtv) | | already best; do not turn off |
| `execute_unclipped_draw_vs_on_cpu_for_psi_render_backend` (`:34`, used `:572`) | true | not set | rov: estimate how tall a screen-space draw is; only acts if `execute_unclipped_draw_vs_on_cpu` (default **false**, `util/draw_extent_estimator.cpp:27`, `:275`) is also on | none | tiny; CPU cost |
| `readback_resolve` / `d3d12_readback_resolve` (`command_processor.cpp:49`, `d3d12/command_processor.cpp:46`) | "none" / false | not set | already off (`command_processor.cpp:173-183`) | | none |
| `readback_memexport` (`command_processor.cpp:63`) | **true** | not set | read shader-written memory back; can force a full wait for the card (`d3d12/command_processor.cpp:3062`, `:3165-3166`) | game logic that reads it | the session measured "no change" with read-back off `[measured]` |
| `occlusion_query_enable` (`command_processor.cpp:46`) | true | not set | | | session measured no change `[measured]` |
| `d3d12_submit_on_primary_buffer_end` (`d3d12/command_processor.cpp:50`) | true | not set | | | session measured no change `[measured]` |
| `vsync` (`command_processor.cpp:39`) | true | **false** | only the guest's wait loop and vblank timer (`command_processor.cpp:1037`, `graphics_system.cpp:164`); not a card setting | | none |
| `d3d12_allow_variable_refresh_rate_and_tearing` (`ui/d3d12/d3d12_presenter.cpp:33`) | true | not set | desktop window may tear | | already best |
| `swap_post_effect` (`graphics_system.cpp:34`), `present_effect` (`ui/presenter.cpp:75`) | none / bilinear | not set | FXAA and upscalers are off | | already cheapest |
| `half_pixel_offset`, `resolve_resolution_scale_fill_half_pixel_offset` (`util/draw.cpp:30-32`, used `:326`, `:1055-1059`) | true / true | not set | where pixels sit, and a one-pixel edge fill on resolves | edges / seams if changed | none: no pass is added or removed |
| `draw_resolution_scaled_texture_offsets`, `snorm16_render_target_full_range`, `depth_transfer_not_equal_test`, `native_stencil_value_output` | | not set | correctness only; the last cannot help: this card reports "Pixel-shader-specified stencil reference: no" (b024 log line 5) `[measured]` | | none |
| `texture_cache_memory_limit_soft` / `_hard` (`pipeline/texture/cache.cpp:34-42`) | 384 / 768 MB (+24 MB x 5 at 2 x 3, `:262-268`) | not set | over the soft limit, textures unused for 30 s are dropped and loaded again | none | hitches, not steady cost |

**For rov there is no setting that lowers the per-pixel work except the size.** The depth and gamma switches
are overridden (`render_target_cache.cpp:867-878`). `[inferred-static 2026-11-10]`

## 3. Why rtv can be slower than rov here

Read from code `[inferred-static 2026-11-10]`:

- rtv keeps one host surface per (EDRAM base, pitch, format, samples). When a draw binds a surface over EDRAM
  last written through a different one, the old content is **drawn across as a full-surface pass** before the
  draw (`pipeline/render_target/cache.cpp:686-691`, `d3d12/render_target_cache.cpp:1067-1073`, draw at `:4549`).
  rov skips all of it (`cache.cpp:693-696`).
- **On this card a depth transfer is 9 passes, not 1**: no shader-written stencil (log line 5), so stencil is
  moved one bit at a time, 8 extra full-surface draws with discard (`render_target_cache.cpp:372-376`, `:4132`,
  `:4541-4549`). At 2560 x 2160 that is 9 x 5.5 million pixels per depth transfer.
- With the default `depth_float24_convert_in_pixel_shader = false` and a float depth format, each depth
  transfer also runs a "host depth store" compute pass (`:3885-3913`).
- With the default `gamma_render_target_as_unorm16 = true`, a gamma surface and the plain view of the same
  memory are two host surfaces; alternating between them is a transfer each time, at double the bytes.
- Resolve clears in rtv go through the same transfer routine (`:1335-1350`).

Guessed `[hypothesis]`: The Darkness re-uses the same EDRAM area for several surfaces within a frame, so
these transfers happen several times a frame and, being full-surface, grow with size. Not checked: the game's
surface layout was not read. If true, rtv's cost is mostly size-dependent while rov's is half fixed, so **rtv
may win at a smaller size even though it lost at 2 x 3**.

Settings that would cut rtv's transfers: `gamma_render_target_as_unorm16 = false`,
`depth_float24_convert_in_pixel_shader = true`; keep `direct_host_resolve`, `mrt_edram_used_range_clamp_to_min`
on. Nothing removes the 8 stencil passes on NVIDIA.

## 4. Cheap tests this suggests (each one toml line, simulator)

1. `anisotropic_override = -1` on rov, 2 x 3.
2. rtv at 1 x 1 and 2 x 2 (only 2 x 3 was measured): is rtv's fixed part smaller than rov's 7 ms?
3. rtv + `gamma_render_target_as_unorm16 = false` + `depth_float24_convert_in_pixel_shader = true`, 2 x 3.
4. In the Nsight trace: count full-surface passes that are not the game's (transfers, resolve copies, texture
   loads from resolves) and time the frame's fixed part against section 0.

If 2-3 do not beat rov, the remaining lever is code in the staging tree: a pixel shader specialised per surface
format and blend mode (the upstream TODO), or dropping the side-by-side copies when the headset is the output.
