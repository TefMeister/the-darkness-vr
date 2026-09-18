// VR stereo probe, step 1 of 2: READ-ONLY observation of the projection matrix.
//
// Dossier §6 says the per-eye injection point is the end of sub_82249580 ("apply
// viewport"), which copies the current viewport's projection P into the render
// context at RC+17088 and then scales column 0 by 2/width and column 1 by 2/height.
// All of that was established statically and checked against a Python emulation.
// NOTHING has been observed in the running game. This file is that observation, and
// it changes nothing: it calls the original first and only reads afterwards.
//
// The three questions it answers, in order of what would hurt most if wrong:
//   1. Does the function run a handful of times per frame, one per viewport apply?
//   2. Is there a perspective P at all (p[11] == 1.0 and p[15] == 0.0), distinct
//      from the orthographic HUD/menu ones?
//   3. Does the perspective P stay put while the look stick turns the camera? The
//      §6 model says the turn lives in M, not P. If P moves with the stick, the
//      model is wrong and the injection point moves.
//
// Why a weak-symbol hook and not [[midasm_hook]]: the generated code defines every
// guest function as a weak alias of __imp__<name> precisely so a strong definition
// can replace it, and all nine call sites call sub_82249580 rather than __imp__.
// midasm_hook passes only named registers -- no ctx, no base -- so it cannot read
// guest memory, which is the whole job here. It would also force a codegen re-run.

// ---------------------------------------------------------------------------
// Step 2, added after step 1 passed: OPTIONAL per-eye shift, off unless asked for.
//
// Step 1 established, live, that the two projections applied per frame are
// bit-identical whether the look stick is held hard over or not, while the picture
// plainly changes. So the camera turn lives in the view matrix M and P is fixed --
// which is what the injection point depends on.
//
// The shift is P' = T(e) * P with T(e) = identity whose translation row is (e,0,0,1).
// In this engine's row-vector convention that is v*M*T(e)*P = (v*M + e_x)*P: the eye
// moves sideways in VIEW space, which is exactly one eye of a stereo pair. Because
// rows 0..2 of T are identity, the whole operation is "add e * row0 to row3", four
// multiply-adds, and it is applied AFTER the original has written and scaled P.
//
// Controlled by environment variables so no rebuild is needed to change the test:
//   DK_STEREO_EYE     eye offset in view-space units (default 0 = observe only)
//   DK_STEREO_TARGET  which viewport to shift: "near" (the one with the closer near
//                     plane), "far", or "all" (default "near")
//   DK_CAM_EYE        eye offset applied to the CAMERA instead of to P (see below).
//                     Use this OR DK_STEREO_EYE, not both.
//   DK_STEREO_PERIOD  frames per eye. 0 (default) = one fixed eye, the step-2 test.
//                     N > 0 = alternate the SIGN of the offset every N frames: left
//                     eye, then right eye. 1 is true alternate-eye rendering; a large
//                     N (say 45) makes the picture hop once a second or so, which a
//                     window capture can see and a log line can label.
//
// Step 3, the pair (decision recorded in the dossier, 2026-09-18): stereo is made by
// rendering the SAME world twice, one eye per guest frame, offset flipped per frame.
//
// A FRAME IS A SWAP, NOT A VIEWPORT APPLY. The first version counted applies of the
// near-plane viewport, on the belief that it happens once per frame. It does not: the
// interval histogram shows bursts of about three applies within a few milliseconds,
// then a ~66 ms gap [measured 2026-09-18, n=1200 applies]. Flipping on applies
// therefore changed eye in the middle of frames. The eye now flips only in the hook on
// sub_82867620, the one guest function that calls VdSwap, so every draw of a frame
// shares one eye by construction.
// ---------------------------------------------------------------------------

#include "darknessrecomp_pch.h"

#include <rex/hook.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

DECLARE_REX_FUNC(sub_82249580);

