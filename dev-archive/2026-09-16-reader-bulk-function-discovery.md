# Bulk recovery of the function starts ReXGlue's analyser missed (The Darkness, 2007)

Author: PD reader session, 2026-09-16. Static analysis only — nothing was launched, no
project file was modified. Tool written to `E:/the-darkness/src/DarknessRecomp/tools/find_missing_functions.py`.

## 1. What the FATAL actually is

`[FATAL] Call to invalid or unregistered function at guest address 0x…` is emitted from
exactly one place: `REX_CALL_INDIRECT_FUNC` in the project's generated
`darknessrecomp_pch.h`, which falls back to
`rex::runtime::ResolveIndirectFunction` → `InvalidFunctionTrap`
(`rexglue-sdk/src/system/function_dispatcher.cpp:38`). **[verified-numerically 2026-09-16]**

So every one of these is an **indirect** call target (`bctr` / `bctrl` / `blrl`) — a
function pointer the static analyser never saw. It is *not* an unresolved direct branch
(those become a compile-time `REX_FATAL("Unresolved call …")` instead). This matters: no
amount of direct-call graph walking will ever find them.

## 2. The XEX can be unpacked offline in ~60 lines of Python

`default.xex` is **encryption=1, compression=1 (basic)** — AES-128-CBC with the retail key,
then a plain (data_size, zero_size) block list. No LZX. **[verified-numerically 2026-09-16]**

- retail key `20B185A59D28FDC340583FBB0896BF91` decrypts it (devkit/zero key does not)
- the unpacked image is laid out **by RVA**: file offset = guest address − 0x82000000.
  ⚠️ It is *not* laid out by PE raw pointer. Using rawptr produced a plausible-looking but
  completely wrong disassembly for everything from `.text` onward, and cost this session a
  full wrong round of conclusions.
- sections: `.rdata` 82000600, `.pdata` 8209F800, `.text` 820C0000–829C6644, then eight
  `.embsec_*` executable sections up to 82A19750, `.data` 82A20000–82AC2FA0, `.idata`,
  `.tls`, `.reloc`.

## 3. Why the analyser misses them — the RTTI vtable scanner finds nothing at all

`VTableScanner::findCompleteObjectLocators()` requires MSVC RTTI Complete Object Locators
in `.rdata` with `.?AV`/`.?AU` type names. **This binary contains 0 of them** — it is a
retail `/GR-` (no-RTTI) build. **[verified-numerically 2026-09-16, n=1 binary]**

Therefore ReXGlue's entire vtable-slot discovery path contributes **zero** functions here,
and every vtable in the game is invisible to it. Discovery is left with: entry point,
imports, `.pdata`, direct `bl` targets, and gap fill.

`.pdata` holds **15,722** RUNTIME_FUNCTION entries and **all 15,722 are already registered**
— that source is fully exploited, nothing to gain there. **[verified-numerically 2026-09-16]**

The misses are small thunks that MSVC emits in blocks under one shared `.pdata` record
(vtable dispatch stubs, adjustor thunks), reachable only through a function pointer.
Typical body, four instructions:
`lwz r12,0(r3) ; lwz r11,0x350(r12) ; mtctr r11 ; bctr`.

Empirically **98.39% of real function starts in this binary begin with `mflr r12`
(0x7D8802A6)** — but **0%** of the missed ones do, because they are leaf thunks.
A prologue-signature scan is therefore the wrong instrument for this problem.
**[verified-numerically 2026-09-16, n=15722]**

## 4. Two scans recover them; both are absent or disabled in the SDK we use

**A — absolute pointers in data sections.** 4-aligned big-endian dwords in
`.rdata`/`.data`/`.idata`/`.tls` that land in executable space. 10,054 distinct targets,
38,193 sites. **[measured 2026-09-16]**

**B — materialised addresses in code.** `lis rD,hi` followed within 32 instructions by
`addi rX,rD,lo` or `ori rX,rD,lo`, where the result is then stored / `mtctr`'d / left in an
argument register within 8 instructions. 1,099 distinct targets. **[measured 2026-09-16]**

Neither runs today:
- the SDK in use (`E:/condemned-2-vr/src/rexglue-sdk`, upstream v0.10.0) has **no
  `scan_data_pointers` cvar at all** — that knob exists only in the `Genesis5500/ArmyOfTwo-Recomp-rexglue`
  fork, which adds it (default `true`) as ~80 lines at the end of `discoverAllFunctions()`.
  **[verified-numerically 2026-09-16 — grep of our SDK returns nothing]**
