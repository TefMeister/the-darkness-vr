# ReXGlue SDK, 2026-11-06 to 11-11: every changed-memory upload between draws is a whole-buffer barrier pair; two Vulkan-only fixes for it; `skip_msr` already exists; 0.11 nightlies restructure the SDK

**Status:** 🆕 new · **Priority:** high for (1)-(2), medium for the rest. Bears on the `[PD]` row "THE CARD'S COST PER
DRAW" (~560 small memory uploads an eye sit between the draws, ~10 % of card time) and on how our SDK patches and
`patch_msr_lock.py` age.

## What is public

Read online on 2026-11-11 through the GitHub viewer and API (source files read at the `v0.10.0` tag and on the
`development` branch, issues, pull requests, release notes). Nothing was downloaded or copied.

### 1. How the D3D12 command processor uploads guest memory per draw `[inferred-static 2026-11-11, read from v0.10.0 source]`

For each draw, `IssueDraw` asks the shared-memory buffer for the vertex-fetch ranges (and memexport ranges) it uses
(`shared_memory_->RequestRange`). If any page changed since it was last uploaded, `D3D12SharedMemory::UploadRanges`
**transitions the WHOLE shared-memory buffer to `COPY_DEST`, submits the barriers, copies the pages, and the draw then
transitions it back for reading** (`UseForReading`). So every draw that finds a changed page costs a barrier pair on
the one buffer every draw reads, in the middle of the draw stream. The `development` branch has the same code. This
is the design inherited from Xenia.

What a barrier pair between draws costs on our card is not measured `[hypothesis]`: a transition of a buffer that
earlier draws are still reading normally makes the GPU finish those draws first, so ~560 of them an eye could
serialise the draw stream well beyond the copies themselves. A fixed cost per upload would show up in our numbers as
part of the "about 2-3 microseconds per draw that does not shrink with the picture size".

### 2. Two open pull requests that attack exactly this, Vulkan only `[reported 2026-11-11]`

- **#536 (datamothra, 2026-10-25) "record guest memory uploads ahead of the open render pass":** when the buffer is
  only being read and no draw already in the pass reads the pages being uploaded, the copy goes into a slot in front
  of the pass instead of ending it. Tekken 6 on an M1 Max: about 125 render passes a frame down to 56, about 21 frames
  shown a second up to 60. Switch `vulkan_hoist_uploads`. Needs #535 (a draw must never continue inside an ownership
  transfer pass).
- **#549 (mkalkhoven-po, 2026-10-29, closes #548) "compare rewritten guest pages instead of write-protecting them":**
  a page uploaded three frames running becomes "hot": no longer write-protected, compared against a shadow copy on
  request and uploaded only if it changed; back to normal after 120 quiet frames or on a watch or GPU write; at most
  64 MB of shadow copies. Need for Speed: Carbon (Vulkan, RTX 5080): about 90,000 uploads and 10,000-15,000 write
  faults a second in a race, ~15 % of the command-processor thread in `NtProtectVirtualMemory` before; race mean 87 to
  147 FPS after. Off by default (`shared_memory_hot_pages`). **The D3D12 half is explicitly left out**: it needs
  `EnableHotPages`, `CopyPagesForUpload` in `D3D12SharedMemory::UploadRanges` and a frame-end advance.

Our own 2026-11-10 thread sampling saw the port's graphics thread spend ~12 % copying memory and much of its lock
time on memory watches (note `2026-11-10g`), which is the write-protect cycle #548 describes `[hypothesis]`.

### 3. ROV research fork (#520, crabinacrabic, 2026-10-16) `[reported]`

The exact EDRAM path (D3D12 ROV / Vulkan interlock) on AMD: 83-92 % of pixel-shader time was the driver's ordering
wait; a short ordered section, a late depth test inside it, known render-target formats and an exact early depth
rejection took Viva Pinata from 13-15 to 30 game ticks a second, bit-identical EDRAM. The D3D12 versions are "not done
yet; expected to port", and only AMD was tested. We moved to `rtv` in b025, so this matters only if the `rtv` picture
turns out wrong and `rov` comes back.

