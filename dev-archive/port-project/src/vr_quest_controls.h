// The Quest controllers as the game's pad, Tefa's layout of 2026-11-14 (job 1 of
// dev-archive/design/2026-11-14-quest-controls-design.md). Pure logic, no SDK: one sample of the controllers in,
// one pad state out. vr_bridge.cpp's pad driver calls Step once per read; tests/test_quest_controls.cpp covers it.
#pragma once
#include <cstdint>

namespace dkqc {

// buttons: the raw bits from rex_xr_sample_buttons (A 0x1000, B 0x2000, X 0x4000, Y 0x8000, menu 0x0010,
// left stick click 0x0040, right stick click 0x0080, grips 0x0100 / 0x0200, left thumb rest 0x0400).
// ax: [0] left trigger, [1] right trigger, [2,3] left stick, [4,5] right stick.
struct QuestIn {
  uint32_t buttons;
  float ax[6];
  bool in_level;
  int64_t now_ms;
};

// buttons: XInput bits. Sticks -1..1, triggers 0..1. The left stick is handed through untouched (the caller turns it
// by the head's yaw); in a level ry is always 0 (the head looks up and down, not the stick).
struct PadOut {
  uint16_t buttons;
  float lt, rt, lx, ly, rx, ry;
};

class QuestControls {
 public:
  PadOut Step(const QuestIn& in);

 private:
  enum Flick { kNone, kUp, kDown, kLeft, kRight };
  Flick flick_ = kNone;          // the right-stick direction being held (cone + reach), or none
  int64_t flick_since_ = 0;
  bool dpad_sent_ = false;       // the thumb-rest pick already pressed for this push
  bool menu_was_ = false, start_sent_ = false;
  int64_t menu_since_ = 0;
  bool b_was_ = false;
  int64_t b_since_ = 0;
  int back_reads_ = 0, b_reads_ = 0, dpad_reads_ = 0;
  uint16_t dpad_bit_ = 0;
};

}  // namespace dkqc
