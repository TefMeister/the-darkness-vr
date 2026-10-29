// The Darkness VR: the hand poser (2026-10-29).
//
// Tefa, first wear with the turning hands: "hands are fine, but need rotating the right way". The old line-up was
// taken by itself at the first moment the hands were used, whatever way the controllers were held then, and from
// whichever character's hand solved first (other characters' hands were fed too). This replaces it with a line-up
// Tefa does on purpose, in the headset, and keeps it in a file so it is there at every launch.
//
// The pose of a hand is a fixed 3x3 O per hand: the hand bone's axes written in the controller's own axes. The hand's
// rows are O * C (C = the controller's axes in the world), so turning the controller turns the hand by the same turn.
// Lining up takes O = H * C^T, H being the animation's rows at that moment: the controller as held then counts as
// the game's own hand pose.
//
// In the headset (the controllers only, no desk):
//   hold B and Y together for 1 s   pose mode on: both hands go back to the game's own angle, and the game gets no
//                                   controller buttons or sticks (the Xbox pad still works);
//   pull a trigger                  that hand is lined up: hold the controller the way the game's hand looks first;
//   that hand's stick               turns the lined-up hand a little: left/right turns it, up/down tilts it; with
//                                   that hand's grip held, left/right rolls it;
//   hold B and Y for 1 s again      pose mode off, and the pose is saved to dk_hand_pose.txt beside the game.
// From the desk (bridge): pose.on, pose.lineup, pose.hand + pose.yaw/pitch/roll (degrees, applied once), pose.save.
//
// Sets: one pose per set, pose.set picks which (0 = empty hands). The other sets are for the guns, once the game
// tells us which gun is in the hand; until then only set 0 is used.
// [compile-verified] at most until it has run.

#include "generated/default/darknessrecomp_pch.h"

extern "C" {
#include "bridge/bridge.h"
}

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "vr_hand_pose.h"

namespace {

constexpr const char* kPoseFile = "dk_hand_pose.txt";   // beside the game (the launch scripts start it there)
constexpr int kSets = 4;
constexpr uint32_t kRawB = 0x2000, kRawY = 0x8000;     // the controllers' raw bits (SDK patch 11)
constexpr uint32_t kRawGrip[2] = {0x0100, 0x0200};     // left, right grip squeezed past half
constexpr int kComboHoldMs = 1000;                     // B + Y held this long switches pose mode
constexpr float kTriggerPulled = 0.8f, kTriggerLetGo = 0.3f;
constexpr float kStickDead = 0.2f;
constexpr float kNudgeDegPerS = 45.f;                  // a stick pushed all the way turns the hand this fast
constexpr float kDegToRad = 3.14159265f / 180.f;
constexpr float kMaxStepS = 0.1f;                      // a longer gap between solves turns no further than this

std::mutex g_pm;
int32_t g_set = 0;                     // pose.set
int32_t g_mode = 0;                    // pose.on
int32_t g_mode_seen = 0;               // the mode last acted on (pose.on can also be set from the desk)
int32_t g_lineup = 0;                  // pose.lineup: bit 0 left, bit 1 right, taken at the next solve
int32_t g_nudge_hand = 1;              // pose.hand (desk nudges): 0 left, 1 right
float g_nudge_deg[3] = {0, 0, 0};      // pose.yaw / pose.pitch / pose.roll: degrees, applied once, then back to 0
int32_t g_save = 0;                    // pose.save
bool g_have[kSets][2] = {};
float g_o[kSets][2][3][3];
bool g_lined[2] = {false, false};      // lined up in this pose-mode stretch
float g_rate[2][3] = {};               // from the sticks: degrees a second about the controller's X, Y, F
std::chrono::steady_clock::time_point g_last_solve[2];
char g_text[160] = "no pose yet";      // pose.text

const char* HandName(int hand) { return hand ? "right" : "left"; }

void UpdateText() {
  std::snprintf(g_text, sizeof(g_text), "mode=%d set=%d saved: left %s right %s; lined this stretch: left %d right %d",
                g_mode, g_set, g_have[g_set][0] ? "yes" : "no", g_have[g_set][1] ? "yes" : "no", g_lined[0] ? 1 : 0,
                g_lined[1] ? 1 : 0);
}

void Load() {
  std::FILE* f = std::fopen(kPoseFile, "r");
  if (!f) {
    REXGPU_INFO("[POSE] no {} yet: the hands keep the game's own angle until lined up (hold B+Y 1 s)", kPoseFile);
    return;
  }
  char line[512];
  int n = 0;
  while (std::fgets(line, sizeof(line), f)) {
    int set = -1;
    char hand = 0;
    float m[9];
    if (line[0] == '#') continue;
    if (std::sscanf(line, "%d %c %f %f %f %f %f %f %f %f %f", &set, &hand, &m[0], &m[1], &m[2], &m[3], &m[4], &m[5],
                    &m[6], &m[7], &m[8]) != 11)
      continue;
    if (set < 0 || set >= kSets || (hand != 'L' && hand != 'R')) continue;
    const int h = hand == 'R' ? 1 : 0;
    std::memcpy(g_o[set][h], m, sizeof(m));
    g_have[set][h] = true;
    ++n;
  }
  std::fclose(f);
  REXGPU_INFO("[POSE] {} hand pose(s) loaded from {}", n, kPoseFile);
}

// Called with g_pm held.
void Save() {
  std::FILE* f = std::fopen(kPoseFile, "w");
  if (!f) {
    REXGPU_INFO("[POSE] could not write {}", kPoseFile);
    return;
  }
  std::fprintf(f, "# The Darkness VR hand pose, written by the hand poser. set hand(L/R) then the 3x3, row by row.\n");
  int n = 0;
  for (int s = 0; s < kSets; ++s)
    for (int h = 0; h < 2; ++h) {
      if (!g_have[s][h]) continue;
      const float(*m)[3] = g_o[s][h];
      std::fprintf(f, "%d %c %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n", s, h ? 'R' : 'L', m[0][0], m[0][1],
                   m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]);
      ++n;
    }
  std::fclose(f);
  REXGPU_INFO("[POSE] {} hand pose(s) saved to {}", n, kPoseFile);
}

