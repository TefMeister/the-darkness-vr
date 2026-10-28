# The game's hand IK: solver, target, and where to feed a controller pose

From: the background reader of the 2026-10-28 live session (static reading only; nothing launched or attached).
For: dossier section 6e. Everything below was read from the recompiled code under
`generated/default/` on the home PC, so it is `[inferred-static 2026-10-28]` unless tagged otherwise.

## Short answer

- **The solver is `sub_821EE4E0`**: one generic two-bone IK routine. Arguments: r3 = the solver block at
  `IK+32`, r4 = skeleton (`CXR_Skeleton`), r5 = skeleton instance, **r6 = pointer to the TARGET, a 64-byte 4x4
  matrix** (rotation rows at +0/+16/+32, position at +48), r7 = game object, r8 = character, r9 = optional
  vec3 offset (a sway offset; null in some callers), r10 = 0. The byte at `IK+40` (`r3+8`) picks the chain.
- **The game already has a "put this hand HERE" setter: `sub_821BF880`.** It writes a world point for each hand
  to `IK+1864` (chain A) and `IK+1892` (chain B) and raises a flag; `sub_821BF988` then turns each point into a
  target matrix and runs the solver, and when it does, the regular animation-driven hand IK is skipped.
- **Best hook for a controller (position only):** hook `sub_821BF988` on entry (r3 = the IK object), write the
  controller point to `IK+1864` / `IK+1892` and set the flags in `IK+0`. **For position AND rotation:** hook
  `sub_821EE4E0` on entry and overwrite the 64-byte matrix at r6 when the chain byte `*(u8*)(r3+8)` is 0 or 1.

## The switch bits (correction to 6e)

`sub_821B22E0` packs the three registry answers into `IK+1856` like this (read from the `rlwimi` masks):
`DISABLE_CHAR_DARKLING_IK` -> 0x80000000, `DISABLE_CHAR_PLAYER_IK` -> 0x40000000, `DISABLE_HAND_IK` -> 0x20000000.
6e says "player IK into the top bit"; the top bit is the Darkling one. Missing key = 0 = IK on.
- 0x20000000 (hand) is tested once, in `sub_821CED18`, right before `bl sub_821B8300` (the hand IK step).
- 0x80000000 / 0x40000000 are tested in `sub_821BD498` (character IK) and `sub_821C1640`; `sub_821BD110` sets or
  clears both top bits together at run time. `sub_821C4C48` is a copy routine for the object.
- ⚠️ The hand switch does **not** gate `sub_821BF988` (the explicit-target path): that call comes first.

## The call chain (per character update)

`sub_821CED18` (big character/animation update; r31 = the IK object, r17 = skeleton, r26 = skeleton instance,
r21 = a game object, r27 = character) runs, in order:
1. sets or clears the explicit hand targets: `sub_821BF880(IK, &pointA, f1, f2, &pointB)` or
   `sub_821BF930(IK, 1, 1)` (clear). Two sources of points:
   - an object answers message id 13556 (0x34F4) through its vtable+84 with two vec3s (a `vec3[2]` built at
     sp+288 / sp+300): A and B. Only when `char+10300` has bit 0x02 and `char+392` lacks 0x00080000.
   - a second path (mode 2, `char+392` bit 0x00400000) sets only A, a point blended toward bone 23's position.
2. `sub_821BD498` (character IK, player/Darkling; legs and also arm chains 8/9).
3. `sub_821BF988` (explicit hand targets). If it returns non-zero, step 4 is skipped.
4. if `IK+1856 & 0x20000000` is clear: `sub_821B8300` (animation-driven hand IK).

## Chains (the byte at IK+40 -> bones, from the switch in `sub_821EE4E0`)

| byte | root, middle, end (+extra) | used by |
| --- | --- | --- |
| 0 | 18, 19, 21 (+23) | hand IK chain A (`sub_821B8300`, `sub_821BF988`) |
| 1 | 13, 14, 16 (+22) | hand IK chain B |
| 9 / 8 | 18-19-21 / 13-14-16, no extra | character IK (`sub_821BD498`) |
| 5, 4, 6, 7 | 2-3-4, 5-6-7, 92-3-4, 96-6-7 | character IK (legs) |
| 2, 3 | nothing | |

