# The Darkness: where the camera sits in the vertex-shader constants

Date: 2026-09-16. Author: PD reader for the `/lm` session. Nothing was launched; everything below comes from
reading the ReXGlue SDK source (`E:/condemned-2-vr/src/rexglue-sdk`, v0.10.0) and the game's own data files.
This is for the dossier, §6 Seam B.

## 1. A snapshot taken at the swap is the wrong place to look for the camera

- **How constants are stored** `[inferred-static 2026-09-16]`, from reading the source. There is one persistent
  register array, `RegisterFile::values[0x5003]` (`include/rex/graphics/register_file.h:40-44`). Vertex float
  constant `i`, component `c` (0=x … 3=w), lives at `values[0x4000 + 4*i + c]`
  (`XE_GPU_REG_SHADER_CONSTANT_000_X = 0x4000`, `register_table.inc:654`; `255_W = 0x43FF`, `256_X = 0x4400`
  starts the pixel constants). There is **no per-draw copy and no history**. Each write overwrites the value,
  which then stays until the next write.
- **Write paths.** Ring packets `SET_CONSTANT` / `SET_CONSTANT2` / `SET_SHADER_CONSTANTS` / `LOAD_ALU_CONSTANT`
  (`graphics/command_processor.cpp` 1431-1512) go to the virtual `WriteRegistersFromMem`. The D3D12 override
  stores the values byte-swapped to host order with `memory::copy_and_swap` (`d3d12/command_processor.cpp:1839`)
  and marks `cbuffer_binding_float_vertex_` dirty. `frame_open_` only controls the dirty flag, never the storage.
- **Per-draw upload.** `D3D12CommandProcessor::UpdateBindings` (`d3d12/command_processor.cpp:4104-4130`),
  called from `IssueDraw` at line 2473, `memcpy`s `regs[0x4000 + (i<<8) + (bit<<2)]` straight into the GPU
  constant buffer as `float`s. So each stored u32 is the IEEE-754 bit pattern of the float, already in host byte
  order.
- **Why the swap is the wrong place** `[verified-numerically 2026-09-16, n=420 shaders]` for the layout;
  `[inferred-static]` for what that means for timing. In every vertex shader in the game's shader cache, the
  on-screen position is computed from **c0-c3 plus c7**, and those values are **the current object's** matrix
  (model × view × projection), not a camera-only value (see §3). They are rewritten for every object drawn. At
  the swap they hold whatever was drawn **last**: most likely a HUD, a 2D overlay or a full-screen post pass
  through the same pipeline, or at best the last world object. A swap-time log will show them changing when the
  stick moves only if the last draw happens to be a world object, and even then mixed with that object's own
  movement.
- **Where to sample instead.** Sample per draw, at `D3D12CommandProcessor::IssueDraw`
  (`d3d12/command_processor.cpp:2271`), right after `vertex_shader` is fetched at line 2290. That is before the
  early returns at 2304, 2324 and 2341, and before the async-compile skip at 2401-2404. Every draw with a real
  vertex shader passes through here, on the same thread that applies the register writes, so there is no race.
  `IssueDraw` is called from the base class at `graphics/command_processor.cpp:1381`. Record
  `vertex_shader->ucode_data_hash()` (`shader.h:834`) and the per-frame draw index alongside the constants.

## 2. How to read a constant

```cpp
const RegisterFile& regs = *register_file_;
float v;
std::memcpy(&v, &regs[XE_GPU_REG_SHADER_CONSTANT_000_X + (i << 2) + c], sizeof(float));   // i in 0..255
```

This is the same indexing as the upload loop at line 4123 (`+ (i << 8) + (index << 2)`, where `i` is the bitmap
word, so `(i<<8) + (bit<<2) == 4 * (64*i + bit)`).

## 3. The camera slots, found without running the game

- **Engine documentation** `[reported]`, from Starbreeze's own vertex-program template
  `game-files/System/Gl/VP.xrg`, lines 56-68: `c[0..3]` Model*Projection (model space to clip space);
  `c[4..6]` model rotate 3x3 (model space to view space); `c[7]` model translate; `c[8]`/`c[9]` fixed numbers.
  Position output at lines 1070-1074: `ADD R4, R8, c[7]; DP4 oPos.x..w, c[0..3], R4`.
- **Checked against the compiled Xbox 360 shaders** `[verified-numerically 2026-09-16, n=420]`.
  `game-files/System/Xenon/ProgramCache.xpc` holds 420 vertex-shader containers (magic `0x102A1101`; ucode at
  container+`u32@+4`, size `u32@+8`). A decoder written from the bit layout in `ucode.h:2009-2060` shows that
  **all 420** write the on-screen position from `dp4` against **c0, c1, c2, c3** (one row each, not relative),
  after `add rX, rY, c7`. The constant table in every one is a single `float4 c[128]` array at register 0
  (8 to 128 slots used). (22 other places where the magic number appears failed the size checks; they are
  probably coincidental matches.) The scanner script was scratch and is not kept.
- **Hypotheses to separate by measurement** `[hypothesis]`:
  - (a) For static world geometry, whose model matrix is the identity, `c4-c6` is the camera's pure rotation.
    It would be the cleanest thing to watch for "turns with the stick", and it would be the **same across many
    draws in a frame** (the most common value).
  - (b) c7 carries the camera's position relative to the object (camera-relative rendering).
  - (c) Some shaders are compiled at run time and are not in the cache file. Only a run with `dump_shaders` would
    show that.
- **`dump_shaders`** (`graphics/flags.cpp:27`, a string path). Setting it writes
  `shader_<hash>.ucode.bin.vert` plus a disassembly `shader_<hash>.ucode.vert` into that folder
  (`pipeline/shader/shader.cpp:122-150`), along with translated D3D12 files (`d3d12/pipeline_cache.cpp:1224`).
  It is not needed for the slot answer above, only for checking (c).
