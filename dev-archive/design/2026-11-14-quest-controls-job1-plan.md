# Job 1: the Quest buttons and holds — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (Tefa's standing rule: no subagents, work
> inline). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In a level, the Quest controllers work the game through Tefa's layout (design section 3, "always on" and
"the Darkness out"), as presses of the game's own pad.

**Architecture:** One new pure-logic unit, `QuestControls`, turns one sample of the Quest controllers (buttons, axes,
time, "in a level") into one pad state. `BridgePadDriver::GetDeviceState` calls it instead of today's `RemapXr` and
stick code. The SDK reports one new input, the left thumb rest touch, as bit `0x0400`.

**Tech Stack:** C++23, clang-cl, the ReXGlue SDK (patched), OpenXR.

**Spec:** `dev-archive/design/2026-11-14-quest-controls-design.md`

## Global Constraints

- The game's default pad layout (preset 1, its own controls page in `Content/Gui/CubeWnd.xcr`)
  `[inferred-static 2026-11-14, label positions on the controller picture]`:
  Back = journal, Start = pause, LT = secondary fire, RT = primary fire, LB = drain (Darkness mode), RB = the Darkness
  (use power), left stick = move, left stick click = crouch, right stick = look, right stick click = aim, Y = jump,
  A = use, B = reload, X = redirect (darklings), D-pad up / down = pick a power, D-pad left / right = pick a weapon.
- Quest raw bits from `rex_xr_sample_buttons`: A 0x1000, B 0x2000, X 0x4000, Y 0x8000, menu 0x0010, left stick click
  0x0040, right stick click 0x0080, left grip 0x0100, right grip 0x0200, **left thumb rest 0x0400 (new)**.
  Axes: [0] left trigger, [1] right trigger, [2,3] left stick, [4,5] right stick.
- Pad bits out (XInput): up 0x1, down 0x2, left 0x4, right 0x8, Start 0x10, Back 0x20, LS click 0x40, RS click 0x80,
  LB 0x100, RB 0x200, A 0x1000, B 0x2000, X 0x4000, Y 0x8000.
- Out of a level (menus, films, the journal) the map is the plain pad: A = A, B = B, X = X, Y = Y, menu tap / hold,
  sticks as they are, triggers as they are; grips and the thumb rest send nothing.
- `DK_VR_QUEST_CTL=0` in the start scripts gives back the b009 map unchanged (rollback). On by default.
- Numbers (one settings table in the new file): stick flick hold 300 ms, flick cone 35 degrees, flick reach 0.70,
  menu hold 300 ms, B hold 1000 ms, a pulse lasts 6 reads, trigger as bumper above 0.5, stick dead zone 0.15.
- Hand smoothing, the hand poser (B+Y 1 s) and head walking keep working as they are.

## Review Focus

1. Turning while the stick drifts up a little must never jump: only the 35-degree cone past 0.70 for 300 ms counts.
2. Letting go of the menu button before 300 ms gives exactly one journal press, never a pause as well.
3. A flick that is still held when the level ends (a film starts) must not leave a button stuck down.
4. With the thumb on the rest, pushing the right stick must not turn, jump or crouch, and gives one D-pad press per
   push.
5. The Touch profile must not lose all its buttons if the runtime refuses the thumb rest path.

---

### Task 1: The mapping, tested on its own

**Files:**
- Create: `port-project/src/vr_quest_controls.h`, `port-project/src/vr_quest_controls.cpp`
- Create: `port-project/tests/test_quest_controls.cpp`, `build-scripts/test_quest_controls.bat`

