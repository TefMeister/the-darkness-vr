// The Darkness VR: the headset turns the game camera (DK_XR_HEAD=1), 2026-10-25, Condemned 2's way.
//
// The SDK's patch 09 (DK_XR=1, with DK_SBS=1) sends each side-by-side pair to the headset. This file
// is the game's half, ported from condemned-2-vr's vr_head_pose.cpp:
//  * on the left eye's camera build it samples the head through the SDK's rex_xr_sample_head and
//    keeps it, so the right eye of the pair uses the same head;
//  * vr_viewport_probe.cpp's camera hook (sub_823F9B00) calls TurnCamera before the eye shift, so the
//    shift follows the turned right row;
//  * dk_xr_eye_fov tells the SDK the field of view the player view is drawn with, so the headset shows
//    the picture at its true size instead of stretched to the headset's own view.
//
// Settings (environment): DK_XR_HEAD=1 on. DK_XR_HEAD_MIRROR=1 if the view turns the wrong way (the
// camera's "right" row may point to screen-left, as the eye-shift comment suggests; unproven).
// 2026-10-28: the first headset run's curved head turn was the hook handing TurnCamera the rows in
// the wrong order (the buffer is right, up, forward, not forward, right, up); fixed in the hook.
// DK_XR_TAN_X / DK_XR_TAN_Y: the player view's half-width tangents (default from the authored
// projection, dossier: P00 = 1.07111 at 16:9). DK_XR_FOV=0 stops the export.
// Head POSITION is not applied: the world unit is not measured yet (board row).
// [compile-verified] at most until it has run.

#include "generated/default/darknessrecomp_pch.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstdint>

#include <windows.h>

#include "vr_xr_head.h"

namespace {

constexpr float kTanXDefault = 0.93361f;          // 1 / 1.07111, the player view's P00 (dossier)
constexpr float kAspectDefault = 16.0f / 9.0f;    // all authored projections are 16:9 (dossier)
constexpr uint64_t kReportEverySamples = 600;      // one log line per this many head samples

struct Settings {
  bool xr = false, head = false, mirror = false, fov = true;
  float tan_x = kTanXDefault, tan_y = kTanXDefault / kAspectDefault;
};

const Settings& Get() {
  static const Settings s = [] {
    Settings r;
    if (const char* v = std::getenv("DK_XR")) r.xr = v[0] == '1';
    if (const char* v = std::getenv("DK_XR_HEAD")) r.head = v[0] == '1';
    if (const char* v = std::getenv("DK_XR_HEAD_MIRROR")) r.mirror = v[0] == '1';
    if (const char* v = std::getenv("DK_XR_FOV")) r.fov = v[0] != '0';
    if (const char* v = std::getenv("DK_XR_TAN_X")) r.tan_x = float(std::atof(v));
    if (const char* v = std::getenv("DK_XR_TAN_Y")) r.tan_y = float(std::atof(v));
    REXGPU_INFO("[DKXR] xr={} head={} mirror={} fov export={} tan_x={:.4f} tan_y={:.4f}", r.xr, r.head,
                r.mirror, r.fov, r.tan_x, r.tan_y);
    return r;
  }();
  return s;
}

using SampleFn = int (*)(uint32_t*, float*, float*);
SampleFn Sampler() {
  static const SampleFn fn = [] {
    HMODULE gpu = GetModuleHandleW(L"rexgpu-xenos.dll");
    return gpu ? reinterpret_cast<SampleFn>(
                     reinterpret_cast<void*>(GetProcAddress(gpu, "rex_xr_sample_head")))
               : nullptr;
  }();
  return fn;
}

// The head's rotation as a 3x3 matrix in OpenXR axes (+X right, +Y up, -Z forward), columns = where
// the head's own right, up and back axes point.
struct Rot {
  float m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
};
Rot g_last;  // the newest sample, for builds with no pair number
bool g_have = false;
uint64_t g_samples = 0, g_none = 0;

// One head sample per pair, so the SDK can show each pair with the pose it was drawn with
// (condemned-2-vr vr_head_pose.cpp's ring). Written on the game thread, read by the SDK's GPU thread.
constexpr uint32_t kPairRing = 64;
struct PairHead {
  std::atomic<uint32_t> pair{dkxr::kNoPair};
  std::atomic<uint32_t> id{0};
  Rot rot;
};
PairHead g_pairs[kPairRing];

Rot FromQuat(float x, float y, float z, float w) {
  Rot r;
  r.m[0][0] = 1 - 2 * (y * y + z * z); r.m[0][1] = 2 * (x * y - z * w);     r.m[0][2] = 2 * (x * z + y * w);
  r.m[1][0] = 2 * (x * y + z * w);     r.m[1][1] = 1 - 2 * (x * x + z * z); r.m[1][2] = 2 * (y * z - x * w);
  r.m[2][0] = 2 * (x * z - y * w);     r.m[2][1] = 2 * (y * z + x * w);     r.m[2][2] = 1 - 2 * (x * x + y * y);
  return r;
}

// Samples the head; returns the SDK's sample id (0 = none) and updates g_last.
uint32_t Sample() {
  uint32_t id = 0;
  float o[4] = {0, 0, 0, 1}, p[3] = {0, 0, 0};
  const SampleFn fn = Sampler();
  const bool ok = fn && fn(&id, o, p);
  ++(ok ? g_samples : g_none);
  if (ok) {
    float x = o[0], y = o[1], z = o[2];
    if (Get().mirror) { y = -y; z = -z; }  // mirrored in X: the turn about up and forward reverses
    g_last = FromQuat(x, y, z, o[3]);
    g_have = true;
  }
  if ((g_samples + g_none) % kReportEverySamples == 1)
    REXGPU_INFO("[DKXR] head samples={} none={} q=({:.3f} {:.3f} {:.3f} {:.3f})", g_samples, g_none, o[0],
                o[1], o[2], o[3]);
  return ok ? id : 0u;
}

// The rotation for this camera build: the pair's own sample (taken at its first build), or the
// newest one when no pair number is known.
const Rot* RotFor(bool first_eye, uint32_t pair) {
  if (pair == dkxr::kNoPair) {
    if (first_eye || !g_have) Sample();
    return g_have ? &g_last : nullptr;
  }
  PairHead& slot = g_pairs[pair % kPairRing];
  if (slot.pair.load(std::memory_order_acquire) != pair) {
    const uint32_t id = Sample();
    if (!g_have) return nullptr;
    slot.rot = g_last;
    slot.id.store(id, std::memory_order_relaxed);
    slot.pair.store(pair, std::memory_order_release);
  }
  return &slot.rot;
}

}  // namespace