### 4. `skip_msr` is a manifest key in v0.10.0 already `[inferred-static 2026-11-11, read from source]`

The code generator reads `skip_msr` (default false) from the project manifest; when set, `mfmsr` and `mtmsrd` emit
nothing at all: no global lock, no fence. Our `patch_msr_lock.py` (b024) instead rewrites the three lock macros after
each code generation and keeps the fences. Differences to weigh before swapping: with `skip_msr` an `mfmsr` leaves its
register unchanged instead of answering "interrupts on", and the `seq_cst` fences go too (`stwcx.` is still a real
compare-and-swap). Upstream is NOT dropping the lock: on `development` it is now `rex::platform::InterruptMask`, one
process-wide recursive mutex that interrupt callbacks also hold for their whole run, and `mtmsrd` writes only the EE
and RI bits (e5b48481, 2026-11-04).

### 5. The 0.11.0-dev nightlies (2026-11-09, 11-09, 11-10) `[reported]`

About 300 changes since v0.10.1: `rexcore.dll` split out of the runtime, the GPU as a loadable module ("Xenos
plugin", host API picked at run time), the sources moved (`src/graphics/d3d12/` is now `src/gpu/xenos/d3d12/`), hooks
declared in C++ (`REX_DETOUR`, mid-asm hooks in C++), a new guest heap (TLSF), "wait for work instead of spinning in
the Xenos workers" (#556-related), "skip render targets for draws whose depth test never passes", per-instruction fast
or exact vector math in codegen. **Every one of our SDK patches (01-16) would need re-making on it**, so it is not a
mid-wear upgrade. Open issue **#556** (2026-10-31): the vsync thread's `Sleep(1)` and `WAIT_REG_MEM`'s sleeping poll
add vblank and frame-pacing jitter on Windows; relevant if frame pacing in the headset looks uneven.

### 6. Xenia canary, Aug-Nov 2026: nothing on per-draw cost or stereo

Its GPU commits since August are accuracy work (EDRAM formats, MSAA sample positions, resolves, memexport write-back);
one D3D12 over-invalidation fix of fetch constants (95545f8e) is already in ReXGlue v0.10.0 (inclusive end index,
checked in the source). No stereo, multi-view or VR work in Xenia, ReXGlue or any other 360 recompilation found
`[reported 2026-11-11, GitHub repository and issue search; the same searches returned unrelated hits, so they ran]`.

## Why it matters here

- (1) + (2) name a concrete, measurable suspect for the size-independent per-draw cost, and two designs already
  worked out and measured by others, waiting for a D3D12 version. Both are shared-SDK work, so they would land on
  Condemned 2 and every ReXGlue title too.
- (4) could retire a post-generation patch script for a supported setting, or explain why we keep ours.
- (5) says when NOT to rebase.

## Next step it unlocks (modding side)

1. Measure first: count `UploadRanges` calls and their barriers per eye (a counter in the port or the trace's
   barrier list), then try the cheapest test: a frame with the same draws and uploads moved ahead, or hot pages on
   D3D12 behind a switch, and compare card time per eye in the alley.
2. Optional: build once with `skip_msr = true` in the manifest instead of `patch_msr_lock.py` and compare frames a
   second and hangs; keep whichever is plainer.

## Sources

- rexglue-sdk: releases (nightly-20261109-2d51d257, nightly-20261109-a5341fda, nightly-20261110-0b78fa33, v0.10.1)
  <https://github.com/rexglue/rexglue-sdk/releases>; `src/graphics/d3d12/shared_memory.cpp` and
  `command_processor.cpp` at v0.10.0; `src/codegen/builders/system.cpp`, `src/codegen/config.cpp`; commits e5b48481,
  bc4d43e6, 11539bc1, ecf56926
- rexglue-sdk pull requests and issues: #535, #536 (datamothra), #548 and #549 (mkalkhoven-po), #520 (crabinacrabic),
  #556, #508 — <https://github.com/rexglue/rexglue-sdk/pulls>, <https://github.com/rexglue/rexglue-sdk/issues>
- Xenia canary commits <https://github.com/xenia-canary/xenia-canary/commits/canary_experimental> (95545f8e, goldislead
  with has207)
