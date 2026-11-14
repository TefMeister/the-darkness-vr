# The Darkness VR: Quest controls and gun handling (design, 2026-11-14)

Status: APPROVED by Tefa 2026-11-14, with the work order in section 6. Worked out with Tefa in one planning conversation on 2026-11-14 (Developer PC, Opus,
no game running). Everything marked **(Tefa)** is Tefa's decision; everything marked **(call)** is mine, open to
correction.

## 1. Why

- Tefa, 2026-11-07, after the first wear with the powers: *"the Darkness powers look epic in VR, just need to figure
  out the controls"*. The build today only lays the Quest buttons over an Xbox pad (b009 map), which is not a VR
  control scheme.
- Tefa does not want the game's always-on dual pistols in VR: one gun in the right hand, Resident Evil style, with the
  reloads being built for Village; a second gun only when one is picked up from the world (Tefa).
- The base layout below is also the layout Tefa wants for the RE games (Tefa, 2026-11-14), so players of our Visceral
  mods learn one layout. Not carried to the RE games yet; a row on each RE board.

## 2. Goals, non-goals, anti-goals

- Goals: every action in the game reachable from the Quest controllers; guns as physical objects (holstered, held,
  dropped, thrown, picked up again); the Darkness' own controls while it is out.
- Non-goals: the flat game's controls; multiplayer; changing the powers themselves.
- Anti-goals (would be a failure even if it works):
  - the game's two pistols showing whenever pistols are held;
  - thumb rests used for anything common (Tefa: last resort or rare things only; the gun / power picker is the
    one agreed exception);
  - a gun that vanishes when thrown or dropped;
  - a casual toss that staggers an enemy;
  - the Darkness power firing while the left hand shoots a gun.

## 3. The layout (settled with Tefa)

**Always on (also the RE layout):**
- left stick: walk, in the direction the head looks (built, b020)
- right stick left / right: turn, smooth as today
- right stick up held 0.3 s: jump
- right stick down held 0.3 s: crouch
- left stick direction + right stick click: dodge
- A: use
- menu tap: journal (objectives); menu held 0.3 s: pause menu
- Y: the Darkness out / in
- left thumb on its rest + right stick direction: the D-pad: picks a gun, or a power while the Darkness is out;
  while the thumb rests there the right stick does not turn, jump or crouch (Tefa)

**Right hand (one gun, RE style):**
- right trigger: fire
- B tap: drop the magazine; the left grip docked on it takes it out; let go in one of the holster spots to "put it
  away" (purely cosmetic, Tefa). Whether Village's two-magazines-in-the-hand swap can work here: to be seen (Tefa)
- reload from the pouch exactly like Village (the rules, not the code: Village is RE Engine)
- B held 1 s: drop the gun in the right hand
- right grip at a holster with a gun in it: put the held gun away and take that one, in one move
- right grip at an EMPTY holster with a gun in hand: put it away; the hand stays empty
- right grip held + a throwing motion + let go mid-throw: throw the gun, full or empty
- empty right hand + right grip on a gun in the world: pick it up (a press is enough, no hold) (Tefa)

**Left hand (a second gun only from the world):**
- left grip held on a gun in the world: it comes into the left hand with whatever rounds it has (Tefa)
- left grip held + left trigger: fire it; no reload (Tefa)
- let go of the grip at an empty holster: it is stored there; anywhere else: it drops (Tefa)
- throwing motion + let go: thrown; only a strong throw staggers an enemy, a casual toss does not; no damage (Tefa)
- left grip at the pouch: take a magazine (the reload)
- with a gun in the left hand the right gun cannot be reloaded: drop, throw or holster the left one first

**The Darkness out:**
- left trigger: use the power (tap or hold, no delay: the Demon Arm charges as the game intends)
- X: send the darkling where the right hand points
- A: devour a heart
- left grip + left trigger with a gun in the left hand: fires the gun, never the power (Tefa)

**Picking up:** A never picks up guns (Tefa). Gravity-gloves style pick-up from a reasonable distance (Tefa): point the
hand at a gun, grip, flick, it flies to the hand.

## 4. Guns are objects (Tefa)

- A thrown or dropped gun stays where it falls, keeps its round count, and can be picked up again.
- Jackie can carry five guns: four in holsters and one in the right hand. A free right hand means dropping, throwing
  or holstering the held gun.
- **(call)** The six spots as in the RE games (both hips, both shoulders, back left, back right): map at the left back
  (Tefa), the pouch at the left hip (as Village), guns in the other four (right hip, both shoulders, right back).
