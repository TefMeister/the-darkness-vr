# 2026-10-28 (evening) — the hand goes where we point (home PC, `/lm`, Fable, OpenXR simulator, no headset)

## The question
Motion-controlled hands, step (1) of the board plan: which bone is the hand, so step (3) can move it. Tefa: carry on
with the hand bone, third pass, on the simulator.

## What happened, in order

### 1. Nudging one bone at a time, picked live (build b003, exe fcbea77f2259)
`vr_skin_probe.cpp` grew bridge vars (`skin.bone`, `skin.mode`, `skin.dx/dy/dz`, `skin.dirty`, `skin.scan`): the hook on
the pre-draw commit rewrites that bone's three constants (c96 + 3b) in the shadow before the upload, so one launch
covers every bone (`bone_sweep.py`, 53 bones in about 90 s, a picture each). First sweep, car scene, the scene dead
still (noise 0.00 between two untouched pictures): the shift reached the screen for bones 1-14, 30-36 and 43-52, bone
48 most (mean change 3.38), and the thing that moved was the first-person sleeve-and-hand, big in the frame
`[measured 2026-10-28, n=1 sweep]`. The difference maps lit the whole arm for several bones, not one patch each.
Then, about three minutes later, the same nudge did NOTHING, for any bone, with or without the dirty mask set
(`bone_sweep2.py`, before/nudged/after pictures per bone; effect no larger than drift) `[measured 2026-10-28, n=2
re-runs]`. A bone-row scan (`skin.scan`) showed the table in the shadow at that moment belonged to some other
character (bone 0 at about (−2042, −1235, −225) world units, 150 units from the camera) and that slots 48-52 held my
own accumulated shifts (716,040 = 72,000 commits × 10), i.e. nothing of the game's had written them since.
**Why it stopped is explained by the reader's later finding** (below): the palette writer re-uploads only when the
draw's bone descriptor pointer changes (`RC+17192` cache), so what the shadow holds at commit time is not what the
arms are drawn with once the descriptor is cached `[hypothesis 2026-10-28]`, consistent, not re-tested. The shadow
route is parked: not needed any more.

### 2. The reader found the game's own hand IK, with an explicit target (static, two passes)
Full text: `engine-research/inbox/2026-10-28-reader-hand-ik-solver*.md` (folded into dossier §6e; verbatim copies in
`dev-archive/recon/2026-10-28-hand-ik-sim/`). The short form: `sub_821BF988(ik, skeleton, skel_instance, world, char)`
is the explicit-target hand IK; when the flag word at `ik+0` has `0x00100000` (chain A) or `0x00200000` (chain B)
it drives the two-bone arm so the grip bone lands on the vec3 at `ik+1864` / `ik+1892`, with the hand's rotation from
the animation. The game sets those with `sub_821BF880`. The IK object is `ClientData+4384`.

### 3. Feeding it a point of ours (build b004, exe 71f6cd0f8430) — IT WORKS
`vr_hand_ik.cpp` hooks `sub_821BF988` on entry and, when `ik.on` is set through the bridge, writes a camera-relative
point (newest camera rows from `dkxr::LastView`, one line added to the camera hook to keep them) into the target
slot(s), sets the flags, and stores the blend time and the constant as the game's own setter does. Results, car scene,
pictures through the bridge (`ik_test.py`, `ik_test2.py`; pictures in `C:/NonSteam/the-darkness/screens/xr-sim-2026-10-28/ik1`, `ik2`, game content, this PC only):
- **hand A on, target 24 forward / 8 right / 6 down of the camera: the first-person hand swings into view and reaches
  for that point; off again, it goes back** (mean picture change 9.11, then 0.83 after the blend back)
  `[verified-live 2026-10-28, simulator, n=1]`.
- hand B on: only the small hand/sleeve at the top of the frame changes (mean 1.00): chain B is the other hand, mostly
  out of view in this pose `[verified-live 2026-10-28, n=1]`.
- **The player's IK object is the one at 0xD9CD95B0 in this run**: fed alone it moves the big hand (9.32); each of the
  other five alone gives 1.1-2.2 (NPC hands, drift) `[verified-live 2026-10-28, n=1 per object]`. Addresses change per
  run; the reader's vtable test (`*(u32*)(ik-4384)`) is the durable way, not checked live yet.
- **The hand follows the target:** right −12 puts it at the lower left, up +10 brings it across the picture (35.5), forward
  40 reaches it out, 12/16/−12 nearly leaves the frame (6.4) `[verified-live 2026-10-28, n=1 per point]`.
So milestone (3) of the plan, "hand follows a point we choose", is done on the simulator through the game's own IK:
no bone index, no palette writing. What is still missing is the point itself (the controllers, step 2).

## Not established
- Nothing seen in the real headset; nothing with a real controller.
- The target's space is SI+36 space, world by every sign (camera-relative points land where expected), not proved
  separately `[hypothesis]`.
