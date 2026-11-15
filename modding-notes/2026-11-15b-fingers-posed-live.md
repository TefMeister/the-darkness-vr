# 2026-11-15 (/lm, Opus, simulator, nobody in the headset): fingers posed live; the Hand Poser link (port b033)

Tefa: *"1 yes 2 go"* (yes to `2026-11-15-hand-poser-plan.md`; go for game sessions). Steps 1-3 of the plan done; step 4
(posing with Tefa in the headset) is next.

## Step 1: the skeleton (static, the session's reader; dossier §6i)

The player body `j_e_noguns.xmd` (inside `GameContext_Create.XDF`, one zlib stream) has 293 bones with parents and no
names; bones 0-67 are the shared human skeleton. Each hand bone has five 3-joint fingers: bones 38-52 hang from 16,
53-67 from 21. The skeleton instance keeps LOCAL matrices at `*(SI+32)` and composed ones at `*(SI+36)`; the game's
composer `sub_82420678(skel, start bone, &parent world, SI)` re-builds a bone and all below it.

**Live, the same day:** the port read the live skeleton: 293 bones, every hand parent as in the file's table
`[verified-live 2026-11-15, simulator, n=1]`.

## Step 2: the port speaks Hand Poser's files (`vr_hand_poser_link.cpp`, drafted by the reader, wired in here)

`reframework/data/hand_poser/` beside `darknessrecomp.exe`: `state.json` out (game "the-darkness", "Jackie", the
skeleton, live angles), `command.json` in. After the game's IK step (the `sub_821BF988` hook), each held joint's local
turn is written and re-composed with the game's own composer. `DK_POSER=1` / bridge `poser.on`.

Simulator, Chinatown, VR Bench holding the pretend controllers in front of the face `[verified-live 2026-11-15,
simulator, n=1 each]`:
- a held knuckle reads back exactly (40, 0, 0) and the next joint is carried along (its turn as seen from the hand
  went 20 -> 60 degrees);
- holding a WRIST at 90 degrees turned the drawn hand; four knuckles at 60 degrees turned the four drawn fingers as
  whole fingers: **what we write after the IK step reaches the picture**, and nothing snapped back in the frames seen;
- **the curl is about X** (Y twists the fingers flat and open); which sign closes the hand on each side: not settled;
- **bone 16 is the RIGHT hand** (the hand on the right controller): the port has driven that chain with the right
  controller since 2026-10-29 (`ik.right_chain = 1`, "0 crossed the hands in the headset"), and the wrist test agreed.
  The reader's first labels (from the 2026-10-28 reading) were the other way; the link's names are fixed
  (16 / 38-52 = `R_`, 21 / 53-67 = `L_`). Dossier §6i's table carries the old labels: read L and R swapped there.

## Step 3: Hand Poser knows The Darkness (hand-poser-vr, installed on this PC)

A folder holding `darknessrecomp.exe` is a game with no script: the script button is hidden for it. Self-test all
pass. ⚠️ The window remembers the last game's folder (Village on this PC): pick The Darkness's build folder once with
GAME FOLDER... (after the game has run once with the link on, so the folder has `reframework\`).

## Builds

- Port b033 = `C:/NonSteam/the-darkness/build-home-xr-b033` (DK_POSER=1 in its scripts), the newest folder, so the
  desktop icon starts it. The build history calls it b034 (its numbers run one ahead of the port's from here).
- Test copy `build-test-b033` (b032's files + the first b033 exe, before the name fix).

## Not established

- The sign of the curl per hand, and how the window's X / Y / Z sliders feel against Village's (the axes are the
  body's, not each finger's own).
- Steadiness over minutes and in motion (only still frames seen); in a cutscene or a weapon change.
- One gun in the right hand: the dual-wield flag was disproved on 2026-11-14 and the pair is one weapon in the data;
  what is left is hiding the left pistol's model: engine work nobody has written down: `MODEL: FABLE`.