**Interfaces:**
- Produces:
  ```cpp
  namespace dkqc {
  struct QuestIn { uint32_t buttons; float ax[6]; bool in_level; int64_t now_ms; };
  struct PadOut  { uint16_t buttons; float lt, rt, lx, ly, rx, ry; };   // sticks in -1..1, triggers 0..1
  class QuestControls { public: PadOut Step(const QuestIn& in); };
  }
  ```
  The left stick is handed through untouched (`vr_bridge.cpp` still turns it by the head's yaw); `ry` is always 0
  in a level (no pitch).

- [ ] **Step 1: Write the failing tests** (`tests/test_quest_controls.cpp`, a plain `main` with a `CHECK` macro):
  - `menu_tap_is_one_back`: menu down at 0 ms, up at 200 ms: Back seen in the next 6 reads, Start never.
  - `menu_hold_is_start`: menu held 0..400 ms: Start from 300 ms on, Back never.
  - `rs_up_held_jumps`: right stick (0, 0.9) from 0 ms: no Y at 250 ms, Y at 300 ms and while held, gone after release.
  - `rs_up_short_does_nothing`: (0, 0.9) for 200 ms then centre: Y never.
  - `turning_never_jumps`: (0.9, 0.5) (about 61 degrees from up) held 1 s: Y never, rx = 0.9.
  - `rs_down_held_crouches`: (0, -0.9) for 300 ms: LS click (0x40) on.
  - `flick_cone_blocks_turn`: (0.2, 0.9) held: rx = 0 (inside the up cone the stick does not turn).
  - `thumbrest_picks_dpad`: thumb rest + (0, 0.9): D-pad up, no Y even after 1 s, rx = 0; (0.9, 0): D-pad right;
    (0, -0.9): D-pad down; (-0.9, 0): D-pad left.
  - `y_is_darkness`: Quest Y = LB; `lt_is_power`: left trigger 0.8 = RB, left trigger 0.3 = nothing; `rt_fires`:
    right trigger 0.7 = rt 0.7.
  - `a_use_x_redirect`: Quest A = A, Quest X = X.
  - `b_tap_reloads`: B down 0..150 ms: pad B pulse after release; `b_hold_no_reload`: B held 1.2 s then up: pad B
    never.
  - `grips_send_nothing`: raw 0x0100 | 0x0200: buttons 0; `lstick_click_sends_nothing`: raw 0x40 in a level: 0.
  - `menus_plain_pad`: in_level = false: A, B, X, Y as they are, right stick up passes as ry, thumb rest ignored.
  - `level_end_releases`: Y held by a flick at 400 ms, then in_level = false: Y not held.

- [ ] **Step 2: Run them to see them fail**

  Run: `build-scripts\test_quest_controls.bat`  Expected: a compile error (no `vr_quest_controls.h`).

- [ ] **Step 3: Implement `dkqc::QuestControls::Step`** in `vr_quest_controls.cpp` with the settings table above.
  Flicks: a direction counts when the stick is past 0.70 and within 35 degrees of straight up / down; the timer
  starts when it enters the cone and resets when it leaves. B: decided on release (before 1000 ms = a 6-read B
  pulse). Pulses are counted in reads, not time.

- [ ] **Step 4: Run the tests until all pass**

  Run: `build-scripts\test_quest_controls.bat`  Expected: `ALL n PASSED`, exit code 0.

- [ ] **Step 5: Commit** `the-darkness-vr: job 1 task 1, the Quest-to-pad mapping and its tests`.

### Task 2: The thumb rest from the SDK

**Files:**
- Modify: `D:/the-darkness-build/rexglue-sdk/src/graphics/d3d12/xr_output.cpp` (the State's button enum, the
  names table, `kTouch`, the profile loop, `rex_xr_sample_buttons`'s `kBits`)
- Create: `dev-archive/sdk-patches/17-xr-thumbrest.patch` (the diff of this task)

**Interfaces:**
- Produces: bit `0x0400` in `rex_xr_sample_buttons`'s buttons while the left thumb rests on the left thumb rest.

- [ ] **Step 1:** Add `BTN_LTHUMBREST` (boolean action `lthumbrest_touch`, "Left thumb rest") bound only in the Touch
  profile to `/user/hand/left/input/thumbrest/touch`; `kBits` gains `0x0400`.
- [ ] **Step 2:** The profile loop tries "Touch with the thumb rest" first; if refused, logs it and tries Touch without
  the thumb rest path, so the buttons never go.
- [ ] **Step 3:** Build the SDK (`build-scripts\home_build_sdk.bat`). Expected: builds, `rexgpu-xenos.dll` new.
- [ ] **Step 4:** Save the diff as patch 17; commit.

### Task 3: Wire it in, build, check on the pretend headset

**Files:**
- Modify: `port-project/src/vr_bridge.cpp` (`GetDeviceState`: use `QuestControls` unless `DK_VR_QUEST_CTL=0`)
- Modify: `port-project/CMakeLists.txt` (add `src/vr_quest_controls.cpp`)

- [ ] **Step 1:** In `GetDeviceState`, when the Quest pad is on and `DK_VR_QUEST_CTL` is not 0: build `QuestIn` from
  `b`, `ax`, `dkview::InLevelNow()`, a steady-clock ms; take `PadOut`; turn the left stick by the head yaw exactly as
  today; OR the buttons into the bridge pad's; take the larger trigger. Else today's path unchanged. The hand poser
  still gets the raw buttons first (`dkpose::FeedPad`). A log line once: `[DKXR] Quest layout 2026-11-14 on`.
- [ ] **Step 2:** Copy the changed files into `D:/the-darkness-build/src/DarknessRecomp`, build
  (`home_build_darkness.bat`). Expected: builds clean.
- [ ] **Step 3:** Into a test folder (not the installed build): the new exe + dll. On the simulator, in a level:
  through VR Bench, the menu tap opens the journal, the menu hold the pause menu; A uses; Y brings the Darkness out
  (the log or a still); the right stick held up 0.3 s jumps. Expected: each seen once `[verified-live, simulator]`.
  What VR Bench cannot press (the thumb rest, perhaps the sticks) stays for the headset.
- [ ] **Step 4:** Find out whether the game's crouch is hold or toggle (preset 1: the left stick click): if hold,
  keep the crouch latched until the next down-flick (`kCrouchLatch`), with a test added to Task 1's file.
- [ ] **Step 5:** Save the build as b030 (builds store), install beside b029 (the Desktop shortcut picks the highest),
  board row `[VR USER]` with the test list, modding note, commit and hand over.