- **(call)** Guns on the ground last until the level changes (the game unloads the level); guns in holsters and in
  hand carry over.

## 5. What is known, and what is not (constraints)

- The game is our recompiled Xbox 360 port; it reads only an Xbox pad. Our pad layer (`BridgePadDriver`,
  `port-project/src/vr_bridge.cpp`, SDK patch 11) turns the OpenXR controllers into pad presses.
- The game's own actions, from its text files: jump, crouch (and a crouch toggle), use, journal, the Darkness (power),
  drain, darkness-mode, cycle weapon, D-pad up / down / left / right for powers and weapons, darkling orders "to where
  you are aiming", devour `[measured 2026-11-14, StringTable_Eng.xcr, names only]`. Which pad button each is: not yet
  read (the DevMenu has CUSTOMIZE CONTROLS; the binding table is somewhere in the game data).
- No hint text for a dodge, a weapon drop or a map `[inferred-static, text only, not checked in play]`: likely new.
- The held weapon: ClientData + 0x1BE8 (0 empty hands, 6 the two pistols) `[measured 2026-11-10, n=2]`; the rest of
  the inventory, the ammo counts, world pickups: not found.
- The game aims from the camera; guns follow the game's aim, not the controller (board row "aiming straight").
- The thumb rest touch is not in our controller bindings yet (SDK patch 11 has none); whether Virtual Desktop passes it
  is unchecked. Fallback: left grip + right stick.
- Whether the Darkness is out: no state read yet; the mode switch needs one.

## 6. The jobs, in order (Tefa agreed the order of 1-6; 7 added 2026-11-14)

| # | Job | How | Unknowns | Model |
|---|-----|-----|----------|-------|
| 1 | Buttons and holds (section 3, "always on" + the Darkness-out buttons) | **pad presses (call; Tefa handed the how)**: each gesture becomes the game's own pad button in `BridgePadDriver` | the game's action-to-button table; a "Darkness out" state; the thumb rest binding | OPUS |
| 2 | Holsters and the map spot | the RE spot positions; a holster picks one exact gun: pad cycling first, a direct write if cycling is too slow or visible | the weapon list in memory | OPUS |
| 3 | Aim from the hand (guns and the darkling order) | move the game's aim ray to the right controller | where the aim is taken | FABLE |
| 4 | Guns as objects: one gun, the left-hand world gun, drop / throw / stagger, pick up again, five guns | direct game calls (spawn a pickup with a round count, hide the second pistol, a stagger on hit) | nearly all of it | FABLE |
| 5 | Reload by hand (B, the magazine put away in a spot, the pouch) | Village's rules on this game's pistols | whether the game's reload animation can be split | OPUS, FABLE if it resists |
| 6 | Stroking the Darkness heads so they react | contact between the hand and the heads' bones | everything | FABLE, last |
| 7 | Gravity-gloves pick-up | a ray from the hand, a highlight, a flick pulls the gun | needs 4 (guns as objects) | OPUS |

**Order of work, Tefa 2026-11-14 (approves the design):** *"let's build the game around the one handed shooting and
reloading mechanics first, and see if the guns as real game world objects even is a thing. so please do things that
you think that can be done first, we'll deal with the unknown after."* So:
1. job 1, the buttons (doable now);
2. one gun in the right hand (the game's second pistol hidden and silent): first find out whether it can be done;
3. job 5, reload by hand, on that one gun;
4. a look (no building) at whether guns can be world objects: inventory, round counts, world pickups, dropping;
5. the rest after, in the table's order.

## 7. Approaches considered (job 1)

1. **Pad presses (picked, call).** Gestures become presses of the game's own pad; the game's rules do the rest. Cheap,
   safe, testable on the pretend headset. Limit: only what the pad can do.
2. **Direct game calls.** Precise, but every action needs digging first. Kept for the things the pad cannot do (an
   exact gun from a holster, the dodge, the drop, the throw).

## 8. Left to the builder (calls made here, correct me)

- Jump / crouch fire when the 0.3 s is reached, not on release; "up" and "down" mean within about 35 degrees of
  straight and past 70 % of the stick's travel, so turning never jumps.
- Crouch is the game's own crouch toggle: hold down 0.3 s to crouch, again to stand.
- Menu: let go before 0.3 s = journal; reaching 0.3 s = pause.
- Until job 3, X sends the darkling where the head looks (the game's aim).
- The dodge, the drop and the map have no pad button in the game: they wait for jobs 2 and 4 unless the button table
  shows one.
