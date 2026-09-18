# 2026-09-18 (evening) — the first true stereo pair, the two-eyes decision, and two things I got wrong on the way

`/lm`, dev PC, Claude (Fable) driving, Tefa at work. Five live runs. The reader ran static work
alongside and was still working when this was written.

## The decision

**Geometry stereo, one eye per guest frame, the world held still for the second eye** ("synced
sequential"). Offset flipped per frame at the proven site (end of `sub_82249580`, viewport A only),
frames routed to the headset by swap parity at Seam A. **Depth reprojection is the named fallback.**
Reasoning, costs and build order: dossier §6, "DECIDED 2026-09-18".

The short version of why: this game keeps hands, guns and tentacles at arm's length all game long,
and the first pair measured **~190 px of disparity on the hands against 47 px on the seat backs**. A
synthesised second eye would have to invent everything those near objects uncover, permanently, in
the middle of the picture. A real second render does not have to invent anything.

## What was proved

- ✅ **A real, repeatable left/right pair.** `DK_STEREO_EYE=0.6`, eye flipped every 2 frames, window
  grabs ~135 ms apart: interior **47 px**, hands **182–208 px**, HUD **0 px**, in 7 of 7 usable pairs
  `[verified-live 2026-09-18, n=7 pairs]`. Evidence in
  `dev-archive/recon/2026-09-18-first-stereo-pair/`.
- ✅ **Only viewport A needs the offset.** Shifting B alone moves nothing — 0 px everywhere at match
  quality ≥ 0.99, five pairs `[verified-live 2026-09-18, n=5 pairs]`. A draws the world and the
  first-person hands.
- ✅ **Flipping every single frame is stable** — a 30 s gameplay run, 4,542 flips, no fatal
  `[verified-live 2026-09-18, n=1]`.
- ✅ **The true frame boundary is `sub_82867620`**, the only generated function that calls `VdSwap`.
  The eye now flips only there.

## Two things I got wrong, both caught the same day

**1. "Viewport A is applied once per frame."** It is applied **three times** (histogram over 2,400
frames: 847 with three applies, 99 with two). I had inferred "once" from the A/B alternation and
from an average of 29.4 applies a second that happened to look like a 30 fps cap. The interval
histogram showed the truth: bursts a few ms wide, then ~66 ms gaps. **The dev PC runs this scene at
about 15 frames a second.** Because the first alternation code flipped on applies, it was changing
eye in the middle of frames.

**2. "The shift does not disturb lighting" — RETRACTED.** The first three pairs all showed red-lit
heads in one eye. I tested it across 50 labelled frames, found red in both eyes alike, and recorded
it as a passing light. **That test could not have found the effect**, because its eye labels came
from the mid-frame flipping above and were scrambled. With frame-true labels the front-seat heads
are **red-lit in one eye and dark in the other for 12 consecutive alternations**
`[verified-live 2026-09-18, n=12]`. A fixed `+0.5` run shows the red; the unshifted control does
not.

> The pattern is worth keeping: n=3 by eye said "eye-specific", n=50 by statistic said "no", and the
> n=3 was right — because the statistic was built on a broken label. A negative result is only
> evidence if the test could have produced a positive one, and I did not re-check that after
> finding the tool it depended on was wrong. It is the house rule, and it applied here exactly.

## Open

- ❌ **The per-eye lighting defect.** Something reads P and does not get a consistent answer per
  eye. Cause unknown; with the reader. First real cost of the chosen route.
- **Which grab is which eye** cannot be settled from outside the process — the measured sign came
  out consistently inverted against the hook's labels, which fits a one-eye-span display lag
  `[hypothesis]`. It is exact at Seam A.
- **Holding the world still** for the second eye: not attempted. Waiting on how the game keeps time.
- **Frame rate.** ~15 fps here means ~7 pairs a second on this machine. The home PC is unmeasured,
  and so is whether the guest is capped at 30.
- **World-unit scale**, for a physically right eye separation: with the reader. From disparity, the
  seat backs sit ≈ 17.5 units away and the hands ≈ 4.
- Nothing has been seen in a headset.
