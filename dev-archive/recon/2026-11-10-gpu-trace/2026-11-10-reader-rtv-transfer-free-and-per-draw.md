# Making "rtv" transfer-free for this game, and what each draw costs (2026-11-10, background reader, static only)

From: the background reader (b024 session). Nothing launched, built or committed. SDK paths are relative to
`D:/the-darkness-build/rexglue-sdk`; game code is `D:/the-darkness-build/src/DarknessRecomp/generated/default`.
SDK line numbers are for the tree WITH the marker patch applied (they differ from the two earlier files by the patch's added lines).
Follows `2026-11-10-reader-rov-gpu-cost.md` and `2026-11-10-reader-pass-markers-and-overdraw.md`.

## A. Which layout switches transfer under rtv `[inferred-static 2026-11-10]`

A host surface is one per key = (EDRAM base, pitch in tiles, **samples**, depth-or-colour, storage format)
(`src/graphics/pipeline/render_target/cache.cpp:636-641`). A transfer is queued only when a draw claims tiles
whose LAST OWNER is a different key (`cache.cpp:1292-1297`).

| Switch seen in the trace | Transfer? | Why |
| --- | --- | --- |
| c0 `2_10_10_10_FLOAT` <-> `2_10_10_10_FLOAT_AS_16_16_16_16` at 720 | **no** | both map to the same storage format before the key is built (`include/rex/graphics/xenos.h:353-362`, `cache.cpp:492`): one host surface |
| colour writes off <-> on, same depth | **no** for depth (same key). Colour keeps its tiles while unbound. One caveat: a depth-ONLY draw is a single target, and the "clamp to the next surface" rule needs two (`cache.cpp:605`), so it claims as many rows as its scissor/viewport reaches (`cache.cpp:569-575`, `628-631`); beyond row 720 it would take colour's tiles. Not expected with a 720-row scissor | |
| depth 1x pitch 1280 <-> 4x pitch 640 at base 0 | **yes, both ways** | same tiles, same pitch in tiles (16), samples differ. Each way on this card: a stencil clear, 1 depth draw, **8 stencil-bit draws** (`src/graphics/d3d12/render_target_cache.cpp:4157-4166`, `4234-4242`, `4566-4574`), plus, because depth is D24FS8 with the default float24 setting, a host-depth-store compute pass (`:3910-3937`) |
| c0 FLOAT@720 bound in a 4x range | **yes, both ways**, 1 draw each | same reason |

So per 4x excursion: 18 full-surface depth passes + 2 colour passes; three excursions per eye is about 60
passes, ~330M pixels at 2 x 3: the size of the rtv excess you measured `[hypothesis]`.
**The 35M ranges:** 9 passes over 512 of the surface's 720 tiles (32 of 45 tile rows) is 35.4M pixels at
2560 x 2160; so these look like the depth transfer back from (or into) a 4x range that owned 512 tiles
`[hypothesis]`; check that each 35M range sits next to a `640 4x` range. The pixels belong to the first draw
of the range (transfers run inside `Update`), which is why a range with 0-4 draws can carry them.

## B. Making rtv transfer-free (ordered by expected saving)

1. **Stop the 4x clears: hook `sub_828621D0`** (the console graphics library's fast clear; three call sites,
   all in `sub_82862420`) `[inferred-static 2026-11-10]`. Arguments: r3 device, r4 x1, r5 y1, r6 x2, r7 y2,
   r8 colour pointer, r10 the surface-info word, f1 depth, more on the caller's stack. It aligns the rectangle
   to 80 x 16, clears the unaligned edge strips with the surface as it is (first `bl sub_82861EE0`, the emitter
   that writes the `0x12000` packet), and clears the aligned middle as a **4-sample surface of half the
   pitch, with the rectangle halved** (`rlwimi r25,r11,17,14,15`, the shifts by r29/r24). When the aligned
   middle is empty it takes `loc_828623D8`: ONE clear of the whole rectangle with the surface untouched.
   The fix is to always take that branch: a `REX_HOOK_RAW(sub_828621D0)` that does not call the original but
   does what `loc_828623D8` does: make a 288-byte frame, store x1,y1,x2,y2 at frame+96..108, then
   r4 = 1, r5 = frame+96, r6 = old r8, r8 = old r10, r9 = word at caller-sp+84, r10 = word at caller-sp+92,
   frame+84 = word at caller-sp+108, call `sub_82861EE0`, restore r1. Same pixels end up cleared (a 4x
   640-pitch surface and a 1x 1280-pitch one are the same EDRAM memory); the edge strips already work this
   way. Sketch only, not compiled. Also helps rov (fewer layout changes, no forced 4-sample draws).
   Not checked: `sub_829E0D68` and `sub_829E2B20` contain the same "set 4 samples" instruction; if `640 4x`
   ranges survive the hook, look there.
2. FLOAT vs FLOAT_AS_16: already one host surface, nothing to do.
3. Keep `depth_float24_convert_in_pixel_shader = false` (default) for rtv: set to true, every pre-Z and
   shadow-volume draw gets a depth-converting pixel shader (`src/graphics/d3d12/pipeline_cache.cpp:2842-2852`),
   which throws away rtv's main advantage. `gamma_render_target_as_unorm16` is moot: no gamma surface seen.
4. After 1, the remaining per-frame rtv extras are resolve clears (through the transfer routine but with no
   source copy when the owner is unchanged, `render_target_cache.cpp:1360-1375`) and the resolves themselves.

## C. What one draw emits, and what could go `[inferred-static 2026-11-10]`

Already skipped when unchanged upstream: pipeline (`src/graphics/d3d12/command_processor.cpp:2817-2821`), root
signature (`:4432`), viewport, scissor, stencil reference, blend factor, topology (`:870-907`, `:3963-3986`),
render-target binding, each constant buffer (uploaded and re-bound only when its registers were written,
`:4499-4587`). There are no vertex-buffer binds at all: vertices are read from the shared-memory buffer in the
vertex shader. NOT deduplicated: the index buffer bind, every indexed draw (`:3084`).

| Per-draw item | Cost on the card | Could be cut |
| --- | --- | --- |
| **Shared-memory upload between draws** (your ~560 `memupload` per eye): the 512 MB buffer goes read-state -> COPY_DEST, one copy, and back to read-state for the next draw (`src/graphics/d3d12/shared_memory.cpp:309`, `:341`; `include/rex/graphics/d3d12/shared_memory.h:47-52`). Two state changes of the buffer every draw reads from, in the middle of a pass | the likeliest source of the 2-3 microseconds per draw `[hypothesis]` | **Hoist them:** record a submission's uploads in a second list replayed before the draws (one state change per submission). Safe only for a page no earlier draw of the same submission has read; keep a per-submission "pages read" bitmap (set in `RequestRange`) and fall back to today's inline copy when the page is in it. Design only |
| System / float / fetch constant buffers: new upload-buffer address + root CBV when dirty | small | already minimal |
| Index buffer bind | tiny | compare with the last view |
| Pipeline switch when shaders or state change | driver cost, per change | sort is not possible (guest order) |

First check, free with the markers: do the `memupload` ranges fall inside the pre-Z range (979 draws)? If the
same few pages are uploaded again and again, the game rewrites one small buffer per draw (hoisting will mostly
fall back), and the better fix is on the game side or a finer dirty-tracking than 4 KB pages.
