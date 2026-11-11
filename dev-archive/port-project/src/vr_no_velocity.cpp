// vr_no_velocity.cpp -- the velocity ("MotionMap") pass is not drawn while motion blur is off (2026-11-11).
//
// sub_82276148 emits the batch of draws tagged XRShader_MotionMap (the tag pointer 0x8206764C is stored
// into the batch header at the top of the routine): every moving and static object drawn once more into
// a velocity picture that only the motion-blur pass reads. The VR build switches motion blur off
// (DK_VR_SCREEN_FX_OFF=1), so nothing reads that picture. The routine already returns without emitting
// anything when its list is empty (loc_82276960: the epilogue only), and both callers (sub_825CF178,
// sub_825BD270) ignore a return value, so returning at once is a path the game has itself.
// Lead: the other public port's RENDERING.md, "Redundant geometry passes" [reported 2026-11-11].
//
// OFF unless DK_VR_NO_VELOCITY=1. Read once. Measured 2026-11-11 (simulator, test copy): about 1,200 batches a
// second skipped, each with one list entry, and no change in the frame rate in three standing scenes
// (Chinatown 116 / 116, the otherworld 151 / 149 game frames a second, on / off), the picture the same. So it
// stays a switch for a busy scene with many moving things, not a default.

#include "generated/default/darknessrecomp_pch.h"

#include <rex/hook.h>

#include <atomic>
#include <cstdlib>

namespace {
constexpr uint64_t kLogEvery = 100000;   // one log line per this many skipped batches

bool NoVelocityOn() {
  static const bool on = [] {
    const char* own = std::getenv("DK_VR_NO_VELOCITY");
    return own && own[0] == '1';
  }();
  return on;
}
}  // namespace

DECLARE_REX_FUNC(sub_82276148);

REX_HOOK_RAW(sub_82276148) {
  if (!NoVelocityOn()) {
    __imp__sub_82276148(ctx, base);
    return;
  }
  static std::atomic<uint64_t> skipped{0};
  const uint64_t n = skipped.fetch_add(1, std::memory_order_relaxed);
  if (n % kLogEvery == 0) {
    REXLOG_INFO("[VP] velocity pass not drawn: {} batches skipped so far (list entries in this one: {})", n,
                REX_LOAD_U16(ctx.r5.u32 + 10));
  }
}