- Which chain is LEFT and which RIGHT (A reached in from the right side of the rolled picture; the roll makes it
  ambiguous).
- The hook runs on several game threads at once (the first log lines numbered two objects "#3"); the counters and
  the seen-list are not locked. Harmless for a test, to fix before it ships.
- The IK blends over `ik.time` (0.25 s here); the reader says state byte 3 at `ik+1448/1450` and weight 1.0 at
  `ik+1452/1456` skip the blend. Not tried.
- The shadow-nudge mystery (worked, then stopped) has a consistent explanation, not a re-test.

## Files
- `dev-archive/port-project/src/vr_hand_ik.cpp` (new), `vr_skin_probe.cpp` (bridge vars), `vr_xr_head.cpp/.h`
  (`NoteView`/`LastView`), `vr_viewport_probe.cpp` (one line), `CMakeLists.txt`.
- Scripts: `staging/the-darkness-vr/xr-sim-2026-10-28/` and copies in `dev-archive/recon/2026-10-28-hand-ik-sim/`.
- Builds b002 (skin probe), b003 (bone nudge), b004 (hand IK fed) in the builds repo; b004 installed in `build-home-xr`
  (previous exes in `_backup-2026-10-28-before-skin-bone/` and `_backup-2026-10-28-before-hand-ik/`), stamped.
- ⚠️ The repo's `port-project/src` still lacks `vr_bridge.cpp/.h` and its `main.cpp`/`darknessrecomp_app.h` are the
  pre-bridge ones; the build folder on this PC (`D:/the-darkness-build/src/DarknessRecomp/src`) is the one that builds.
  A `/pd` tidy row.

## Later the same evening (Opus): the motion controllers drive the hands (simulator)
Tefa: *"install the plugin update, then the controller sampling and other stuff you gotta do."*
- **SDK patch 10** (`dev-archive/sdk-patches/10-xr-hands.patch`, the reader's draft, applied as written to
  `D:/the-darkness-build/rexglue-sdk`): one OpenXR action set with a grip pose and an aim pose per hand, bound for the
  simple/Touch/Index/Vive/WMR profiles, and `rex_xr_sample_hands`, which syncs the actions and locates both grips in the
  same space and at the same predicted moment as the head sample. Built: `rexgpu-xenos.dll` exports it
  `[compile-verified 2026-10-28]`. On the simulator both controllers report tracked (flags `0x80000377`: synced, both
  grips orientation + position + tracked, both aims) `[verified-live 2026-10-28, n=1 run]`.
- **Game side:** `vr_xr_head.cpp` takes the controllers right after every head sample (`dkxr::LastHands`);
  `vr_hand_ik.cpp` `ik.src=1` turns each controller's grip position, relative to the head, into the head's own axes,
  lays it along the head-turned camera rows from the camera position, times 39.37 units per metre, and writes it into
  that hand's IK target. Also: `ik.vtable` (feed only one ClientData class), `ik.snap` (skip the blend, untried), the
  counters behind a mutex, and `ik.last` showing the raw head/controller/camera numbers.
- **Results** (builds b005/b006, exe fc0c426df3ac + rexgpu-xenos.dll 1d46e77a64d8, installed in `build-home-xr`, previous
  pair in `_backup-2026-10-28-before-controllers/`) `[verified-live 2026-10-28, simulator, n=1 per pose]`:
  - a controller 20 cm right, 15 cm below the eyes, 61 cm ahead lands on the same world point as the fixed test point
    of the first pass, to 0.1 unit, and the picture is the same: the right hand reaching there;
  - moving the RIGHT controller 15 cm left / up / down / 20 cm nearer moves the right hand the same way on screen
    (down takes it out of view); each move changes the point by the right distance (15 cm ~ 5.9 units);
  - the LEFT controller drives the left arm (chain B), which comes in from the left and follows up and across;
  - so `ik.right_chain = 0` is right: chain A (bones 18-19-21) is the RIGHT hand, chain B the LEFT.
- **A trap, in the simulator, not the game:** its `controller_pose_command.json` measures posY from head height (posY
  1.55 came back as 3.25 with the head at 1.70), which first put both hands 1.5 m above the eyes. Written into the
  game's `KNOWN.md` (control profiles repo) with a tool idea for the simulator's MCP server.
- The player check: the six IK objects' ClientData vtables were 0x82061FE0 (four) and 0x82064FD0 (two) in this run;
  the reader's guess of 0x82062190 for the player was not seen. Which 0x82061FE0 object is the player still needs
  `ik.only` (feeding all of them works for now).

## Not established (controllers)
- Nothing on the real Quest controllers or in the headset. The hand keeps the animation's ROTATION (only its position
  follows); the reader is working out how to feed the controller's rotation.
- The left arm came in very close to the camera (a big sleeve across the view): the arm's shoulder sits where the game
  puts it, not where a real shoulder would be; how that feels needs the headset.
- `ik.snap` untried; the 0.25 s blend is still in.
