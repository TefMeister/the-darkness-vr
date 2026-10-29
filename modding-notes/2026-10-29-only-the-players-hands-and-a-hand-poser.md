# 2026-10-29 (morning) — only the player's hands, and a hand poser (home PC, Opus, OpenXR simulator, no headset)

## The questions (Tefa, after wearing b009 at 01:15)
*"can you see why in The Darkness, the player hands are twisted, is there a way to create a hand poser and a gun
poser for the game and why my hands move others hands as well? continue should start the game from a save game now"*

## Why the hands were twisted (read from the code and last night's log)
The hands' turn was lined up by itself at the first solve after launch: `O = H * C^T`, H = that moment's animation
rows, C = the controller then. Two things were wrong with that moment `[inferred-static, matches the 01:09 log]`:
- the controllers were held however they happened to be held one second after launch (01:09:28, while the headset
  went on), so the hand kept that arbitrary twist for the rest of the run;
- every character's hand IK was fed, and the line-up was taken from whichever object solved FIRST, so the player's
  hands could be lined up against another character's hand.
The maths after the line-up is consistent (positions and turns go through the same camera mapping; the mapping is a
proper rotation, `det[right, up, -fwd] = +1`, so no mirror sneaks in).

## Why your hands moved the others' hands
Every object that runs the game's explicit-target hand IK got our target (six in the car scene, six at the save
spot). There was a switch to pick one (`ik.only`) but nothing picked it.

## What was built (exe f4b2a66f3474 -> 0b8f27964993, installed in `build-home-xr`)
- **Only the player's hands** (`vr_hand_ik.cpp`, `ik.pick=1` default): for every IK object, the distance from the
  camera to the middle of its shoulders (bones 18 and 13 from `*(SI+36)`, SI = r5 at `sub_821BF988`); the nearest
  within 30 units is the player; nobody that near = nobody is fed (a camera outside anyone's head).
  - At the CONTINUE save spot: six objects, distances 24.5, 24.5, 42.5, 119.3, 168.7, 252.2 `[measured 2026-10-29, n=2 runs]`.
  - ⚠️ **The player has TWO IK objects on one skeleton** (both 24.5, same skeleton); fed alone, one moves the
    arms on screen (mean picture change 8.56), the other does not (0.04) `[verified-live 2026-10-29, simulator, n=1]`.
    So everything as near as the nearest (within 1 unit) is fed. With that, the hands move (10.74) `[verified-live
    2026-10-29, simulator, n=1]`.
  - Not seen yet: the car scene with Jackie's two passengers (it needs NEW GAME, which deletes the save). By
    construction the passengers are further than the player and are not fed `[hypothesis until seen]`.
  - The seen-list now makes room for new objects (a level change used to fill it for good).
- **The hand poser** (`vr_hand_pose.cpp`, new): the pose per hand is kept in `dk_hand_pose.txt` beside the game,
  loaded at launch; no line-up happens by itself any more (no file = the hands keep the game's own angle).
  - Headset: hold **B + Y for 1 s** = pose mode (hands show the game's angle, the game gets no controller input);
    **pull a trigger** = that hand lined up; **that hand's stick** turns it (left/right turn, up/down tilt; grip
    held: roll), 45 degrees a second at full push; **B + Y 1 s** again = off and saved.
  - Desk (bridge): `pose.on`, `pose.lineup` (1 left, 2 right, 3 both), `pose.hand` + `pose.yaw/pitch/roll` (degrees,
    once), `pose.save`, `pose.text`; `ik.calib=1` is now `pose.lineup=3`.
  - Simulator checks `[verified-live 2026-10-29, simulator, n=1 each]`: the pose loads at launch; pose mode on from
    the desk; a simulated trigger pull lines the right hand up (log line); pose mode off saves; `pose.yaw=70` turns
    the hand on screen and `-70` brings back the same picture; `pose.roll=90` rolls it.
  - ⚠️ **Not tested: the B + Y hold** (the simulator only has a trigger and A), the sticks, and anything in the
    real headset. The test pose file from the simulator was set aside
    (`_backup-2026-10-29-b008-before-poser/dk_hand_pose_simulator-test.txt`), so Tefa starts clean.
- **Gun poser:** the same tool already keeps four pose sets (`pose.set`, 0 = empty hands); a gun needs its own set
  chosen automatically when that gun is in hand, and the game's "which gun is held" is not found yet (the reader saw
  `Weapon handle` / `P6_inv_hand02` strings, `sub_821B0DC8`). A `[PD]` row.
- **CONTINUE**: the launch route (START, then A until a level camera) already lands on CONTINUE, and now loads Tefa's
  save: in play at about 85 s, three runs this morning.
- Tool: `staging/the-darkness-vr/xr-sim-2026-10-28/dk_ik.py` sets/reads bridge variables, moves a simulated
  controller, and takes pictures, in one ordered line.
- Build bookkeeping: the b009 the headset wore last night was never saved as a build; saved this morning as builds-repo
  **b008** (the builds repo had no b008). The poser build is b009 in the builds repo.

## Next
1. Headset: line both hands up with the poser (B + Y, trigger), say whether they now turn the right way.
2. `[PD]` move in the direction the head looks (from the board).
3. `[PD]` find which gun is held, so the poser's sets switch by themselves (the gun poser).
