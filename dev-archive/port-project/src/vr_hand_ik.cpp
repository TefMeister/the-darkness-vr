// The Darkness VR: the game's own hand IK, fed a target of ours (2026-10-28, first live try).
//
// The reader's static pass (engine-research/inbox/2026-10-28-reader-hand-ik-solver.md) found that the game's
// hand IK has an EXPLICIT-TARGET path: sub_821BF988(ik, ...) reads a vec3 at ik+1864 (hand chain A) or
// ik+1892 (chain B) when the flag word at ik+0 has 0x00100000 / 0x00200000 set, and solves the two-bone arm so
// the hand's "grip" bone lands there, keeping the animation's rotation. The game sets those targets itself
// with sub_821BF880(ik, pointA, ?, pointB): vec3 copied to +1864, flags |= 0x100000 | 0x400000, +1876 and
// +1880 = a blend time (its f1), +1884 = a constant; same for B at +1892 / 0x200000 | 0x800000 / +1904..1912.
//
// This hook sits on sub_821BF988's ENTRY (its r3 is the IK object) and, when ik.on is set through the
// bridge, writes a target of ours first: the newest camera position plus ik.fwd / ik.right / ik.up along the
// camera's rows (world units ~ inches), exactly as sub_821BF880 would. ik.hand picks A, B or both. ik.only
// limits it to one IK object (its guest address, from ik.seen) so NPC hands are left alone once the player's
// is known. Counters: ik.calls, ik.set, ik.seen (the first few IK objects met, logged once each).
//
// Milestone 3 of the motion-controlled-hands plan: once the controllers are sampled (SDK patch, step 2),
// the controller's grip point goes where ik.fwd/right/up go now. [compile-verified] at most until it has run.

#include "generated/default/darknessrecomp_pch.h"

#include <rex/hook.h>

extern "C" {
#include "bridge/bridge.h"
}

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "vr_xr_head.h"

namespace {

constexpr uint32_t kFlags = 0u;          // ik+0: the flag word
constexpr uint32_t kTargetA = 1864u;     // vec3, hand chain A
constexpr uint32_t kTimeA1 = 1876u, kTimeA2 = 1880u, kConstA = 1884u;
constexpr uint32_t kTargetB = 1892u;     // vec3, hand chain B
constexpr uint32_t kTimeB1 = 1904u, kTimeB2 = 1908u, kConstB = 1912u;
constexpr uint32_t kOnA = 0x00100000u, kFreshA = 0x00400000u;
constexpr uint32_t kOnB = 0x00200000u, kFreshB = 0x00800000u;
constexpr uint32_t kRowRight = 0u, kRowDown = 16u, kRowFwd = 32u, kRowPos = 48u;   // dossier 6d
constexpr int kSeenMax = 8;

int32_t g_on = 0;                 // ik.on
int32_t g_hand = 0;               // ik.hand: 0 = A, 1 = B, 2 = both
float g_fwd = 24.f, g_right = 8.f, g_up = -6.f;   // ik.fwd / ik.right / ik.up: the target, camera-relative
float g_time = 0.25f, g_const = 0.f;              // ik.time / ik.const: what sub_821BF880 stores at +1876.. / +1884
uint32_t g_only = 0;              // ik.only: IK object address to feed, 0 = every one
uint32_t g_calls = 0, g_set = 0;  // ik.calls / ik.set
uint32_t g_seen[kSeenMax];
int g_seen_n = 0;
char g_seen_text[kSeenMax * 12 + 8] = "none yet";

inline float GuestFloat(uint8_t* base, uint32_t a) {
  const uint32_t bits = REX_LOAD_U32(a);
  float v;
  std::memcpy(&v, &bits, sizeof(v));
  return v;
}
inline void StoreGuestFloat(uint8_t* base, uint32_t a, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, sizeof(bits));
  REX_STORE_U32(a, bits);
}

