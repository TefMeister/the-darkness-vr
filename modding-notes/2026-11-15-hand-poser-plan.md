# 2026-11-15 (/pd, Opus, no game run): a Hand Poser for The Darkness, the plan

Tefa 2026-11-12: *"a hand poser for the darkness, like re games we have to start that too"*. This is the short plan the
board row asks for: what Hand Poser needs from a game, against what this port can give. **Nothing built; a plan for
Tefa's yes.** Read: hand-poser-vr's README and `app/HandPoser.cs`, this repo's dossier §6e, `vr_hand_pose.{h,cpp}`.

## What Hand Poser needs (read from its code, 2026-11-15)

- A folder `<game folder>\reframework\data\hand_poser\` with two files. The window writes **`command.json`** (which joints
  to hold, at what local X / Y / Z angles in degrees; which joints to read) and reads **`state.json`** (the game's name,
  the character, the whole skeleton as joint names with their parents, the live angles of the joints it asked about),
  written about once a second; stale after 3 s.
- The window finds a game as a running process with a `reframework` folder beside its exe. The RE games put a Lua script
  there; **The Darkness needs no script**: our own C++ in the port reads and writes the two files.
- Joint names: the finger groups ("one slider bends a whole finger") come from the names (`JointNames.cs`: Village's
  `L_IndexF1`, RE2's `hand_index_0`). Our bones have numbers only, so the port names them itself.

## What the port can give (dossier §6e)

- The bone palette: 4x3 per bone, written by `sub_8224AB18` in the pre-draw commit from a per-draw descriptor (64-byte
  4x4 matrices, a remap, a count), bone-to-world `[verified-live 2026-10-28]`. The hand chains are known (A = right hand,
  bones 18-19-21, grip 23; B = left, from bone 16), and `vr_hand_pose.cpp` already turns whole hands from the animation.
- ⚠️ **Not known yet: which bones are the fingers, and the bones' parents.** The palette holds each bone already placed
  in the world, so turning a finger's first joint does NOT carry its tip along by itself: either the hook sits where the
  skeleton instance still has LOCAL turns (before they are composed), or the port carries the children itself, which
  needs the parent of every bone.

## The plan, cheapest first

1. **[FLAT, session alone, simulator] Find the finger bones and their parents.** Static first: the skeleton's node table
   (`CXR_Skeleton`, `NODES` read by `sub_82422978`) may hold a parent index per node; then confirm live by nudging one
   bone at a time (`DK_SKIN_BONE`, the b003 lever) on a still frame and seeing which part of the hand moves. Out: a
   table "bone N = right index finger, joint 1; parent M".
2. **[PD] The port speaks Hand Poser's two files** (new `vr_hand_poser_link.cpp`, beside `vr_hand_pose.cpp`): writes
   `state.json` once a second (game "the-darkness", the hand bones with names in Village's style, `R_IndexF1` ..., so the
   window's finger groups work unchanged, and their parents), reads `command.json` every few frames, and turns each held
   bone by the held angles in the same hook that already turns the hands, children carried along by the table of step 1.
   Creates `reframework\data\hand_poser\` beside `darknessrecomp.exe` so the window finds it.
3. **[PD] Hand Poser: The Darkness by name** (app side, small): the top line says *the-darkness · Jack · connected*; no
   "put the script into the game" button for it.
4. **[VR USER] Pose with Tefa:** Hand Freeze holding the controllers, Window View showing the poser in the headset, the
   same way as Village. The poses saved per gun (the port already keeps one pose set per held gun, `pose.set`).

**The fork in the road:** if step 1 finds the skeleton's LOCAL turns live (before composing), step 2 writes there and
the children follow by themselves (no parent table needed): fewer moving parts. If only the composed palette can be
reached, the port carries the children: more code, still Opus. `MODEL: OPUS` for all four; Fable only if the bone
writes resist (the board row's own words).
