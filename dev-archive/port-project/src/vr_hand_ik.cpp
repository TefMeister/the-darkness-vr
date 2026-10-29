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
// DK_HANDS=1 (environment) starts with ik.on=1 ik.src=1. ik.only limits it to one IK object; ik.vtable to the objects whose ClientData vtable (*(ik-4384)) matches (the
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

#include "vr_hand_pose.h"
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
// 2026-10-29 (Tefa: "when i move my hands then still the other characters hands move with mine, same in a cutscene"):
// only the player's hands are fed. The player is the character whose shoulders are nearest the camera: bones 18 (arm
// A) and 13 (arm B) from the skeleton instance's bone matrices (*(SI+36), 64 bytes a bone, position at +48; dossier 6e).
// Nobody within kPlayerNear (the camera is not in anyone's head, a cutscene from outside) = nobody is fed.
constexpr uint32_t kSiBones = 36u, kBoneBytes = 64u, kBonePos = 48u;
constexpr uint32_t kShoulderBone[2] = {18u, 13u};
constexpr float kPlayerNear = 30.f;                  // world units (~76 cm) from the camera to the shoulders' middle
constexpr uint32_t kPickFresh = 64u;                 // an object not met in this many calls is left out of the pick
// The player has TWO IK objects on one skeleton (the same distance to the hundredth); only one of them moves the
// arms on screen [verified-live 2026-10-29, simulator: 8.56 vs 0.04 mean picture change], so every object as near as
// the nearest, to within this, is fed.
constexpr float kSameCharacter = 1.0f;
constexpr uint32_t kReportEvery = 3000;              // hook calls between [IK] summary lines while on

std::mutex g_mu;
int32_t g_on = 0;                 // ik.on
int32_t g_src = 0;                // ik.src: 0 test point, 1 controllers
int32_t g_hand = 0;               // ik.hand (src 0): 0 = A, 1 = B, 2 = both
int32_t g_right_chain = 1;        // ik.right_chain (src 1): the chain the RIGHT controller drives (1 since 2026-10-29: 0 crossed the hands in the headset)
float g_fwd = 24.f, g_right = 8.f, g_up = -6.f;   // ik.fwd / ik.right / ik.up (src 0)
float g_scale = kUnitsPerMetre;   // ik.scale (src 1): world units per metre
float g_time = 0.25f, g_const = 0.75f;            // what sub_821BF880 stores at +1876.. / +1884
int32_t g_snap = 0;               // ik.snap
int32_t g_pick = 1;               // ik.pick: 1 = only the character nearest the camera (the player), 0 = everyone
uint32_t g_only = 0;              // ik.only
uint32_t g_vtable = 0;            // ik.vtable
uint32_t g_calls = 0, g_set = 0, g_no_hands = 0;  // ik.calls / ik.set / ik.no_hands
uint32_t g_seen[kSeenMax], g_seen_vt[kSeenMax], g_seen_call[kSeenMax];
float g_seen_dist[kSeenMax];
int g_seen_n = 0;
uint32_t g_player = 0;            // ik.player: the IK object picked as the player's, 0 = none
float g_player_dist = 0.f;        // its shoulders' distance from the camera
char g_dist_text[kSeenMax * 28 + 8] = "none yet";
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
  // DK_HANDS=1 starts with the controllers driving the hands (ik.on=1, ik.src=1), for headset runs with no bridge.
  if (const char* v = std::getenv("DK_HANDS")) {
    if (v[0] == '1') { g_on = 1; g_src = 1; }
    REXGPU_INFO("[IK] DK_HANDS={}: hands from the controllers {}", v, g_on ? "ON" : "off");
  }
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
  bridge_var("ik.pick", BRIDGE_I32, &g_pick, 1, "1 = feed only the character nearest the camera (the player); 0 = all");
  bridge_var("ik.player", BRIDGE_U32, &g_player, 0, "the IK object picked as the player's (0 = nobody near the camera)");
  bridge_var_str("ik.dist", g_dist_text, sizeof(g_dist_text), 0, "each IK object met: address, shoulders to camera (units)");
  bridge_var("ik.calls", BRIDGE_U32, &g_calls, 0, "calls of the explicit-target hand IK (sub_821BF988)");
  bridge_var("ik.set", BRIDGE_U32, &g_set, 0, "calls where a target of ours was written");
  bridge_var("ik.no_hands", BRIDGE_U32, &g_no_hands, 0, "src 1: calls with no controller position to feed");
  bridge_var_str("ik.seen", g_seen_text, sizeof(g_seen_text), 0, "the first IK objects met: address/ClientData vtable");
  bridge_var_str("ik.last", g_last_text, sizeof(g_last_text), 0, "src 1: the newest controller points written");
}

