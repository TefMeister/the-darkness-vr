// The Darkness VR: the game's own hand IK, fed a target of ours (2026-10-28).
//
// The reader's static pass (dev-archive/recon/2026-10-28-hand-ik-sim/2026-10-28-reader-hand-ik-solver*.md) found
// that the game's hand IK has an EXPLICIT-TARGET path: sub_821BF988(ik, skeleton, skel instance, world, character)
// reads a vec3 at ik+1864 (hand chain A) or ik+1892 (chain B) when the flag word at ik+0 has 0x00100000 /
// 0x00200000 set, and solves the two-bone arm so the hand's grip bone lands there, keeping the animation's rotation.
// The game sets those targets itself with sub_821BF880: vec3 to +1864, flags |= 0x100000 | 0x400000, a blend time
// at +1876/+1880, a constant at +1884; the same for B at +1892 / 0x200000 | 0x800000 / +1904..+1912.
//
// This hook sits on sub_821BF988's ENTRY (r3 = the IK object) and, when ik.on is set through the bridge, writes a
// target of ours first, exactly as sub_821BF880 would. Two sources (ik.src):
//   0  a test point: the newest camera position plus ik.fwd / ik.right / ik.up along the camera's rows (world units)
//      [verified-live 2026-10-28, simulator: the hand reaches for it and follows it]
//   1  the motion controllers (SDK patch 10, sampled with every head sample in vr_xr_head.cpp): each controller's
//      grip position relative to the head, turned into the head's own axes, laid along the (head-turned) camera's
//      rows from the camera position, times ik.scale (world units per metre; 1 unit ~ 1 inch, so ~39.37
//      [inferred-static, dossier: "ONE WORLD UNIT IS ABOUT ONE INCH"]). ik.right_chain says which chain the RIGHT
//      controller drives (the other drives the left); only a controller whose position is valid is fed.
// ik.only limits it to one IK object; ik.vtable to the objects whose ClientData vtable (*(ik-4384)) matches (the
// reader's table: 0x82062190 most likely the playable player). ik.snap=1 also sets the hand's blend state to "held"
// with full weight (+1448/+1450 = 3, +1452/+1456 = 1.0), skipping the game's 0.2 s blend-in [untried].
// Counters and the seen-list are behind one mutex: the hook runs on several game threads at once.
// [compile-verified] at most until it has run.

#include "generated/default/darknessrecomp_pch.h"

#include <rex/hook.h>

extern "C" {
#include "bridge/bridge.h"
}

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "vr_xr_head.h"

