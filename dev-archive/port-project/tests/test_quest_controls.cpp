// Tests for the Quest-to-pad mapping (job 1 of the 2026-11-14 controls design). Plain main, no framework:
// build-scripts/test_quest_controls.bat compiles this with ../src/vr_quest_controls.cpp and runs it.
#include "../src/vr_quest_controls.h"

#include <cstdio>
#include <functional>

namespace {
int g_fail = 0, g_pass = 0;
#define CHECK(c)                                                                \
  do {                                                                          \
    if (!(c)) { std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } \
  } while (0)

using dkqc::PadOut;
using dkqc::QuestControls;
using dkqc::QuestIn;

constexpr uint32_t kA = 0x1000, kB = 0x2000, kX = 0x4000, kY = 0x8000, kMenu = 0x0010;
constexpr uint32_t kLClick = 0x0040, kLGrip = 0x0100, kRGrip = 0x0200, kThumbRest = 0x0400;
constexpr uint16_t pUp = 0x1, pDown = 0x2, pLeft = 0x4, pRight = 0x8, pStart = 0x10, pBack = 0x20, pLS = 0x40;
constexpr uint16_t pLB = 0x100, pRB = 0x200, pA = 0x1000, pB = 0x2000, pX = 0x4000, pY = 0x8000;

QuestIn In(int64_t ms, uint32_t b = 0, float rx = 0, float ry = 0, bool level = true) {
  QuestIn in{};
  in.buttons = b;
  in.ax[4] = rx;
  in.ax[5] = ry;
  in.in_level = level;
  in.now_ms = ms;
  in.darkness = -1;
  return in;
}

// Steps q from `from` to `to` (inclusive) every 10 ms with the same input shape; returns OR of all buttons seen.
uint16_t Run(QuestControls& q, int64_t from, int64_t to, uint32_t b, float rx = 0, float ry = 0, bool level = true) {
  uint16_t seen = 0;
  for (int64_t t = from; t <= to; t += 10) seen |= q.Step(In(t, b, rx, ry, level)).buttons;
  return seen;
}

void Test(const char* name, const std::function<void()>& f) {
  const int before = g_fail;
  f();
  if (g_fail == before) { ++g_pass; std::printf("ok   %s\n", name); }
  else std::printf("BAD  %s\n", name);
}
}  // namespace

