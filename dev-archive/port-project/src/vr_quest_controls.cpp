// The Quest controllers as the game's pad: see vr_quest_controls.h and the 2026-11-14 controls design.
//
// The game's own default pad layout (preset 1, its controls page in Content/Gui/CubeWnd.xcr, read 2026-11-14
// [inferred-static]): Back journal, Start pause, LT secondary fire, RT primary fire, LB drain (the Darkness out / in),
// RB the Darkness (use the power), left stick click crouch, right stick click aim, Y jump, A use, B reload,
// X redirect (darklings), D-pad up / down a power, D-pad left / right a weapon.
//
// Tefa's layout in a level (2026-11-14):
//   A = use (A)              X = send the darkling (X)        Y = the Darkness out / in (LB)
//   left trigger = the power (RB)                              right trigger = fire (RT)
//   left grip + left trigger = the left gun (LT), never the power
//   B tap = reload (B; the hand reload of job 5 replaces it)   B held 1 s = drop the gun (job 4; nothing yet)
//   menu tap = journal (Back)                                  menu held 0.3 s = pause (Start)
//   right stick left / right = turn                            right stick up held 0.2 s = jump (Y)
//   right stick down held 0.2 s = crouch (left stick click)
//   left thumb on its rest + right stick = the D-pad (up / down a power, left / right a gun), one press per push
//   grips, stick clicks: nothing here (holsters, the dodge: later jobs)
// Out of a level (menus, films, the journal): the plain pad, so menus work as the game expects.
#include "vr_quest_controls.h"

#include <cmath>

namespace dkqc {
namespace {
// ---- settings (one table, 2026-11-14) ----
constexpr int64_t kFlickHoldMs = 200;   // a right-stick push held this long = jump / crouch (Tefa 2026-11-14: 0.3 -> 0.2 s)
constexpr float kFlickConeDeg = 35.0f;  // within this many degrees of straight up / down (or left / right on the rest)
constexpr float kFlickReach = 0.70f;    // and past this much of the stick's travel
constexpr float kStickDead = 0.15f;     // below this the stick is at rest
constexpr int64_t kMenuHoldMs = 300;    // menu held this long = pause; let go sooner = journal
constexpr int64_t kBHoldMs = 1000;      // B held this long = drop the gun, not a reload
constexpr int kPulseReads = 6;          // a tap is held for this many pad reads
constexpr float kTriggerAsButton = 0.5f;

// ---- bits ----
constexpr uint32_t qA = 0x1000, qB = 0x2000, qX = 0x4000, qY = 0x8000, qMenu = 0x0010, qThumbRest = 0x0400;
constexpr uint32_t qLGrip = 0x0100;   // the left grip squeezed past half (SDK patch 11)
constexpr uint16_t pUp = 0x1, pDown = 0x2, pLeft = 0x4, pRight = 0x8, pStart = 0x10, pBack = 0x20, pLS = 0x40;
constexpr uint16_t pLB = 0x100, pRB = 0x200, pA = 0x1000, pB = 0x2000, pX = 0x4000, pY = 0x8000;

// Degrees between the stick's direction and the axis direction (ax, ay).
float DegFrom(float x, float y, float ax, float ay) {
  const float dot = (x * ax + y * ay) / std::sqrt(x * x + y * y);
  return std::acos(dot < -1.0f ? -1.0f : dot > 1.0f ? 1.0f : dot) * 57.29578f;
}
}  // namespace

PadOut QuestControls::Step(const QuestIn& in) {
  PadOut o{};
  const uint32_t b = in.buttons;
  const int64_t now = in.now_ms;

  // The menu button: the same in and out of a level.
  const bool menu = (b & qMenu) != 0;
  if (menu && !menu_was_) { menu_since_ = now; start_sent_ = false; }
  if (menu) {
    if (now - menu_since_ >= kMenuHoldMs) { o.buttons |= pStart; start_sent_ = true; }
  } else if (menu_was_ && !start_sent_) {
    back_reads_ = kPulseReads;
  }
  menu_was_ = menu;
  if (back_reads_ > 0) { o.buttons |= pBack; --back_reads_; }

  if (!in.in_level) {   // the plain pad; whatever a level was holding is let go
    if (b & qA) o.buttons |= pA;
    if (b & qB) o.buttons |= pB;
    if (b & qX) o.buttons |= pX;
    if (b & qY) o.buttons |= pY;
    o.lt = in.ax[0]; o.rt = in.ax[1];
    o.lx = in.ax[2]; o.ly = in.ax[3]; o.rx = in.ax[4]; o.ry = in.ax[5];
    flick_ = kNone; dpad_sent_ = false; dpad_reads_ = 0; b_reads_ = 0; b_was_ = false;
    return o;
  }

  if (b & qA) o.buttons |= pA;
  if (b & qX) o.buttons |= pX;
  if (b & qY) o.buttons |= pLB;
  // Left grip + left trigger = the left gun (the game's secondary fire, LT), never the power (Tefa 2026-11-14).
  if (b & qLGrip) o.lt = in.ax[0];
  else if (in.ax[0] > kTriggerAsButton) o.buttons |= pRB;
  o.rt = in.ax[1];
  o.lx = in.ax[2]; o.ly = in.ax[3];

  // B: decided when it is let go (a tap reloads; a 1 s hold is the drop, a later job).
  const bool bdown = (b & qB) != 0;
  if (bdown && !b_was_) b_since_ = now;
  if (!bdown && b_was_ && now - b_since_ < kBHoldMs) b_reads_ = kPulseReads;
  b_was_ = bdown;
  if (b_reads_ > 0) { o.buttons |= pB; --b_reads_; }

  // The right stick.
  const float x = in.ax[4], y = in.ax[5];
  const float mag = std::sqrt(x * x + y * y);
  const bool rest = (b & qThumbRest) != 0;
  const bool moved = mag >= kStickDead;
  const bool in_up = moved && DegFrom(x, y, 0, 1) <= kFlickConeDeg;
  const bool in_down = moved && DegFrom(x, y, 0, -1) <= kFlickConeDeg;
  Flick d = kNone;
  if (mag >= kFlickReach) {
    if (in_up) d = kUp;
    else if (in_down) d = kDown;
    else if (rest && DegFrom(x, y, 1, 0) <= kFlickConeDeg) d = kRight;
    else if (rest && DegFrom(x, y, -1, 0) <= kFlickConeDeg) d = kLeft;
  }
  if (d != flick_) { flick_ = d; flick_since_ = now; dpad_sent_ = false; }

  if (rest) {   // the picker: the stick is the D-pad, nothing else
    if (flick_ != kNone && !dpad_sent_) {
      dpad_bit_ = flick_ == kUp ? pUp : flick_ == kDown ? pDown : flick_ == kLeft ? pLeft : pRight;
      dpad_reads_ = kPulseReads;
      dpad_sent_ = true;
    }
  } else {
    if (now - flick_since_ >= kFlickHoldMs) {
      if (flick_ == kUp) o.buttons |= pY;
      if (flick_ == kDown) o.buttons |= pLS;
    }
    if (!in_up && !in_down) o.rx = x;   // inside the up / down cones the stick does not turn
  }
  if (dpad_reads_ > 0) { o.buttons |= dpad_bit_; --dpad_reads_; }
  return o;
}

}  // namespace dkqc