namespace {

constexpr uint32_t kFlags = 0u;          // ik+0: the flag word
constexpr uint32_t kTarget[2] = {1864u, 1892u};      // vec3 per chain (A, B)
constexpr uint32_t kTime1[2] = {1876u, 1904u}, kTime2[2] = {1880u, 1908u}, kConst[2] = {1884u, 1912u};
constexpr uint32_t kOn[2] = {0x00100000u, 0x00200000u}, kFresh[2] = {0x00400000u, 0x00800000u};
constexpr uint32_t kState[2] = {1448u, 1450u};       // per-hand blend state byte: 0 off, 1 in, 2 out, 3 held
constexpr uint32_t kWeight[2] = {1452u, 1456u};      // per-hand blend weight
constexpr uint8_t kStateHeld = 3;
constexpr uint32_t kClientDataBack = 4384u;          // IK object = ClientData + 4384; ClientData+0 = its vtable
constexpr uint32_t kRowRight = 0u, kRowDown = 16u, kRowFwd = 32u, kRowPos = 48u;   // dossier 6d
constexpr uint32_t kPosValid[2] = {0x02u, 0x20u};    // DK_XR_HAND_L_POS, DK_XR_HAND_R_POS (SDK patch 10)
constexpr float kUnitsPerMetre = 39.37f;             // 1 world unit ~ 1 inch [inferred-static]
constexpr int kSeenMax = 8;
constexpr uint32_t kReportEvery = 3000;              // hook calls between [IK] summary lines while on

std::mutex g_mu;
int32_t g_on = 0;                 // ik.on
int32_t g_src = 0;                // ik.src: 0 test point, 1 controllers
int32_t g_hand = 0;               // ik.hand (src 0): 0 = A, 1 = B, 2 = both
int32_t g_right_chain = 0;        // ik.right_chain (src 1): the chain the RIGHT controller drives
float g_fwd = 24.f, g_right = 8.f, g_up = -6.f;   // ik.fwd / ik.right / ik.up (src 0)
float g_scale = kUnitsPerMetre;   // ik.scale (src 1): world units per metre
float g_time = 0.25f, g_const = 0.75f;            // what sub_821BF880 stores at +1876.. / +1884
int32_t g_snap = 0;               // ik.snap
uint32_t g_only = 0;              // ik.only
uint32_t g_vtable = 0;            // ik.vtable
uint32_t g_calls = 0, g_set = 0, g_no_hands = 0;  // ik.calls / ik.set / ik.no_hands
uint32_t g_seen[kSeenMax], g_seen_vt[kSeenMax];
int g_seen_n = 0;
char g_seen_text[kSeenMax * 22 + 8] = "none yet";
char g_last_text[400] = "none yet";              // ik.last: the newest controller points written

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
  bridge_var("ik.on", BRIDGE_I32, &g_on, 1, "1 = feed the game's hand IK a target of ours");
  bridge_var("ik.src", BRIDGE_I32, &g_src, 1, "0 = test point (camera + fwd/right/up), 1 = the motion controllers");
  bridge_var("ik.hand", BRIDGE_I32, &g_hand, 1, "src 0: 0 = hand chain A, 1 = chain B, 2 = both");
  bridge_var("ik.right_chain", BRIDGE_I32, &g_right_chain, 1, "src 1: the chain (0 = A, 1 = B) the RIGHT controller drives");
  bridge_var("ik.fwd", BRIDGE_F32, &g_fwd, 1, "src 0: world units along the camera's forward");
  bridge_var("ik.right", BRIDGE_F32, &g_right, 1, "src 0: world units along the camera's right");
  bridge_var("ik.up", BRIDGE_F32, &g_up, 1, "src 0: world units along the camera's up");
  bridge_var("ik.scale", BRIDGE_F32, &g_scale, 1, "src 1: world units per metre of controller movement");
  bridge_var("ik.time", BRIDGE_F32, &g_time, 1, "the blend time stored at +1876/+1880");
  bridge_var("ik.const", BRIDGE_F32, &g_const, 1, "the value stored at +1884 (the game uses 0.75)");
  bridge_var("ik.snap", BRIDGE_I32, &g_snap, 1, "1 = set the hand's blend to held, full weight (skip the blend-in)");
  bridge_var("ik.only", BRIDGE_U32, &g_only, 1, "feed only this IK object (guest address from ik.seen); 0 = all");
  bridge_var("ik.vtable", BRIDGE_U32, &g_vtable, 1, "feed only IK objects whose ClientData vtable is this; 0 = all");
  bridge_var("ik.calls", BRIDGE_U32, &g_calls, 0, "calls of the explicit-target hand IK (sub_821BF988)");
  bridge_var("ik.set", BRIDGE_U32, &g_set, 0, "calls where a target of ours was written");
  bridge_var("ik.no_hands", BRIDGE_U32, &g_no_hands, 0, "src 1: calls with no controller position to feed");
  bridge_var_str("ik.seen", g_seen_text, sizeof(g_seen_text), 0, "the first IK objects met: address/ClientData vtable");
  bridge_var_str("ik.last", g_last_text, sizeof(g_last_text), 0, "src 1: the newest controller points written");
}

// Called with g_mu held.
void NoteObject(uint8_t* base, uint32_t ik) {
  for (int i = 0; i < g_seen_n; ++i)
    if (g_seen[i] == ik) return;
  if (g_seen_n >= kSeenMax) return;
  const uint32_t vt = REX_LOAD_U32(ik - kClientDataBack);
  g_seen[g_seen_n] = ik;
  g_seen_vt[g_seen_n] = vt;
  ++g_seen_n;
  int p = 0;
  for (int i = 0; i < g_seen_n && p < static_cast<int>(sizeof(g_seen_text)) - 22; ++i)
    p += std::snprintf(g_seen_text + p, sizeof(g_seen_text) - p, "%s%08X/%08X", i ? " " : "", g_seen[i], g_seen_vt[i]);
  REXGPU_INFO("[IK] hand IK object #{} at 0x{:08X}, ClientData vtable 0x{:08X}, flags 0x{:08X}", g_seen_n, ik, vt,
              REX_LOAD_U32(ik + kFlags));
}

void WriteChain(uint8_t* base, uint32_t ik, int chain, const float t[3], uint32_t& flags) {
  for (int i = 0; i < 3; ++i) StoreGuestFloat(base, ik + kTarget[chain] + 4u * i, t[i]);
  StoreGuestFloat(base, ik + kTime1[chain], g_time);
  StoreGuestFloat(base, ik + kTime2[chain], g_time);
  StoreGuestFloat(base, ik + kConst[chain], g_const);
  flags |= kOn[chain] | kFresh[chain];
  if (g_snap == 1) {
    REX_STORE_U8(ik + kState[chain], kStateHeld);
    StoreGuestFloat(base, ik + kWeight[chain], 1.0f);
  }
}