namespace dkxr {

bool HeadOn() { return Get().xr && Get().head; }

void TurnCamera(float fwd[3], float right[3], float up[3], bool first_eye, uint32_t pair) {
  const Rot* rot = RotFor(first_eye, pair);
  if (rot == nullptr) return;
  const float (&g_m)[3][3] = rot->m;
  // A head-local vector (a, b, c) in OpenXR axes lands in the world at a*right + b*up - c*fwd.
  float f[3], r[3], u[3];
  for (int i = 0; i < 3; ++i) {
    r[i] = g_m[0][0] * right[i] + g_m[1][0] * up[i] - g_m[2][0] * fwd[i];
    u[i] = g_m[0][1] * right[i] + g_m[1][1] * up[i] - g_m[2][1] * fwd[i];
    f[i] = -(g_m[0][2] * right[i] + g_m[1][2] * up[i] - g_m[2][2] * fwd[i]);
  }
  for (int i = 0; i < 3; ++i) { fwd[i] = f[i]; right[i] = r[i]; up[i] = u[i]; }
}

}  // namespace dkxr

// Asked by SDK patch 09: which head sample was pair `pair` drawn with? (2026-10-28; without it the
// SDK showed every pair with the newest pose: sample_hits=0 in the 2026-10-25 and 10-28 logs.)
extern "C" __declspec(dllexport) int dk_xr_pair_sample(uint32_t pair, uint32_t* id) {
  const PairHead& slot = g_pairs[pair % kPairRing];
  if (slot.pair.load(std::memory_order_acquire) != pair) return 0;
  *id = slot.id.load(std::memory_order_relaxed);
  return *id != 0;
}

// Asked by SDK patch 09: the player view's tangents (left, right, up, down), all positive.
extern "C" __declspec(dllexport) int dk_xr_eye_fov(float t[4]) {
  const Settings& s = Get();
  if (!s.fov || s.tan_x <= 0.0f || s.tan_y <= 0.0f) return 0;
  t[0] = s.tan_x; t[1] = s.tan_x; t[2] = s.tan_y; t[3] = s.tan_y;
  return 1;
}