int main() {
  Test("menu_tap_is_one_back", [] {
    QuestControls q;
    uint16_t seen = Run(q, 0, 200, kMenu);
    seen |= Run(q, 210, 400, 0);
    CHECK(seen & pBack);
    CHECK(!(seen & pStart));
  });
  Test("menu_hold_is_start", [] {
    QuestControls q;
    CHECK(!(Run(q, 0, 290, kMenu) & pStart));
    CHECK(q.Step(In(300, kMenu)).buttons & pStart);
    uint16_t seen = Run(q, 310, 400, kMenu) | Run(q, 410, 500, 0);
    CHECK(!(seen & pBack));
  });
  Test("rs_up_held_jumps", [] {
    QuestControls q;
    CHECK(!(Run(q, 0, 150, 0, 0.0f, 0.9f) & pY));
    CHECK(q.Step(In(200, 0, 0.0f, 0.9f)).buttons & pY);   // Tefa 2026-11-14: 0.3 s -> 0.2 s
    CHECK(q.Step(In(500, 0, 0.0f, 0.9f)).buttons & pY);
    CHECK(!(q.Step(In(510, 0, 0.0f, 0.0f)).buttons & pY));
  });
  Test("rs_up_short_does_nothing", [] {
    QuestControls q;
    uint16_t seen = Run(q, 0, 150, 0, 0.0f, 0.9f) | Run(q, 160, 600, 0);
    CHECK(!(seen & pY));
  });
  Test("turning_never_jumps", [] {
    QuestControls q;
    uint16_t seen = 0;
    PadOut o{};
    for (int64_t t = 0; t <= 1000; t += 10) { o = q.Step(In(t, 0, 0.9f, 0.5f)); seen |= o.buttons; }
    CHECK(!(seen & pY));
    CHECK(o.rx > 0.89f && o.rx < 0.91f);
    CHECK(o.ry == 0.0f);
  });
  Test("rs_down_held_crouches", [] {
    QuestControls q;
    CHECK(Run(q, 0, 300, 0, 0.0f, -0.9f) & pLS);
  });
  Test("flick_cone_blocks_turn", [] {
    QuestControls q;
    PadOut o = q.Step(In(0, 0, 0.2f, 0.9f));
    CHECK(o.rx == 0.0f);
  });
  Test("thumbrest_picks_dpad", [] {
    QuestControls q;
    PadOut o{};
    uint16_t seen = 0;
    for (int64_t t = 0; t <= 1000; t += 10) { o = q.Step(In(t, kThumbRest, 0.0f, 0.9f)); seen |= o.buttons; }
    CHECK(seen & pUp);
    CHECK(!(seen & pY));
    CHECK(o.rx == 0.0f);
    Run(q, 1010, 1100, kThumbRest);
    CHECK(Run(q, 1110, 1200, kThumbRest, 0.9f, 0.0f) & pRight);
    Run(q, 1210, 1300, kThumbRest);
    CHECK(Run(q, 1310, 1400, kThumbRest, 0.0f, -0.9f) & pDown);
    Run(q, 1410, 1500, kThumbRest);
    uint16_t l = Run(q, 1510, 1600, kThumbRest, -0.9f, 0.0f);
    CHECK(l & pLeft);
    CHECK(!(l & pLS));
  });
  Test("thumbrest_one_press_per_push", [] {
    QuestControls q;
    int downs = 0;
    bool was = false;
    for (int64_t t = 0; t <= 2000; t += 10) {
      const bool on = (q.Step(In(t, kThumbRest, 0.0f, 0.9f)).buttons & pUp) != 0;
      if (on && !was) ++downs;
      was = on;
    }
    CHECK(downs == 1);
  });
  Test("y_is_darkness", [] {
    QuestControls q;
    PadOut o = q.Step(In(0, kY));
    CHECK(o.buttons & pLB);
    CHECK(!(o.buttons & pY));
  });
  Test("lt_is_power", [] {
    QuestControls q;
    QuestIn in = In(0);
    in.ax[0] = 0.8f;
    PadOut o = q.Step(in);
    CHECK(o.buttons & pRB);
    CHECK(o.lt == 0.0f);
    in.ax[0] = 0.3f;
    in.now_ms = 10;
    CHECK(!(q.Step(in).buttons & pRB));
  });
  Test("lg_lt_fires_left_gun", [] {   // Tefa 2026-11-14: left grip + left trigger = the left gun, never the power
    QuestControls q;
    QuestIn in = In(0, kLGrip);
    in.ax[0] = 0.8f;
    PadOut o = q.Step(in);
    CHECK(!(o.buttons & pRB));
    CHECK(o.lt > 0.79f && o.lt < 0.81f);
  });
  Test("lt_power_only_when_darkness_out", [] {   // Tefa 2026-11-14: "LT uses darkness powers even when darkness is not out"
    QuestControls q;
    QuestIn in = In(0);
    in.ax[0] = 0.8f;
    in.darkness = 0;
    CHECK(!(q.Step(in).buttons & pRB));
    in.darkness = 1;
    in.now_ms = 10;
    CHECK(q.Step(in).buttons & pRB);
    in.darkness = -1;   // not known (no reading): as before, the power
    in.now_ms = 20;
    CHECK(q.Step(in).buttons & pRB);
  });
  Test("rt_fires", [] {
    QuestControls q;
    QuestIn in = In(0);
    in.ax[1] = 0.7f;
    PadOut o = q.Step(in);
    CHECK(o.rt > 0.69f && o.rt < 0.71f);
  });
  Test("a_use_x_redirect", [] {
    QuestControls q;
    CHECK(q.Step(In(0, kA)).buttons & pA);
    CHECK(q.Step(In(10, kX)).buttons & pX);
  });
  Test("b_tap_reloads", [] {
    QuestControls q;
    uint16_t during = Run(q, 0, 150, kB);
    CHECK(!(during & pB));
    CHECK(Run(q, 160, 300, 0) & pB);
  });
  Test("b_hold_no_reload", [] {
    QuestControls q;
    uint16_t seen = Run(q, 0, 1200, kB) | Run(q, 1210, 1500, 0);
    CHECK(!(seen & pB));
  });
  Test("grips_send_nothing", [] {
    QuestControls q;
    CHECK(q.Step(In(0, kLGrip | kRGrip)).buttons == 0);
  });
  Test("lstick_click_sends_nothing", [] {
    QuestControls q;
    CHECK(q.Step(In(0, kLClick)).buttons == 0);
  });
  Test("menus_plain_pad", [] {
    QuestControls q;
    PadOut o = q.Step(In(0, kA | kB | kX | kY | kThumbRest | kLGrip, 0.0f, 0.9f, false));
    CHECK(o.buttons == (pA | pB | pX | pY));
    CHECK(o.ry > 0.89f);
  });
  Test("level_end_releases", [] {
    QuestControls q;
    CHECK(Run(q, 0, 400, 0, 0.0f, 0.9f) & pY);
    CHECK(!(q.Step(In(410, 0, 0.0f, 0.9f, false)).buttons & pY));
  });

  std::printf(g_fail ? "%d FAILED, %d passed\n" : "ALL %d PASSED\n", g_fail ? g_fail : g_pass, g_pass);
  return g_fail ? 1 : 0;
}