// Called with g_mu held.
int NoteObject(uint8_t* base, uint32_t ik) {
  for (int i = 0; i < g_seen_n; ++i)
    if (g_seen[i] == ik) return i;
  int slot = g_seen_n;
  if (g_seen_n >= kSeenMax) {   // full: the one met longest ago makes room (objects die at a level change)
    slot = 0;
    for (int i = 1; i < kSeenMax; ++i)
      if (g_seen_call[i] < g_seen_call[slot]) slot = i;
  } else {
    ++g_seen_n;
  }
  const uint32_t vt = REX_LOAD_U32(ik - kClientDataBack);
  g_seen[slot] = ik;
  g_seen_vt[slot] = vt;
  g_seen_call[slot] = g_calls;
  g_seen_dist[slot] = 1e9f;
  int p = 0;
  for (int i = 0; i < g_seen_n && p < static_cast<int>(sizeof(g_seen_text)) - 22; ++i)
    p += std::snprintf(g_seen_text + p, sizeof(g_seen_text) - p, "%s%08X/%08X", i ? " " : "", g_seen[i], g_seen_vt[i]);
  REXGPU_INFO("[IK] hand IK object #{} at 0x{:08X}, ClientData vtable 0x{:08X}, flags 0x{:08X}", slot + 1, ik, vt,
              REX_LOAD_U32(ik + kFlags));
  return slot;
}

// Called with g_mu held: how far this object's shoulders are from the camera, then who is nearest now.
void UpdatePick(uint8_t* base, int slot, uint32_t si) {
  float rows[16];
  const uint32_t bones = si ? REX_LOAD_U32(si + kSiBones) : 0u;
  float d = 1e9f;
  if (bones != 0u && dkxr::LastView(rows)) {
    float mid[3] = {0, 0, 0};
    for (uint32_t b : kShoulderBone)
      for (int i = 0; i < 3; ++i) mid[i] += 0.5f * GuestFloat(base, bones + kBoneBytes * b + kBonePos + 4u * i);
    float s = 0;
    for (int i = 0; i < 3; ++i) s += (mid[i] - rows[kRowPos / 4 + i]) * (mid[i] - rows[kRowPos / 4 + i]);
    d = std::sqrt(s);
  }
  g_seen_dist[slot] = d;
  g_seen_call[slot] = g_calls;
  int best = -1;
  for (int i = 0; i < g_seen_n; ++i)
    if (g_calls - g_seen_call[i] <= kPickFresh && g_seen_dist[i] < kPlayerNear &&
        (best < 0 || g_seen_dist[i] < g_seen_dist[best]))
      best = i;
  const uint32_t player = best < 0 ? 0u : g_seen[best];
  // Logged when the player changes CHARACTER (not between the player's two tied objects) or goes.
  if (player != 0u && (g_player == 0u || std::fabs(g_seen_dist[best] - g_player_dist) > kSameCharacter))
    REXGPU_INFO("[IK] the player's hands: IK object 0x{:08X} (shoulders {:.1f} units from the camera)", player,
                g_seen_dist[best]);
  else if (player == 0u && g_player != 0u)
    REXGPU_INFO("[IK] the player's hands: nobody near the camera, no hand is fed");
  g_player = player;
  if (best >= 0) g_player_dist = g_seen_dist[best];
  if (g_calls % kPickFresh == 0) {
    int p = 0;
    for (int i = 0; i < g_seen_n && p < static_cast<int>(sizeof(g_dist_text)) - 28; ++i)
      p += std::snprintf(g_dist_text + p, sizeof(g_dist_text) - p, "%s%08X:%.1f", i ? " " : "", g_seen[i],
                         g_seen_dist[i] > 1e8f ? -1.f : g_seen_dist[i]);
  }
}

// Called with g_mu held: is this object one we feed?
bool Fed(uint8_t* base, uint32_t ik) {
  if (g_only != 0u) return g_only == ik;
  if (g_vtable != 0u && g_vtable != REX_LOAD_U32(ik - kClientDataBack)) return false;
  if (g_pick != 1) return true;
  if (g_player == 0u) return false;
  for (int i = 0; i < g_seen_n; ++i)
    if (g_seen[i] == ik)
      return g_calls - g_seen_call[i] <= kPickFresh && g_seen_dist[i] <= g_player_dist + kSameCharacter;
  return false;
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
    UpdatePick(base, NoteObject(base, ik), ctx.r5.u32);
    if (g_on == 1 && Fed(base, ik)) Feed(base, ik);
    if (g_on == 1 && g_calls % kReportEvery == 0)
      REXGPU_INFO("[IK] calls={} set={} no_hands={} src={} last: {}", g_calls, g_set, g_no_hands, g_src, g_last_text);
  }
  __imp__sub_821BF988(ctx, base);
}

