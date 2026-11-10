# Walking follows the head, and the right stick no longer tilts the view (2026-11-10, b020, Fable, simulator)

Tefa 2026-11-07: *"the darkness needs HMD direction movement"* and *"game also needs the decoupled pitch, right now i
can look up and down with RS"*.

Both are done on the sticks, in `vr_bridge.cpp`, where the motion controllers are laid over the game's pad; only in a
level (the camera build ran in the last half second), so the menus keep the sticks as they are.

- **`ctl.no_pitch` = 1 (`DK_VR_NO_PITCH`):** the right stick's up / down is not handed to the game; left / right still
  turns. Seen: the stick held up for 1.5 s, the picture stayed level. `[verified-live 2026-11-10, simulator, n=1]`
- **`ctl.head_walk` = 1 (`DK_VR_HEAD_WALK`):** the left stick is turned by the head's own yaw before the game gets it.
  The game moves relative to its own camera yaw; the head's turn is not part of that (DK_XR_HEAD turns the drawn camera
  only), so the stick is given the difference: `x' = x cos(yaw) + y sin(yaw)`, `y' = -x sin(yaw) + y cos(yaw)`, yaw =
  the head's turn to the right in the headset's own space (from the same head sample the hands use).
  Seen `[verified-live 2026-11-10, simulator, n=1]`: head straight, stick forward 1 s: the camera went from x 193.4 to
  307.6 (y unchanged at -943.9); head turned 90 degrees left, stick forward 1 s: x stayed (307.6 -> 308.4), y went
  -943.9 -> -897.2, and left is +y there (the right hand sits at the lower y). So "forward" walks where the head looks.

**Not worn.** For Tefa: walking while looking around; strafing; whether a turn of the head while walking feels right
(the path bends with the head at once, no easing).