void RegisterVars() {
  static bool done = false;
  if (done) return;
  done = true;
  bridge_var("ik.on", BRIDGE_I32, &g_on, 1, "1 = feed the game's hand IK a target of ours (camera + fwd/right/up)");
  bridge_var("ik.hand", BRIDGE_I32, &g_hand, 1, "0 = hand chain A, 1 = chain B, 2 = both");
  bridge_var("ik.fwd", BRIDGE_F32, &g_fwd, 1, "target: world units along the camera's forward");
  bridge_var("ik.right", BRIDGE_F32, &g_right, 1, "target: world units along the camera's right");
  bridge_var("ik.up", BRIDGE_F32, &g_up, 1, "target: world units along the camera's up");
  bridge_var("ik.time", BRIDGE_F32, &g_time, 1, "the blend time stored at ik+1876/+1880 (sub_821BF880's f1)");
  bridge_var("ik.const", BRIDGE_F32, &g_const, 1, "the value stored at ik+1884");
  bridge_var("ik.only", BRIDGE_U32, &g_only, 1, "feed only this IK object (guest address from ik.seen); 0 = all");
  bridge_var("ik.calls", BRIDGE_U32, &g_calls, 0, "calls of the explicit-target hand IK (sub_821BF988)");
  bridge_var("ik.set", BRIDGE_U32, &g_set, 0, "calls where a target of ours was written");
  bridge_var_str("ik.seen", g_seen_text, sizeof(g_seen_text), 0, "the first IK objects met (guest addresses)");
}

}  // namespace

DECLARE_REX_FUNC(sub_821BF988);

REX_HOOK_RAW(sub_821BF988) {
  RegisterVars();
  const uint32_t ik = ctx.r3.u32;
  ++g_calls;
  if (ik != 0u) {
    bool known = false;
    for (int i = 0; i < g_seen_n; ++i)
      if (g_seen[i] == ik) known = true;
    if (!known && g_seen_n < kSeenMax) {
      g_seen[g_seen_n++] = ik;
      int p = 0;
      for (int i = 0; i < g_seen_n && p < static_cast<int>(sizeof(g_seen_text)) - 12; ++i)
        p += std::snprintf(g_seen_text + p, sizeof(g_seen_text) - p, "%s%08X", i ? " " : "", g_seen[i]);
      REXGPU_INFO("[IK] hand IK object #{} at 0x{:08X} (flags 0x{:08X})", g_seen_n, ik, REX_LOAD_U32(ik + kFlags));
    }
    float rows[16];
    if (g_on == 1 && (g_only == 0u || g_only == ik) && dkxr::LastView(rows)) {
      float t[3];
      for (int i = 0; i < 3; ++i)
        t[i] = rows[kRowPos / 4 + i] + g_fwd * rows[kRowFwd / 4 + i] + g_right * rows[kRowRight / 4 + i] -
               g_up * rows[kRowDown / 4 + i];
      uint32_t flags = REX_LOAD_U32(ik + kFlags);
      if (g_hand == 0 || g_hand == 2) {
        for (int i = 0; i < 3; ++i) StoreGuestFloat(base, ik + kTargetA + 4u * i, t[i]);
        StoreGuestFloat(base, ik + kTimeA1, g_time);
        StoreGuestFloat(base, ik + kTimeA2, g_time);
        StoreGuestFloat(base, ik + kConstA, g_const);
        flags |= kOnA | kFreshA;
      }
      if (g_hand == 1 || g_hand == 2) {
        for (int i = 0; i < 3; ++i) StoreGuestFloat(base, ik + kTargetB + 4u * i, t[i]);
        StoreGuestFloat(base, ik + kTimeB1, g_time);
        StoreGuestFloat(base, ik + kTimeB2, g_time);
        StoreGuestFloat(base, ik + kConstB, g_const);
        flags |= kOnB | kFreshB;
      }
      REX_STORE_U32(ik + kFlags, flags);
      ++g_set;
    }
  }
  __imp__sub_821BF988(ctx, base);
}
