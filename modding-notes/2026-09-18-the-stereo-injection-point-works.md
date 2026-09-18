# 2026-09-18 — the stereo injection point works in the running game

`/lm`, dev PC `DESKTOP-V8GTSIR`, Claude driving, Tefa at work. Six live runs, one x64dbg session
earlier in the day. This is the `[FLAT]` **RESUME HERE** row, both steps, done.

Everything about the injection point until today was **static** — read out of the disassembly and
checked against a Python emulation of the function. Nothing had been observed in the running game.
Now it has.

---

## The probe

`src/vr_viewport_probe.cpp` (copied into `dev-archive/port-project/src/`), attached with the
**weak-symbol hook** the dossier settled on: `REX_HOOK_RAW(sub_82249580)`, calling
`__imp__sub_82249580(ctx, base)` first and working on the result. No generated code was edited and
no codegen re-run was needed. `[compile-verified 2026-09-18]`

It defaults to **observe-only**. The shift is off unless `DK_STEREO_EYE` is set in the environment,
so the installed build is safe to launch normally.

## Step 1 — the sanity check: PASSED, and it could have failed

**1. How often does it run?** During live gameplay, **exactly two viewport applies per frame, in
perfect 1:1 alternation** — 1344/1344 and later 600/600 over separate windows
`[verified-live 2026-09-18, n=3 runs]`. Of ~16,700 calls in one run, ~7,800 were a change of matrix;
the rest were repeats.

| | P00 | vertical FOV | near | far | seen in |
| --- | --- | --- | --- | --- | --- |
| **A** | 1.07111 | 55.4° | **1.80** | ~5000 | gameplay (and videos, varying slightly) |
| **B** | 0.75000 | 73.7° | **4.00** | ~2045 | gameplay **and** menus, rock steady |

Both are 16:9 (P11/P00 = 1.7778). **A varies slightly between runs, B never does.**

**2. Is there a perspective P?** Yes — but ⚠️ **there is nothing else. Every single one of the 9,004
logged matrices in the longest run was perspective; not one orthographic viewport passed through
this function** `[verified-live 2026-09-18, n=3 runs]`.

> ⚠️ **This corrects the plan in dossier §6.** It said to apply the shift only to perspective
> viewports, `RC+17132 == 1.0 && RC+17148 == 0`, so the orthographic HUD and menus would be left
> alone. **That filter passes everything and protects nothing.** The HUD does not come through here
> at all. The viewports must be told apart another way — the near plane does it cleanly (1.80 vs
> 4.00) and that is what the probe uses.

**3. Does P move when the camera turns?** **No.** With the look stick held at **full** deflection —
right, then left — the set of matrices applied is **bit-identical** to a no-input control taken
seconds earlier in the same run:

```
CONTROL - no input          704 lines, 2 distinct P
LOOK RIGHT - full stick     172 lines, 2 distinct P   identical set: True
LOOK LEFT  - full stick     176 lines, 2 distinct P   identical set: True
matrices seen only while the stick was held: none
```

`[verified-live 2026-09-18, n=1 run, 2 turns + 1 control]`

⚠️ **And the test could have produced a positive**, which is the part that matters: the screenshots
either side of the turn show the view plainly changed. **The first attempt at this test was
worthless and is recorded as such** — a 0.55 stick deflection moved nothing at all (the control
profile warns that look speed ramps), so "P did not change" meant only "nothing happened". Full
deflection fixed it.

**So the §6 model holds: the camera turn lives in the view matrix M, and P is fixed.** That is the
assumption the whole injection point rests on.

## Step 2 — the eye shift: IT WORKS

`P' = T(e)·P`, T(e) being the identity with translation row `(e,0,0,1)`. In this engine's row-vector
convention that is `v·M·T(e)·P = (v·M + e)·P` — the eye slides sideways in view space. Because rows
0–2 of T are identity it reduces to **"add e × row0 to row3"**, four multiply-adds.

Applied to viewport **A only** (the near-plane one), with `DK_STEREO_EYE=0.5`, during live gameplay:

```
600x  P00=1.07111  near=1.80   row3 = [0.53556 0.00000 -1.80065 0.00000]   SHIFTED
      row3[0]/P00 = 0.50000  <- exactly the offset asked for
600x  P00=0.75000  near=4.00   row3 = [0.00000 0.00000 -4.00783 0.00000]   untouched
```

`[verified-live 2026-09-18, n=1 run, 600 applications]`

- **Exactly the requested offset, applied exactly once.** Checked for drift across 600 applications:
  none. The function re-copies the viewport's projection each call, so the addition does not
  compound — this was checked explicitly, because a compounding offset would look identical to a
  working one for the first second and then destroy the picture.
- **Only the chosen viewport moves.** The other is bit-identical to the unshifted control.
- **The game renders normally at 0.5** — screenshot `2026-09-18-stereo-eye-offset-live-in-gameplay.png`
  against the no-offset control `2026-09-18-stereo-control-no-offset.png`, same checkpoint.
- **At a deliberately absurd offset (3 and 50) the 3D content distorts violently while the 2D
  overlay — text, buttons, icons — stays exactly where it is** `[verified-live 2026-09-18, n=2]`.
  That is the clearest single demonstration that the hook reaches 3D geometry and nothing else.

## What is NOT established

- **Which of the two viewports the player actually looks through has not been confirmed by eye.**
  The near-plane one carries the scene's varying FOV and distorts visibly when over-shifted, so it
  is very probably the world camera `[hypothesis]`. The clean way to settle it: shift one viewport
  hard while standing still and photograph what moves.
- **No left/right pair has been produced.** One eye has been offset. Alternating `e = ±IPD/2` by
  frame is the next change and it is small — but which alternation scheme to use is the
  `[DECIDE]` row, and it is a foundation decision.
- **The correct scale of `e` is unknown.** 0.5 view-space units renders cleanly; whether that is a
  plausible human eye separation in this engine's units has not been worked out. The near plane is
  1.80, which is a clue and not an answer.
- **Nothing has been seen in a headset.** This is all flat-screen evidence.
- ⚠️ **Culling still uses the unshifted frustum** (dossier §6), so objects at the very edge of view
  may pop in one eye. Not observed yet, not looked for.

## One thing that cost three runs, worth writing down

**The menu drive was on a fixed clock and the game's boot time varies by tens of seconds** between
runs, depending on shader-cache state. When it drifts, every button press lands on the wrong screen
and the run silently produces nothing — which looks exactly like "the pad route is broken". Two runs
were lost to it and one to a *"Starting a new game will delete all your saves"* prompt that only
appears once a save exists.

The fix in the harness: wait for the title art to appear, then **alternate START and A four times**
rather than following a schedule. A on the title does nothing, START on the menu does nothing, and a
spare A on the menu is CONTINUE — which is where we wanted to end up anyway.