Bones 20 and 15 are skipped (forearm twist bones?) `[hypothesis]`. Which of A/B is the left hand is not known;
A gets the weapon/attach-point and the one-hand path, so A = right hand is a guess `[hypothesis]`.

## Where the target comes from

Layouts used: skeleton bone records are 48 bytes at `*(skel+12)`: +16 = the joint's local pivot (vec3), +44 =
parent index (u16). Skeleton instance: `*(SI+36)` = per-bone 4x4 matrices (64 bytes each) that the IK reads;
`*(SI+32)` = per-bone matrices the solver writes. The IK object keeps a hand state byte at +1448 (A) / +1450 (B):
0 off, 1 blending in, 2 blending out, 3 fully on; weights at +1452 / +1456; frame time at +1496.

- **Explicit path (`sub_821BF988`)**: target = rotation of bone 21 (A) or 16 (B) from `*(SI+36)`, position =
  `IK+1864` (A) or `IK+1892` (B). Flags in `IK+0`: 0x00100000 = A target on, 0x00400000 = A fading out,
  0x00200000 = B on, 0x00800000 = B fading out. `sub_821BF880` also writes IK+1876/1880 (= its f1) and 1884
  (= 0.75) for A, 1904/1908 (= 0) and 1912 (= 0.75) for B; meaning unknown.
- **Animation path (`sub_821B8300`)**: target = the animated matrix of bone 21 (A) or 16 (B), position moved to
  bone 23's (A) or 22's (B) pivot; or, when an animation layer at `char+9760` (A) / `char+9872` (B) resolves
  (`sub_821D4BC8`), to a point from that layer, e.g. a weapon's attach point on another model; then shaped by
  `sub_821BC7E8` (timed offsets) and `sub_821B98B8`. So it is **animation-driven every update, not an aim point**.
- **Reach / blend (`sub_821BC360`)**, both paths: blends the target position from the hand's current grip point
  (bone 23 or 22 pivot through bone 21 or 16) to the target at 5 x frame time per update with a smoothstep, then
  holds at state 3. So **the position in the target is where the grip point (bone 23 / 22) should land, not the
  wrist** `[inferred-static 2026-10-28; the solver's use of the extra bone not fully read]`.
- Sway offset passed as r9: `(IK+1344..1352) * (+-50) + (IK+1360..1368) * -100`, scaled by the weight. Zero it
  for VR.

## Hook recipes for the live session

1. **Position only, the game's own route:** hook `sub_821BF988` before the original. Write the controller grip
   point (same space as the `*(SI+36)` matrices) to `IK+1864` / `IK+1892`, set `IK+0 |= 0x00100000` (and/or
   `0x00200000`), clear 0x00400000 / 0x00800000. It has to be here, after step 1 clears the targets each update.
   The hand turns with the animation, only its position follows; the blend-in takes about 0.2 s once.
2. **Position and rotation:** additionally hook `sub_821EE4E0` before the original: when `*(u8*)(r3+8)` is 0 or
   1, overwrite the 64 bytes at r6 with the controller matrix (rows +0/+16/+32, position +48) and, if r9 is not
   null, zero the vec3 at r9.
3. **First live checks:** (a) which space `*(SI+36)` is in: compare bone 0's position there with the player's world
   position `[hypothesis: world space, since an object answers with raw points]`; (b) which hand chain A is: set
   only `IK+1864` and look; (c) whether the hand-IK bone numbers equal the palette index b at c96+3b (a mesh may
   remap bones) `[hypothesis]`.

## Bone names

None found `[measured 2026-10-28]`: the program image has no bone-name strings (only `P6_inv_hand01`,
`Weapon handle`, `P6_inv_hand02` at 0x82064DF8 / 0x82064E08 / 0x82064E18, read by `sub_821B0DC8` and
`sub_823A5800`, both reached only through vtables), `Anim/All.xac` (204 MB) has none, and a plain-text search of
`Content` + `System` for `Bip01`, `R_Hand`, `Hand_R`, `clavicle`, `forearm` found nothing. The code addresses
bones by fixed numbers. Constants at 0x82A4xxxx (e.g. 0x82A48E40, the smoothstep factor) read 0 in the unpacked
image, so they are filled at run time.