// Head rotation (OpenXR quaternion) as a 3x3 whose columns are the head's right, up and back axes.
void QuatToRot(const float q[4], float m[3][3]) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  m[0][0] = 1 - 2 * (y * y + z * z); m[0][1] = 2 * (x * y - z * w);     m[0][2] = 2 * (x * z + y * w);
  m[1][0] = 2 * (x * y + z * w);     m[1][1] = 1 - 2 * (x * x + z * z); m[1][2] = 2 * (y * z - x * w);
  m[2][0] = 2 * (x * z - y * w);     m[2][1] = 2 * (y * z + x * w);     m[2][2] = 1 - 2 * (x * x + y * y);
}

// A controller's grip, relative to the head, as a world point: laid along the head-turned camera's rows.
void ControllerPoint(const dkxr::HandSample& h, int hand, const float rows[16], float out[3]) {
  float m[3][3];
  QuatToRot(h.head_q, m);
  float d[3], dh[3];
  for (int i = 0; i < 3; ++i) d[i] = h.grip[hand][i] - h.head_p[i];
  for (int j = 0; j < 3; ++j) dh[j] = m[0][j] * d[0] + m[1][j] * d[1] + m[2][j] * d[2];   // into the head's axes
  for (int i = 0; i < 3; ++i)
    out[i] = rows[kRowPos / 4 + i] + g_scale * (dh[0] * rows[kRowRight / 4 + i] - dh[1] * rows[kRowDown / 4 + i] -
                                                dh[2] * rows[kRowFwd / 4 + i]);
}

void Feed(uint8_t* base, uint32_t ik) {
  float rows[16];
  if (!dkxr::LastView(rows)) return;
  uint32_t flags = REX_LOAD_U32(ik + kFlags);
  bool wrote = false;
  if (g_src == 0) {
    float t[3];
    for (int i = 0; i < 3; ++i)
      t[i] = rows[kRowPos / 4 + i] + g_fwd * rows[kRowFwd / 4 + i] + g_right * rows[kRowRight / 4 + i] -
             g_up * rows[kRowDown / 4 + i];
    std::snprintf(g_last_text, sizeof(g_last_text), "test point (%.1f %.1f %.1f) cam(%.1f %.1f %.1f)", t[0], t[1], t[2],
                  rows[kRowPos / 4], rows[kRowPos / 4 + 1], rows[kRowPos / 4 + 2]);
    if (g_hand == 0 || g_hand == 2) { WriteChain(base, ik, 0, t, flags); wrote = true; }
    if (g_hand == 1 || g_hand == 2) { WriteChain(base, ik, 1, t, flags); wrote = true; }
  } else {
    dkxr::HandSample h;
    if (!dkxr::LastHands(&h)) { ++g_no_hands; return; }
    float pts[2][3];
    for (int hand = 0; hand < 2; ++hand) {   // 0 left, 1 right
      if (!(h.flags & kPosValid[hand])) continue;
      ControllerPoint(h, hand, rows, pts[hand]);
      const int chain = hand == 1 ? (g_right_chain & 1) : 1 - (g_right_chain & 1);
      WriteChain(base, ik, chain, pts[hand], flags);
      wrote = true;
    }
    if (!wrote) { ++g_no_hands; return; }
    std::snprintf(g_last_text, sizeof(g_last_text),
                  "L(%.1f %.1f %.1f) R(%.1f %.1f %.1f) flags=0x%08X n=%u | head(%.3f %.3f %.3f) gripL(%.3f %.3f %.3f) "
                  "gripR(%.3f %.3f %.3f) cam(%.1f %.1f %.1f)",
                  pts[0][0], pts[0][1], pts[0][2], pts[1][0], pts[1][1], pts[1][2], h.flags, h.serial, h.head_p[0],
                  h.head_p[1], h.head_p[2], h.grip[0][0], h.grip[0][1], h.grip[0][2], h.grip[1][0], h.grip[1][1],
                  h.grip[1][2], rows[kRowPos / 4], rows[kRowPos / 4 + 1], rows[kRowPos / 4 + 2]);
  }
  if (wrote) {
    REX_STORE_U32(ik + kFlags, flags);
    ++g_set;
  }
}

}  // namespace

DECLARE_REX_FUNC(sub_821BF988);

REX_HOOK_RAW(sub_821BF988) {
  RegisterVars();
  const uint32_t ik = ctx.r3.u32;
  if (ik != 0u) {
    std::lock_guard<std::mutex> lock(g_mu);
    ++g_calls;
    NoteObject(base, ik);
    const bool picked = (g_only == 0u || g_only == ik) &&
                        (g_vtable == 0u || g_vtable == REX_LOAD_U32(ik - kClientDataBack));
    if (g_on == 1 && picked) Feed(base, ik);
    if (g_on == 1 && g_calls % kReportEvery == 0)
      REXGPU_INFO("[IK] calls={} set={} no_hands={} src={} last: {}", g_calls, g_set, g_no_hands, g_src, g_last_text);
  }
  __imp__sub_821BF988(ctx, base);
}