// ---- the controller's ROTATION into the hand (2026-10-28, from the reader's static pass) -------------------------
// sub_821BF988 builds the hand's 64-byte target on its stack (rotation rows from the animated hand bone, position from
// IK+1864/+1892) and hands it to the two-bone solver sub_821EE4E0(r3 = IK+32, ..., r6 = &target). The solver makes
// the target's rows the hand bone's world rotation. So on the solver's ENTRY, for chain 0 (A, right hand) or 1 (B,
// left) of an object whose explicit-target flag is on, the rows are rewritten from the controller:
//   C = the controller's axes in the game world: X right, Y up (out of the top), F pointing (OpenXR grip -Z), from the
//       grip quaternion taken into the head's axes and laid along the head-turned camera rows;
//   O = a fixed 3x3 per hand that maps the controller's axes to the hand bone's axes, from the hand poser
//       (vr_hand_pose.cpp, 2026-10-29: lined up on purpose and saved; it replaced the line-up at the first solve,
//       which took whatever way the controllers were held then, from whichever character solved first);
//   rows = O * C: turning the controller turns the hand by the same turn. Only the player's hands (Fed()).
// ik.rot=1 (or DK_HANDS_ROT=1) switches it on; off by default. Position is untouched (the solver's own +48 row).
// [compile-verified] at most until it has run.
namespace {

constexpr uint32_t kSolverChain = 8u;   // the chain byte at IK+40 = r3+8
constexpr uint32_t kSolverBack = 32u;   // r3 = IK+32

int32_t g_rot = 0;                      // ik.rot
int32_t g_calib = 0;                    // ik.calib: 1 = take a new calibration at the next solve of each hand
uint32_t g_rot_set = 0;                 // ik.rot_set

void RegisterRotVars() {
  static bool done = false;
  if (done) return;
  done = true;
  if (const char* v = std::getenv("DK_HANDS_ROT")) g_rot = v[0] == '1' ? 1 : 0;
  dkpose::Start();
  bridge_var("ik.rot", BRIDGE_I32, &g_rot, 1, "1 = the hands also take the controllers' rotation (src 1)");
  bridge_var("ik.calib", BRIDGE_I32, &g_calib, 1, "1 = line both hands up now (the poser's pose.lineup=3)");
  bridge_var("ik.rot_set", BRIDGE_U32, &g_rot_set, 0, "solves whose hand rotation was written from a controller");
}

// The controller's axes in the world, as rows: out[0] = X (right), out[1] = Y (up), out[2] = F (pointing).
void ControllerAxes(const dkxr::HandSample& h, int hand, const float rows[16], float out[3][3]) {
  float mh[3][3], mg[3][3];
  QuatToRot(h.head_q, mh);
  QuatToRot(h.grip[hand] + 3, mg);
  for (int j = 0; j < 3; ++j) {
    float c[3];   // the controller's axis j in the head's axes: Rh^T * Rg[:, j]
    for (int k = 0; k < 3; ++k) c[k] = mh[0][k] * mg[0][j] + mh[1][k] * mg[1][j] + mh[2][k] * mg[2][j];
    const float sign = j == 2 ? -1.f : 1.f;   // F = -(back axis)
    for (int i = 0; i < 3; ++i)
      out[j][i] = sign * (c[0] * rows[kRowRight / 4 + i] - c[1] * rows[kRowDown / 4 + i] - c[2] * rows[kRowFwd / 4 + i]);
  }
}

}  // namespace

DECLARE_REX_FUNC(sub_821EE4E0);

REX_HOOK_RAW(sub_821EE4E0) {
  RegisterRotVars();
  const uint32_t block = ctx.r3.u32, target = ctx.r6.u32;
  if (g_rot == 1 && g_on == 1 && g_src == 1 && block != 0u && target != 0u) {
    const uint32_t chain = REX_LOAD_U8(block + kSolverChain);
    const uint32_t ik = block - kSolverBack;
    if (chain <= 1u && (REX_LOAD_U32(ik + kFlags) & kOn[chain])) {
      std::lock_guard<std::mutex> lock(g_mu);
      const bool picked = Fed(base, ik);
      const int hand = (static_cast<int>(chain) == (g_right_chain & 1)) ? 1 : 0;   // 1 right, 0 left
      dkxr::HandSample h;
      float rows[16];
      if (picked && dkxr::LastHands(&h) && dkxr::LastView(rows) && (h.flags & (hand ? 0x10u : 0x01u))) {
        float c[3][3];
        ControllerAxes(h, hand, rows, c);
        if (g_calib == 1) { dkpose::RequestLineUp(3u); g_calib = 0; }
        float hm[3][3], out[3][3];
        for (int k = 0; k < 3; ++k)
          for (int i = 0; i < 3; ++i) hm[k][i] = GuestFloat(base, target + 16u * k + 4u * i);
        if (dkpose::HandRows(hand, hm, c, out)) {
          for (int k = 0; k < 3; ++k)
            for (int i = 0; i < 3; ++i) StoreGuestFloat(base, target + 16u * k + 4u * i, out[k][i]);
          ++g_rot_set;
        }
      }
    }
  }
  __imp__sub_821EE4E0(ctx, base);
}
