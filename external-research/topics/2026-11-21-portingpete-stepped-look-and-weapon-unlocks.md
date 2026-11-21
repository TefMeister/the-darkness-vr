# portingpete's port, main branch 2026-11-13 to 11-18: a skipped-turn fault at high frame rates, a camera jump after wall contact, and an "Unlock All Weapons" switch

**Status:** 🆕 new · **Priority:** medium-high. Follows the topic of 2026-11-14 (v0.1.10). No new release since
v0.1.10 (2026-11-12, pre-release); these are merged pull requests on `main`. Nothing here is VR work, but two of
them are the game's own timing and camera code, which our port runs too.

## What is public

Read online on 2026-11-21 from the repository viewer (commit list, pull requests #173, #175, #179). Nothing was
downloaded, nothing copied. All `[reported 2026-11-21]` unless tagged otherwise.

- **Skipped look updates at high frame rates (PR #175, merged 2026-11-17, refs their issue #34 "camera stutter"):**
  the game drives two updates from one clock: a periodic one that asks for at least 50 ms between calls, and the
  normal player-camera update that asks for about 8.33 ms (1/120 s). At a 120 frames-a-second cap the time since
  the last camera update can fall just under 1/120 s, the game's strict comparison then skips that turn, and the
  skipped turn is applied later as one larger step: uneven camera motion although frames arrive evenly. Their fix
  lowers the minimum for the normal player-camera update only (scaled to the clock); the 50 ms update, scripted
  cameras, other camera modes and menus keep the original threshold; an environment switch restores the original.
  Measured by them at 720p, 120 cap, one turn each way: repeated (unchanged) sampled camera angles fell from
  277 of 689 intervals to 21 of 690, and from 287 of 689 to 16 of 688; turn speed unchanged (about 211 degrees a
  second). The page names no guest function or address.
- **Camera jump after touching a wall (PR #173, merged 2026-11-17):** after the camera met a wall or corner its
  offset was cleared by the outer camera, so it lagged and then jumped when the contact ended; the fix keeps the
  previous offset so the return is gradual over the game's own grace period. Same PR: several absolute stick
  vectors with the same timestamp no longer fill the retail command packet and spill into later ticks (only the
  last vector is kept). Their measurement is indicative only (different manual runs): the largest eye step at 22
  wall releases 1.661 units, against 9.366 in an earlier failed capture. Also adds native DualSense support.
- **"Unlock All Weapons" and other cheats in their F5 panel (PR #179, merged 2026-11-18, issue #172):** grants
  14 ordinary campaign guns, both Darkness guns and the ability to use them, and reports how many weapons the
  current area allows (Pier 19: 16 of 16; Fulton Station: 1 of 16). The same panel adds infinite ammo (keeps the
  loaded rounds after a shot; an empty gun still needs a reload, through a hook on an original ammo routine),
  "enemies ignore player" and "break nearby lights". The page does not say which game keys, functions or
  inventory data it uses.
- Also on main: softer Darkness eye glow near the camera (#169, 11-14), a Proton shader-compiler fix (#176).
- Estacado (the other public port): no commit since 2026-11-06 `[measured 2026-11-21, commit list]`.
- ReXGlue SDK: **v0.11.0 is now the stable release (2026-11-14)**; its notes list one codegen fix (fctid, srad /
  sraw, vcmpbfp edge cases, #566 / #577). Nightlies of 11-16 and 11-17 are build and module changes. The quit
  heap-corruption issue #525 is still open with no reply and no linked fix (checked 2026-11-21).

## What it means for us

- **The skipped-turn fault is the "jittery" kind Tefa means (stepped, not shaky), and it gets worse the faster the
  game runs.** Our b025 and later run the alley at 112-114 game frames a second and up to 180 elsewhere
  `[measured 2026-11-11]`, right where a 1/120 s minimum starts rejecting updates `[hypothesis]`. In the headset
  the head pose is ours, not the game's, so the head view should not step; what can step is anything the game's
  camera update still drives: right-stick turning, and the aim or body direction our hands and head-walking read
  from the game camera `[hypothesis]`. Cheap check on the simulator: hold the right stick, log the game camera's
  yaw per game frame, count unchanged frames at the game's free rate against a capped rate.
- **The stick-vector spill** bears on head walking (`ctl.head_walk` rewrites the left stick every frame): if our
  rewritten vectors arrive faster than the game's command packet drains them, movement can lag by ticks
  `[hypothesis]`.
- **Unlock All Weapons** is the standing "unlock everything first" route for this game, and the area count (Pier 19
  takes 16) says where to test one gun at a time for `[ID DRK-004]` / `[ID DRK-015]`. We have the developer menu
  already (dossier §3, §6c); their panel proves the grant works through the game's own inventory, so ours can
  be written fresh against the same inventory search (`sub_821B0DC8`, board row on the held weapon) `[hypothesis]`.
- SDK v0.11.0 does not change the "do not rebase mid-wear" advice of 2026-11-11 (every SDK patch would need
  re-making); #525's app-side order is still our own fix to make.

## Next step (modding side)

1. Simulator, session alone: right stick held, game camera yaw logged per game frame, free rate against a 90 cap;
   many unchanged frames at the free rate = the same fault, and the cure is ours to write.
2. When the one-gun work resumes: give every weapon through our developer menu at Pier 19, then switch through
   them with the thumb-rest pick.

## Sources

- portingpete, The Darkness Recomp, commit list of `main` (2026-11-13 to 11-18): https://github.com/portingpete/The-Darkness-Recomp/commits/main
- PR #175 (skipped look updates): https://github.com/portingpete/The-Darkness-Recomp/pull/175
- PR #173 (wall-contact camera jump, stick-vector packing, DualSense): https://github.com/portingpete/The-Darkness-Recomp/pull/173
- PR #179 (F5 cheats and weapon unlocks): https://github.com/portingpete/The-Darkness-Recomp/pull/179
- ReXGlue SDK releases (v0.11.0, nightlies to 11-17): https://github.com/rexglue/rexglue-sdk/releases
- ReXGlue issue #525 (still open): https://github.com/rexglue/rexglue-sdk/issues/525
- Estacado (checked, no change): https://github.com/invinceble55-wq/Estacado-The-Darkness-Recomp