namespace {

// Render context, confirmed inside sub_82249580 itself:
// lis r11,-32089; addi r31,r11,-25856  ->  0x82A69B00.
constexpr uint32_t kRenderContext = 0x82A69B00;
constexpr uint32_t kProjection = kRenderContext + 17088;  // 16 floats
constexpr int kMatrixFloats = 16;

// The function runs a few times per frame and logging goes through a lock on the
// render thread, so only distinct matrices are printed. The repeat counter keeps
// the "how often was each one applied" information that filtering would lose.
float g_last[kMatrixFloats];
bool g_have_last = false;
uint32_t g_repeats = 0;
uint64_t g_calls = 0;

inline float LoadGuestFloat(uint8_t* base, uint32_t guest_address) {
  const uint32_t bits = REX_LOAD_U32(guest_address);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

inline void StoreGuestFloat(uint8_t* base, uint32_t guest_address, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  REX_STORE_U32(guest_address, bits);
}

// Read once: getenv on every viewport apply would be silly, and the test never
// changes the offset mid-run.
// Step 4 (2026-09-18): move the offset OUT of P and into the camera.
//
// Putting it in P is geometrically exact but invisible to the CPU. The renderer makes
// camera-dependent decisions on the host side from the UNSHIFTED viewport and camera -
// per-light scissor rectangles (sub_825C3918) above all - so geometry was drawn from
// the shifted eye while each light's screen rectangle was computed for the centre eye.
// With 47-190 px of displacement that cuts lights and shadows in the wrong place, in
// opposite directions for +e and -e, which is what the red/dark heads are.
//
// Shifting the camera instead means every CPU decision, and world-space lighting with
// it, sees the true eye. sub_823F9B00(client, out) builds the client view into the
// buffer in r4: rows at +0 forward, +16 right, +32 up, +48 position. Moving the
// position along the camera's own right axis is the eye offset.
struct StereoSettings {
  float eye = 0.0f;
  float cam_eye = 0.0f;
  int period = 0;  // frames per eye; 0 = fixed eye
  enum Target { kNear, kFar, kAll } target = kNear;
};

// Frames seen so far, counted by guest swaps, and the eye in force for the frame now
// being built. Both are touched only on the guest render thread.
uint64_t g_frames = 0;
float g_eye_sign = 1.0f;
// Near-plane viewport applies since the last swap - kept to measure how many there
// really are per frame, since assuming "one" was wrong once already.
uint32_t g_applies_this_frame = 0;
uint32_t g_applies_histogram[8] = {};

const StereoSettings& Stereo() {
  static const StereoSettings s = [] {
    StereoSettings out;
    if (const char* e = std::getenv("DK_STEREO_EYE")) {
      out.eye = static_cast<float>(std::atof(e));
    }
    if (const char* c = std::getenv("DK_CAM_EYE")) {
      out.cam_eye = static_cast<float>(std::atof(c));
    }
    if (const char* n = std::getenv("DK_STEREO_PERIOD")) {
      out.period = std::atoi(n);
    }
    if (const char* t = std::getenv("DK_STEREO_TARGET")) {
      if (std::strcmp(t, "far") == 0) {
        out.target = StereoSettings::kFar;
      } else if (std::strcmp(t, "all") == 0) {
        out.target = StereoSettings::kAll;
      }
    }
    REXGPU_INFO("[VP] P-offset = {:.3f}, CAMERA-offset = {:.3f}, period = {}, target = {}",
                out.eye, out.cam_eye, out.period,
                out.target == StereoSettings::kNear  ? "near"
                : out.target == StereoSettings::kFar ? "far"
                                                     : "all");
    return out;
  }();
  return s;
}

}  // namespace

REX_HOOK_RAW(sub_82249580) {
  // The original, unchanged and first. Everything below is observation.
  __imp__sub_82249580(ctx, base);

  float p[kMatrixFloats];
  for (int i = 0; i < kMatrixFloats; ++i) {
    p[i] = LoadGuestFloat(base, kProjection + static_cast<uint32_t>(i) * 4u);
  }

  ++g_calls;

  // The two viewports applied per frame during gameplay are told apart by their near
  // plane: p[14] is -Q*zn, about -1.80 for one and -4.01 for the other, and both are
  // rock steady [verified-live 2026-09-18]. Which of the two actually draws the world
  // is the open question this shift answers.
  const StereoSettings& stereo = Stereo();
  if (stereo.eye != 0.0f) {
    const bool is_near_plane_viewport = (p[14] > -3.0f);

    if (is_near_plane_viewport) {
      ++g_applies_this_frame;
    }

    const bool shift_this_one =
        stereo.target == StereoSettings::kAll ||
        (stereo.target == StereoSettings::kNear && is_near_plane_viewport) ||
        (stereo.target == StereoSettings::kFar && !is_near_plane_viewport);

    if (shift_this_one) {
      // P' = T(e) * P: rows 0..2 of T are identity, so only row 3 changes, by
      // e * row0. Write it back and keep reporting the SHIFTED matrix, so the log
      // shows what the game actually used.
      for (int i = 0; i < 4; ++i) {
        p[12 + i] += g_eye_sign * stereo.eye * p[i];
        StoreGuestFloat(base, kProjection + static_cast<uint32_t>(12 + i) * 4u, p[12 + i]);
      }
    }
  }

  if (g_have_last && std::memcmp(p, g_last, sizeof(p)) == 0) {
    ++g_repeats;
    return;
  }

  // p[11] is RC+17132 and p[15] is RC+17148 -- the perspective test from §6.
  const bool perspective = (p[11] == 1.0f && p[15] == 0.0f);

  REXGPU_INFO(
      "[VP] {} call={} prevx{} | {: .5f} {: .5f} {: .5f} {: .5f} | {: .5f} {: .5f} {: .5f} {: .5f} "
      "| {: .5f} {: .5f} {: .5f} {: .5f} | {: .5f} {: .5f} {: .5f} {: .5f}",
      perspective ? "PERSP" : "ortho", g_calls, g_repeats,
      p[0], p[1], p[2], p[3],
      p[4], p[5], p[6], p[7],
      p[8], p[9], p[10], p[11],
      p[12], p[13], p[14], p[15]);

  std::memcpy(g_last, p, sizeof(p));
  g_have_last = true;
  g_repeats = 0;
}

// The end of a guest frame. sub_82867620 is the only generated function that calls
// VdSwap (three call sites reach it), so it is the frame boundary for everything the
// game draws. The eye for the NEXT frame is chosen here, on the guest thread, which
// also means swap number N on the GPU side carries the same parity with no
// cross-thread signalling.
DECLARE_REX_FUNC(sub_82867620);

REX_HOOK_RAW(sub_82867620) {
  __imp__sub_82867620(ctx, base);

  ++g_frames;
  ++g_applies_histogram[g_applies_this_frame < 7 ? g_applies_this_frame : 7];
  g_applies_this_frame = 0;

  const StereoSettings& stereo = Stereo();
  if ((stereo.eye != 0.0f || stereo.cam_eye != 0.0f) && stereo.period > 0) {
    const float sign = ((g_frames / static_cast<uint64_t>(stereo.period)) & 1u) ? -1.0f : 1.0f;
    if (sign != g_eye_sign) {
      g_eye_sign = sign;
      // +e slides the scene right, which is what the LEFT eye sees.
      REXGPU_INFO("[VP] EYE={} frame={}", sign > 0.0f ? "L" : "R", g_frames);
    }
  }

  if ((g_frames % 600u) == 0u) {
    REXGPU_INFO("[VP] frames={} near-viewport applies per frame: 0:{} 1:{} 2:{} 3:{} 4:{} 5:{} 6:{} 7+:{}",
                g_frames, g_applies_histogram[0], g_applies_histogram[1], g_applies_histogram[2],
                g_applies_histogram[3], g_applies_histogram[4], g_applies_histogram[5],
                g_applies_histogram[6], g_applies_histogram[7]);
  }
}

// The client view build. Hooked to move the EYE rather than the projection - see the
// StereoSettings comment. r4 is the output buffer and must be read BEFORE the original
// runs, because the original is free to clobber the register.
DECLARE_REX_FUNC(sub_823F9B00);

REX_HOOK_RAW(sub_823F9B00) {
  const uint32_t out = ctx.r4.u32;

  __imp__sub_823F9B00(ctx, base);

  const StereoSettings& stereo = Stereo();
  if (stereo.cam_eye == 0.0f || out == 0u) {
    return;
  }

  // rows: +0 forward, +16 right, +32 up, +48 position; xyz of each in the first 3 floats
  constexpr uint32_t kRight = 16u;
  constexpr uint32_t kPos = 48u;
  float right[3], pos[3];
  for (int i = 0; i < 3; ++i) {
    right[i] = LoadGuestFloat(base, out + kRight + static_cast<uint32_t>(i) * 4u);
    pos[i] = LoadGuestFloat(base, out + kPos + static_cast<uint32_t>(i) * 4u);
  }

  for (int i = 0; i < 3; ++i) {
    StoreGuestFloat(base, out + kPos + static_cast<uint32_t>(i) * 4u,
                    pos[i] + g_eye_sign * stereo.cam_eye * right[i]);
  }

  // Once, so the row layout can be checked against reality rather than assumed: a unit
  // right vector and a plausible world position mean the guess is right.
  static bool logged = false;
  if (!logged) {
    logged = true;
    const float len = std::sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
    REXGPU_INFO("[VP] camera view: right=({:.4f} {:.4f} {:.4f}) |right|={:.4f} pos=({:.2f} {:.2f} {:.2f})",
                right[0], right[1], right[2], len, pos[0], pos[1], pos[2]);
  }
}