- `functionPointerScan()` (scan B) **is** in our SDK's `phase_discover.cpp` but its call site
  is commented out in `analyze.cpp` with *"disabled for now, causes too many false positives"*.
  It also lacks the use-filter, which is what makes it noisy.

**No existing cvar raises discovery here.** `max_discovery_iterations` (1000),
`max_vtable_iterations` (100), `max_resolve_iterations` (100), `backward_scan_limit` (64),
`max_jump_table_entries` (512) all govern convergence limits of passes that already reach a
fixed point. Raising them changes nothing. **[inferred-static 2026-09-16 — read of
codegen_flags.cpp + phase_discover.cpp; not tested by re-running codegen]**

## 5. Result: 235 high-confidence candidates, and a 3/3 backtest

`tools/find_missing_functions.py` produces the list. Filters, in order:

1. target is 4-aligned and inside an executable section
2. target is not already in `codegen.partition.json`
3. target is **not already a `loc_XXXXXXXX` label in the generated C++** (156,411 of those).
   Registering an intra-function label splits a real function and turns its internal
   branches into unresolved calls — strictly worse than the bug being fixed. This is the
   same guard both reference projects use (`containsAddress` on discovered blocks).
4. the word *before* the target is `blr` / `bctr` / unconditional `b` / padding / `nop`
   — i.e. it cannot be a fall-through into the middle of a live basic block.

Counts: **11,107 raw candidates → 10,803 (97.3%) are addresses ReXGlue independently
registered already** (that is the precision check) → 36 are existing labels → **304 are new**,
of which **235 pass the boundary test**. **[verified-numerically 2026-09-16]**

**Backtest — all three addresses that actually crashed the game are in the set:**

| address | evidence | boundary |
| --- | --- | --- |
| 0x828BC2A0 | 2 pointers in `.rdata` (a vtable at 0x82007DF0) | after `b` |
| 0x823CEF48 | 1 pointer in `.rdata` (vtable at 0x8207CC60) | after `bctr` |
| 0x82654948 | materialised by `lis`/`addi` at 0x8264A8D4 | after `b` |

**3 of 3. [verified-numerically 2026-09-16, n=3]** Note that 0x82654948 appears **nowhere**
in the image as a stored pointer — scan A alone would have missed it. Both scans are needed.

Whole tables come out at once: `.data` 0x82A28F7C–0x82A28FA0 is eleven consecutive
unregistered pointers into the `.embsec_` region — an entire function-pointer table the
analyser never touched.

## 6. What is NOT claimed

- **Nobody has re-run codegen with these 235 entries.** Whether they all compile, and
  whether the game gets further, is untested. `[hypothesis]` until a run happens.
- The 69 "weak" candidates (no terminator before them) are withheld by default; they are
  more likely to be data misread as pointers, or genuine mid-function addresses that need
  `{ parent = … }` rather than a bare registration. `--all` emits them.
- This finds indirect-call targets. It does **not** address unresolved *jump tables*
  (`bctr` with no recognised switch table), which are a separate class needing
  `[[switch_tables]]` entries. The generated C++ contains 27,790 `REX_CALL_INDIRECT_FUNC`
  sites in total, so the indirect surface is large and more misses may surface later.
  **[measured 2026-09-16]**

## 7. Prior art (read directly from GitHub, 2026-09-16)

- `Genesis5500/ArmyOfTwo-Recomp-rexglue` — fixes it **inside** the SDK: ~80 lines in
  `phase_discover.cpp` plus one bool cvar `scan_data_pointers` (default true). Their game
  manifest then needs only **5** hand-added `[functions]` entries. Their comment states the
  same diagnosis: retail no-RTTI build ⇒ zero COLs ⇒ vtable scanner finds nothing.
- `Player124413/rexauto` — fixes it **around** the SDK with a write-TOML / re-codegen loop,
  and adds: the materialised-pointer scan (scan B, with the store/`mtctr`/arg use-filter this
  tool copies), a gap-fill pass on uncovered code bytes, harvesting
  `REX_FATAL("Unresolved …")` out of the generated C++ so the whole set is cured statically,
  and `clamp_overlapping_ends` — **ReXGlue rejects the entire config on the first overlapping
  boundary**, so any bulk generator must keep entries non-overlapping. Address-only entries
  (no `size`/`end`) cannot overlap, which is why this tool emits only those.
  Their published "cure" counts per title run 1–1789, so a few hundred is a normal size.
- Both accept `[functions]` top-level in an included file (what this project already does)
  or as `[entrypoint.functions]`.
- `config.cpp` also parses an `[analysis]` section (`max_jump_extension`,
  `data_region_threshold`, `large_function_threshold`, `exception_handler_funcs`) that
  neither project uses. **[reported]**
