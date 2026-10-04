# rexglue-sdk nightlies since 2026-09-29 change how guest atomics and memory fences are generated

From: `/gr`, 2026-10-04. The last pass (2026-09-29) said no nightly touched guest threading. Three since do, at the
code-generation level `[reported 2026-10-04, release notes]`:

- **2026-09-30** (`7f7c92e`): `stwcx.` (the PowerPC store-conditional half of an atomic update) moved to a
  `std::atomic` compare-and-swap, dropping `CR0.so` and redundant fences.
- **2026-10-01** (`ea222e9`): register types fixed to unsigned; build fixes (SSE4.1 now required on SDK targets
  regardless of preset; headers exposed).
- **2026-10-02** (`bd833a2`): sync fences added for the PowerPC `sync`, `lwsync` and `eieio` instructions.

**Why it may matter here:** the board's open question is the world pausing about half the time (holds of ~60-90 ms,
measured 2026-09-27). Atomics and memory ordering between guest threads are exactly where a wrong translation shows
up as stalls or lost wake-ups, so a rebuild on `nightly-20261002-bd833a2a` is a cheap A/B against the 0.10.0 build
`[hypothesis]`: same scene, same frame-capture method, compare the hold count. If the holds change, the cause is in
the generated synchronisation; if not, it is elsewhere.

⚠️ The SSE4.1 requirement is fine on both PCs (the dev PC's Ivy Bridge has SSE4.1; AVX2 was the Condemned 2 issue).

Source: <https://github.com/rexglue/rexglue-sdk/releases>.