// Called with g_pm held.
void SetMode(int on) {
  g_mode = on;
  g_mode_seen = on;
  g_lined[0] = g_lined[1] = false;
  for (auto& r : g_rate)
    for (float& v : r) v = 0;
  REXGPU_INFO("[POSE] pose mode {}", on ? "ON: the hands show the game's angle; pull a trigger to line that hand up"
                                        : "off");
  if (!on) Save();
  UpdateText();
}

// Turns every row of o (vectors in the controller's axes) by `rad` about the controller's axis `axis`.
void Turn(float o[3][3], int axis, float rad) {
  const int a = (axis + 1) % 3, b = (axis + 2) % 3;
  const float c = std::cos(rad), s = std::sin(rad);
  for (int k = 0; k < 3; ++k) {
    const float va = o[k][a], vb = o[k][b];
    o[k][a] = c * va - s * vb;
    o[k][b] = s * va + c * vb;
  }
}

}  // namespace

namespace dkpose {

void Start() {
  static bool done = false;
  if (done) return;
  done = true;
  Load();
  UpdateText();
  bridge_var("pose.on", BRIDGE_I32, &g_mode, 1, "1 = pose mode (the hands show the game's angle until lined up)");
  bridge_var("pose.set", BRIDGE_I32, &g_set, 1, "which pose set is used: 0 = empty hands (1-3 kept for the guns)");
  bridge_var("pose.lineup", BRIDGE_I32, &g_lineup, 1, "line up at the next solve: 1 left, 2 right, 3 both");
  bridge_var("pose.hand", BRIDGE_I32, &g_nudge_hand, 1, "the hand pose.yaw/pitch/roll turn: 0 left, 1 right");
  bridge_var("pose.yaw", BRIDGE_F32, &g_nudge_deg[1], 1, "degrees to turn that hand about the controller's up, once");
  bridge_var("pose.pitch", BRIDGE_F32, &g_nudge_deg[0], 1, "degrees to tilt that hand about the controller's right, once");
  bridge_var("pose.roll", BRIDGE_F32, &g_nudge_deg[2], 1, "degrees to roll that hand about the controller's pointing axis, once");
  bridge_var("pose.save", BRIDGE_I32, &g_save, 1, "1 = save the pose file now");
  bridge_var_str("pose.text", g_text, sizeof(g_text), 0, "the poser's state");
}

bool FeedPad(uint32_t raw, const float ax[6]) {
  static std::chrono::steady_clock::time_point combo_since;
  static bool combo_was = false, combo_done = false;
  static bool trig_was[2] = {false, false};
  std::lock_guard<std::mutex> lock(g_pm);
  if (g_mode != g_mode_seen) SetMode(g_mode);
  const auto now = std::chrono::steady_clock::now();
  const bool combo = (raw & kRawB) && (raw & kRawY);
  if (combo && !combo_was) { combo_since = now; combo_done = false; }
  if (combo && !combo_done &&
      std::chrono::duration_cast<std::chrono::milliseconds>(now - combo_since).count() >= kComboHoldMs) {
    SetMode(g_mode ? 0 : 1);
    combo_done = true;
  }
  combo_was = combo;
  if (g_mode) {
    for (int h = 0; h < 2; ++h) {
      const float t = ax[h];
      const bool pulled = trig_was[h] ? t > kTriggerLetGo : t > kTriggerPulled;
      if (pulled && !trig_was[h]) g_lineup |= 1 << h;
      trig_was[h] = pulled;
      const float x = ax[2 + 2 * h], y = ax[3 + 2 * h];
      const float sx = std::fabs(x) > kStickDead ? x : 0.f, sy = std::fabs(y) > kStickDead ? y : 0.f;
      const bool grip = (raw & kRawGrip[h]) != 0;
      g_rate[h][0] = grip ? 0.f : sy * kNudgeDegPerS;   // tilt about the controller's right
      g_rate[h][1] = grip ? 0.f : sx * kNudgeDegPerS;   // turn about the controller's up
      g_rate[h][2] = grip ? sx * kNudgeDegPerS : 0.f;   // roll about the pointing axis
    }
  } else {
    trig_was[0] = trig_was[1] = false;
  }
  return g_mode != 0 || combo;
}

void RequestLineUp(uint32_t hands) {
  std::lock_guard<std::mutex> lock(g_pm);
  g_lineup |= static_cast<int32_t>(hands & 3u);
}

bool HandRows(int hand, const float h[3][3], const float c[3][3], float out[3][3]) {
  std::lock_guard<std::mutex> lock(g_pm);
  if (g_set < 0 || g_set >= kSets) g_set = 0;
  if (g_mode != g_mode_seen) SetMode(g_mode);
  if (g_save == 1) { g_save = 0; Save(); }
  const auto now = std::chrono::steady_clock::now();
  float dt = std::chrono::duration<float>(now - g_last_solve[hand]).count();
  g_last_solve[hand] = now;
  if (dt > kMaxStepS) dt = kMaxStepS;
  float(*o)[3] = g_o[g_set][hand];
  if (g_lineup & (1 << hand)) {
    g_lineup &= ~(1 << hand);
    for (int k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j) o[k][j] = h[k][0] * c[j][0] + h[k][1] * c[j][1] + h[k][2] * c[j][2];
    g_have[g_set][hand] = true;
    g_lined[hand] = true;
    REXGPU_INFO("[POSE] {} hand lined up (set {}): animation rows ({:.2f} {:.2f} {:.2f}) ({:.2f} {:.2f} {:.2f}) "
                "({:.2f} {:.2f} {:.2f})",
                HandName(hand), g_set, h[0][0], h[0][1], h[0][2], h[1][0], h[1][1], h[1][2], h[2][0], h[2][1], h[2][2]);
    UpdateText();
  }
  if (!g_have[g_set][hand]) return false;
  if (g_mode && !g_lined[hand]) return false;   // pose mode: the game's own angle until this hand is lined up
  if (g_mode)
    for (int a = 0; a < 3; ++a)
      if (g_rate[hand][a] != 0.f) Turn(o, a, g_rate[hand][a] * kDegToRad * dt);
  if (g_nudge_hand == hand)
    for (int a = 0; a < 3; ++a)
      if (g_nudge_deg[a] != 0.f) {
        Turn(o, a, g_nudge_deg[a] * kDegToRad);
        REXGPU_INFO("[POSE] {} hand turned {:.1f} degrees about controller axis {}", HandName(hand), g_nudge_deg[a], a);
        g_nudge_deg[a] = 0.f;
      }
  for (int k = 0; k < 3; ++k)
    for (int i = 0; i < 3; ++i) out[k][i] = o[k][0] * c[0][i] + o[k][1] * c[1][i] + o[k][2] * c[2][i];
  return true;
}

}  // namespace dkpose
