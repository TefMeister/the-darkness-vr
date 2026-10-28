# Controller sampling for step 2: patch 10 draft, and what the simulator offers

From: the background reader of the 2026-10-28 live session. Static only: nothing launched, the SDK not
patched and not built.

## The draft

**File:** `staging/the-darkness-vr/xr-sim-2026-10-28/10-xr-hands-draft.patch`. It is a patch against the SDK's
`include/rex/graphics/d3d12/xr_output.h` and `src/graphics/d3d12/xr_output.cpp`.

- **Where patch 09 lives.** There is no `09-*` in `the-darkness-vr/dev-archive/sdk-patches/` (it stops at 08c).
  Patch 09 is `condemned-2-vr/patches/09-rexglue-sdk-openxr-per-eye.patch`, already applied in
  `D:\the-darkness-build\rexglue-sdk`. `rex_xr_sample_head` is at the end of `src/graphics/d3d12/xr_output.cpp`
  as `extern "C" __declspec(dllexport)`. The game finds it through
  `GetProcAddress(GetModuleHandleW(L"rexgpu-xenos.dll"), ...)` in `DarknessRecomp/src/vr_xr_head.cpp`
  `[measured 2026-10-28]`.
- **What it adds:**
  - **The action set.** `StartHands()` runs once, at the end of `Start()`, right after the local and view
    spaces are made. It creates:
    - action set `dk_hands` with two pose actions, `grip_pose` and `aim_pose`, each with subaction paths
      `/user/hand/left` and `/user/hand/right`;
    - suggested bindings for khr/simple, oculus/touch, valve/index, htc/vive and microsoft/motion (a profile the
      runtime refuses is skipped);
    - `xrAttachSessionActionSets`, and one action space per hand per pose.

    Any failure logs `[XR] hands OFF: ...` and leaves the headset path untouched. The new OpenXR functions load
    from their own list, so a missing one cannot switch off the picture.
  - **Focus tracking.** `PollEvents` now records whether the session is FOCUSED.
  - **`rex_xr_sample_hands(uint32_t* sample_id, float grip_left[7], float grip_right[7], uint32_t* flags)`.**
    Each call:
    1. Runs `xrSyncActions` once, only while the session is focused.
    2. Per hand, calls `xrGetActionStatePose` (whether the hand is active), then `xrLocateSpace` of the grip
       space. Both happen in **the same local space and at the same predicted time
       (predicted + lookahead x period) as `rex_xr_sample_head`**.

    Each pose is `px py pz qx qy qz qw`, in metres, with +X right, +Y up and -Z forward. `flags` is a set of bits
    declared in the header:
    - `DK_XR_HAND_L/R_ORIENT`, `_POS`, `_TRACKED`
    - `DK_XR_HAND_L/R_AIM`
    - `DK_XR_HAND_SYNCED`

    It returns 1 when either grip orientation is valid. A hand that is off or not tracked comes back as identity
    with its bits clear. It logs every 300 calls.
  - **`rex_xr_hands_aim(sample_id, aim_left[7], aim_right[7])`:** the aim poses from that same call.
- **Checked** `[compile-verified 2026-10-28]`:
  - `git apply --check` passes on copies of the two current SDK files.
  - The new `xr_output.cpp` (and the old one, for comparison) passes `clang++ -std=c++20 -fsyntax-only -Wall`.
    That run used llvm-mingw with SDL3's bundled OpenXR headers, not the SDK's own toolchain.
  - Not built, not run.
- **Threads** `[hypothesis]`. The hands are synced and located on the game's frame thread, under their own
  mutex. OpenXR handles these calls from any thread, but the runtime's behaviour under that has not been seen yet.
- **Game side, not written:**
  - Look up `rex_xr_sample_hands` beside `rex_xr_sample_head` and call it on the same frame, right after the
    head.
  - Turn the grip into a game point: (grip - head) in metres, rotated by the camera the same way the head offset
    is, scaled to game units, plus the camera's world position. Write the result to `IK+1864` from the existing
    `sub_821BF988` hook.

## The OpenXR Simulator and hands

All `[inferred-static 2026-10-28]`, read from the README and from the DLL's strings.

- **It has a basic action system.** It logs `xrCreateActionSet`, `xrCreateAction`,
  `xrSuggestInteractionProfileBindings`, `xrAttachSessionActionSets`, `xrCreateActionSpace` and `xrSyncActions`
  (which logs "active interaction profile = %s"). It knows the profiles simple, touch, index, vive, motion,
  daydream, go and xbox, and the paths `input/grip/pose` and `input/aim/pose`. So the draft's bindings should
  resolve on it.
- **It can move the controllers, through a file the MCP server does not offer.** The file is
  `%LOCALAPPDATA%\OpenXR-Simulator\controller_pose_command.json`, the same folder as `head_pose_command.json`.
  - Keys found in the DLL: `hand`, `posX`, `posY`, `posZ`, `yaw`, `pitch`, `trigger`, `buttonA`.
  - Its log line: "Controller pose command: hand=%d pos(...) yaw pitch trigger".
  - It has no roll.
  - Hand 0/1 = left/right, metres, and radians like the head command `[hypothesis]`.
  - The runtime consumes these files when it reads them, so write one only while the game runs, as the MCP
    server's own `_write_control_command` does.
- **What it does not do:**
  - The README lists "No hand tracking" (that means finger tracking) and has "Configurable controller emulation"
    still on its roadmap.
  - The MCP tools move only the head.
- **So:**
  - The simulator should be enough for the first test: is a hand sampled, does it move when the file says so,
    and does it come back in the same space as the head. The patch's `[XR] hands ...` log line and a
    `controller_pose_command.json` written by hand will show it `[hypothesis until run]`.
  - Real 6DoF feel, roll, and button presses need VR Bench or the headset.
