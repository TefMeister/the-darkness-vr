# 2026-11-06: the hurt screen off in VR (b016, built, not run)

/pd, home PC, Opus, static only. **The game was not launched and nothing here has been run.**

## What the hurt screen hangs on

- `sub_82125088` (r3 = a player object; its client data at +396) updates the hurt amount every tick:
  `+1716 = +1712; +1712 += rate * (target - +1712)`, target from health `[inferred-static 2026-11-06]`.
- Every reader of clientdata+1712 in the program (scan of the generated code, `lfs fN,1712(`):
  `sub_821570E8` (the client screen pass, 3 sites), `sub_821C5C00`, plus the updater itself and a copy in
  `sub_821A4820`. Both screen readers lerp +1716 -> +1712 by +40 and draw their hurt passes only when the
  result is above 0.0001 (float at 0x8209DDDC) `[inferred-static 2026-11-06]`.
- Every writer (`stfs fN,1712(`): the updater, the respawn reset `sub_821A38C0` (writes 0.0, float at
  0x8209DCBC), the copy in `sub_821A4820` (and a stack slot in `sub_8218CE50`, unrelated) `[inferred-static]`.
- `CL_HURTEFFECT` (the registry value the dossier guessed at) is written by `sub_823FDFD0` (the `hurteffect(n)`
  script command) and no code reads the string: it is not the switch `[inferred-static 2026-11-06]`.

## What b016 does

`vr_probe_screenfx.inc`: a hook on `sub_82125088` runs the original, then stores 0.0 at +1712 and +1716 when
`DK_VR_SCREEN_FX_OFF=1` (already set by the hands launch scripts) or the bridge var `fx.hurt_off` = 1.
`fx.hurt_off` = 0 gives the game's own back, live. `fx.hurt_seen` shows the amount the game set on the last
update before it was zeroed, and the log prints `[VP] hurt screen: ... amount=` up to 5 times when hurt.
`[compile-verified 2026-11-06]`; exe 05fcd13c1bd8; folder `C:/NonSteam/the-darkness/build-home-xr-b016`
(b015 with the new exe); builds repo `headset/v0.1.0-b016 - Hurt screen off`.

## Not established

- That those passes are the red tint / blur Tefa sees `[hypothesis]`.
- That sub_82125088 runs for the local player every tick: it is the only thing raising the value, so if the
  hurt screen ever shows, it ran.
- Damage breathing sounds (`DamageBreath1-3`) are separate and stay.

## The check (flat or pretend headset, one launch)

Take damage with b016 (`dk_vr_sim_hands.ps1`), then read the log:
- `[VP] hurt screen: ... amount=0.0xxx off=1` and no red / blur on screen: works.
- that line, and red / blur still on screen: those passes are not all of it; flip `fx.hurt_off` 0 / 1 and compare.
- no such line while hurt: the hook is not on the player's update; the derivation is wrong, not a knob.
