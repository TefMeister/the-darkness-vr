# 2026-09-18 (late) — holding the world still, and the numbers that were missing

Second half of the `/lm` session in `2026-09-18-moving-the-eye-into-the-camera.md`. The reader
answered all four of its questions before finishing, and one of its byproducts changes what static
work costs from here on.

## The reader's big byproduct: the executable unpacks in 80 lines

`default.xex` is encrypted but **not** LZX-compressed, so it decrypts with plain Python. The result
is a real PE image at `0x82000000`, verified by reproducing this project's own `.rdata` viewport
defaults exactly `[verified-numerically 2026-09-18]`. **45,101 strings readable, every `lis`/`addi`
pair resolvable, a 21,414-function index and a call graph.** The engine names itself: Starbreeze's
**XReality**. Scripts are in `dev-archive/tools/`; the large derived indexes are in private
`staging`.

Until today every static question meant reading generated C++. Now the binary answers directly.

## Holding the world still — built, and the clock pin is verified

**There is no fixed time step.** Everything time-dependent reads one wall clock and differences it
itself, and that clock is a single function, `sub_828A7DB8` (QueryPerformanceCounter → `mftb`), the
**only engine-visible timer, 47 callers** `[measured 2026-09-18]`.

So the freeze is a hook on that one function, switched on for the second eye's frame by the swap
hook we already had. Two deliberate choices, both to avoid known failure modes:

- **Advance one 20 ns tick per call rather than freezing dead**, so a guest loop polling for a
  timeout can still expire instead of hanging.
- **Never go backwards** — a negative time step would throw geometry across the level.

✅ **The pin engages and holds**: exactly half of all frames pinned (900 of 1800, 1050 of 2100, 1350
of 2700), clock held back by up to **~82 ms**, about one frame, each time
`[verified-live 2026-09-18, n=1 run]`. No crash, no visible stutter.

⚠️ **What is measured is that the CLOCK is held.** That the *world* visibly holds still follows only
because this is the only engine timer — which the reader measured, but I have not confirmed on
screen. **Do not record the freeze as working until a visual check is done.** The obvious one:
compare how far the car travels per second with the freeze on and off.

⚠️ Also worth knowing: the engine polls that clock **~40,000 times per frame** (72.7 M calls over
1,800 frames). The hook is on a very hot path.

❌ Not the SDK's `guest_time_scalar`: its vblank worker reads the same scaled clock, so slowing guest
time also stops vblanks and the swap queue never drains.

## The numbers that were missing

- **One world unit is about one inch** (band 2.5–3.2 cm), from seventeen shipped constants that only
  make sense at that scale — head offsets of 58 and 50, `AI_HEIGHT` 56, a 64-unit run stride
  `[measured 2026-09-18]`. The live cross-check agrees: 17.5 units to the car's seat backs is 44 cm,
  and viewport A's 1.80 near plane is 4.6 cm. **So half an eye separation is ≈1.25 units**, about
  twice what the first experiments used.
- **There are four viewports, not three**, and the FOVs invert to round authored numbers — 70° for
  the player, 95° and 90° for the other two, the 90° one being *exactly* the constructor defaults.
- ✅ **Both non-player viewports are already cleared.** The earlier "far" run selected by near plane
  and both have near 4.00, so it shifted both and nothing moved — five pairs, quality ≥ 0.99.
- ✅ **The 30 fps lock is real but not biting.** The game asks for `D3DPRESENT_INTERVAL_TWO` when
  vsync is on, but our config already has `vsync = false`, which gives ~1,000 vblanks a second. **The
  dev PC's ~15 fps is the dev PC. There is no cap to raise right now.**
- ✅ **The stencil decision is a winding flip from a handedness test of the view basis** — a sideways
  translation cannot touch it. ⚠️ But it means the offset must go in the **position row only**: leave
  the basis non-orthonormal and every shadow in the frame inverts.

## What is NOT established

- **That the world visibly holds still.** Clock pin measured; on-screen effect not checked.
- **A clean disparity measurement at the true 1.25-unit separation.** The run was made, but several
  grab pairs came back bit-identical (the window showing one frame across two grabs) and others
  disagreed, so the numbers are not trustworthy yet. The capture side needs to key off the swap, not
  a wall-clock sleep — which is the same argument for doing the side-by-side composition at Seam A.
- Whether the lighting defect is truly cured or merely not provoked (unchanged from the last note).
- Nothing in a headset.

⚠️ **One thing to tell Tefa before the first headset test:** at true eye separation the hands and gun
sit 10–20 cm from the eye and will be **hard to fuse**. That is the known VR first-person-arms
problem rather than a bug, and the fix is to push the arm and weapon model out or give it a smaller
separation of its own.
