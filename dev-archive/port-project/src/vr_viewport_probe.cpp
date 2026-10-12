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
//   DK_FREEZE         1 = hold the world still on the second eye's frame (see below).
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
//
// Step 6 (2026-10-12): THE EYE BELONGS TO THE PACKET, NOT TO THE FRAME END. DK_EYE_BY_PACKET=1.
//
// Measured 2026-09-28: the camera (sub_823F9B00) is built on the GAME thread and the frame end
// (sub_82867620) runs on the MAIN thread, 0/1/2 builds per frame end, so flipping the eye at the
// frame end put one eye on both halves of 30-50% of side-by-side pairs. The two threads hand
// frames over through a packet queue (dossier §6): the game thread ACQUIRES a packet
// (sub_825A3FD8, which stamps a global serial 0x82A6910C into packet+5560), fills it, SUBMITS it
// (sub_825A45A8); the main thread PLAYS it (sub_8259E050, r3 = packet) and swaps.
//
// So with DK_EYE_BY_PACKET=1 the serial decides everything:
//   - the eye of a packet = parity of its serial (DK_STEREO_PERIOD packets per eye); even = left;
//   - the camera build uses the eye of the packet its thread is filling (thread-local, set
//     between acquire and submit); if a build happens OUTSIDE that window it uses the serial the
//     next acquire will get (global + 1), and is counted so a run shows which case is real;
//   - the freeze pins the clock from the submit of a first-eye packet to the submit of its
//     second-eye packet, so the second eye is simulated at the first eye's instant whether the
//     simulation runs before or after the acquire;
//   - the main thread records the serial of the packet it plays, and each VdSwap is entered in a
//     numbered table BEFORE it is issued; the side-by-side compose (sdk-patches/08) asks
//     dk_sbs_swap_eye(swap number) and gets that packet's eye. No counting of frames anywhere.
//
// Why serials are immune to what broke counting:
//   - the FILLER packet the main loop acquires itself on a ready-wait timeout (lr 0x820E3358)
//     takes a serial too. It is played like any packet, so its swap is labelled by ITS serial.
//     It can steal the serial the game thread expected; that is counted (predict_miss).
//   - the DRAIN (sub_825A4510) releases ready packets undrawn. They never reach sub_8259E050, so
//     they are never entered against a swap. The shown sequence can then repeat an eye; the
//     compose keeps the latest picture of each eye, so the other half simply stays put, and the
//     repeat is counted (same_eye_twice).
//   - a frame end with no packet played since the last swap (a loading screen) is entered as
//     "no eye" and shown on both halves.
// With DK_EYE_BY_PACKET unset, everything behaves exactly as step 5 (for the A/B).

#include "generated/default/darknessrecomp_pch.h"

#include <rex/hook.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <thread>

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
// Step 5 (2026-09-18): hold the world still for the second eye.
//
// The two eyes of a pair must show the SAME instant, or the pair is not a pair - and
// with the simulation running they are ~130 ms apart, which is also why no measurement
// of the pair has been possible.
//
// The game uses no fixed time step: everything time-dependent reads one wall clock and
// differences it itself. That clock is sub_828A7DB8 (QueryPerformanceCounter -> mftb),
// the only engine-visible timer, 47 callers [measured 2026-09-18]. So pinning its
// answer for the duration of the second eye's frame holds the whole simulation.
//
// Two deliberate choices:
//  - ADVANCE BY ONE TICK (20 ns) per call rather than freezing hard, so any guest loop
//    that polls the clock waiting for a timeout can still expire instead of hanging.
//  - NEVER GO BACKWARDS. The pinned value starts from the latest real reading; a
//    negative time step would throw geometry across the level.
//
// Not the SDK's guest_time_scalar: its vblank worker reads the same scaled clock, so
// slowing guest time also stops vblanks and the swap queue never drains.
struct StereoSettings {
  float eye = 0.0f;
  float cam_eye = 0.0f;
  bool freeze = false;
  bool eye_by_packet = false;  // DK_EYE_BY_PACKET=1, step 6; only acts when period > 0
  int period = 0;  // frames (or packets) per eye; 0 = fixed eye
  enum Target { kNear, kFar, kAll } target = kNear;
};

// Frames seen so far, counted by guest swaps, and the eye in force for the frame now
// being built. Both are touched only on the guest render thread.
uint64_t g_frames = 0;
float g_eye_sign = 1.0f;
// Calls to the frame-end function, and those that ended without a swap (see its hook).
constexpr uint32_t kNoSwapFlagOffset = 21508u;
uint64_t g_frame_end_calls = 0, g_no_swap_calls = 0;
// Camera builds between two frame ends. If the camera is built less often than frames are
// drawn, one eye's camera is drawn twice (2026-09-28 check). Also which thread builds it.
uint32_t g_cam_builds_this_frame = 0;
uint32_t g_cam_builds_histogram[4] = {};
// Near-plane viewport applies since the last swap - kept to measure how many there
// really are per frame, since assuming "one" was wrong once already.
uint32_t g_applies_this_frame = 0;
uint32_t g_applies_histogram[8] = {};
// Guest clock pinning, for the second eye of each pair. Read by every thread that asks the
// clock; set by the main thread (step 5) or the game thread (step 6).
uint64_t g_last_tick = 0;
std::atomic<bool> g_pin_clock{false};
uint64_t g_pinned_frames = 0;
uint64_t g_clock_calls = 0, g_clock_pinned = 0;
double g_max_hold_ms = 0.0;   // largest gap between the real clock and the value handed back

const StereoSettings& Stereo() {
  static const StereoSettings s = [] {
    StereoSettings out;
    if (const char* e = std::getenv("DK_STEREO_EYE")) {
      out.eye = static_cast<float>(std::atof(e));
    }
    if (const char* c = std::getenv("DK_CAM_EYE")) {
      out.cam_eye = static_cast<float>(std::atof(c));
    }
    if (const char* f = std::getenv("DK_FREEZE")) {
      out.freeze = (f[0] == '1');
    }
    if (const char* n = std::getenv("DK_STEREO_PERIOD")) {
      out.period = std::atoi(n);
    }
    if (const char* b = std::getenv("DK_EYE_BY_PACKET")) {
      out.eye_by_packet = (b[0] == '1');
    }
    if (const char* t = std::getenv("DK_STEREO_TARGET")) {
      if (std::strcmp(t, "far") == 0) {
        out.target = StereoSettings::kFar;
      } else if (std::strcmp(t, "all") == 0) {
        out.target = StereoSettings::kAll;
      }
    }
    REXGPU_INFO("[VP] P-offset = {:.3f}, CAMERA-offset = {:.3f}, period = {}, freeze = {}, target = {}, "
                "eye_by_packet = {}{}",
                out.eye, out.cam_eye, out.period, out.freeze ? 1 : 0,
                out.target == StereoSettings::kNear  ? "near"
                : out.target == StereoSettings::kFar ? "far"
                                                     : "all",
                out.eye_by_packet ? 1 : 0,
                (out.eye_by_packet && out.period <= 0) ? " (ignored: needs DK_STEREO_PERIOD > 0)" : "");
    return out;
  }();
  return s;
}

// ---- Step 6: the eye by packet serial ---------------------------------------------------------

// Queue facts, all from the generated code (dossier §6; re-read 2026-10-12):
//   sub_825A3FD8 acquire: lis r9,-32089; lwz r8,-28404(r9); addi r8,r8,1; stw ... -> the global
//     serial lives at 0x82A6910C and is stamped into packet+5560; returns the packet in r3, or 0
//     when its wait times out.
//   Call sites: 0x820E21A4 and 0x820E21E0 (game thread, sub_820E2058, the second is its retry
//     loop) and 0x820E3358 (main loop sub_820E2E50: the FILLER, after a ready-wait timeout).
//   sub_825A45A8 submit(r3 = queue, r4 = packet): one call site, 0x820E2834, game thread.
//   sub_8259E050 play(r3 = packet): three call sites in the main loop; 0x820E38D4 is a second
//     pass over the SAME packet (r5 = 2), so a repeat of the same serial is not a new packet.
//   sub_825A46A0 release(r3 = queue, r4 = packet): from 0x825A4578 it is the DRAIN releasing a
//     packet that was never played.
constexpr uint32_t kSerialGlobal = 0x82A6910Cu;
constexpr uint32_t kPacketSerialOffset = 5560u;
constexpr uint32_t kAcquireLrGame = 0x820E21A4u;
constexpr uint32_t kAcquireLrGameRetry = 0x820E21E0u;
constexpr uint32_t kAcquireLrFiller = 0x820E3358u;
constexpr uint32_t kSubmitLrGame = 0x820E2834u;
constexpr uint32_t kReleaseLrDrain = 0x825A4578u;
constexpr uint64_t kStatsEverySwaps = 300u;

bool ByPacket() {
  const StereoSettings& s = Stereo();
  return s.eye_by_packet && s.period > 0;
}

// Even serials (period 1) are the first eye = LEFT = +offset, as in step 5 ("+e slides the scene
// right, which is what the LEFT eye sees"; even swaps went on the LEFT).
bool SecondEye(uint32_t serial) {
  return ((serial / static_cast<uint32_t>(Stereo().period)) & 1u) != 0u;
}
float SignFor(uint32_t serial) { return SecondEye(serial) ? -1.0f : 1.0f; }

// The packet this thread is filling (game thread, acquire..submit; the main thread's filler,
// acquire..play) or playing (main thread, inside sub_8259E050).
struct PacketOnThread {
  bool has = false;
  uint32_t serial = 0;
};
thread_local PacketOnThread t_packet;

// Counters. Written on the game and main threads, printed from the frame end.
std::atomic<uint64_t> g_pk_game[2];          // packets acquired by the game thread, per eye
std::atomic<uint64_t> g_pk_filler[2];        // filler packets acquired by the main loop, per eye
std::atomic<uint64_t> g_pk_other{0};         // acquires from a call site not listed above
std::atomic<uint64_t> g_pk_timeouts{0};      // acquires that returned no packet
std::atomic<uint64_t> g_pk_drained[2];       // packets released by the drain, never played
std::atomic<uint64_t> g_pk_played[2];        // distinct packets played, per eye
std::atomic<uint64_t> g_predict_miss{0};     // game acquire got a serial other than predicted
std::atomic<uint32_t> g_predicted{0};        // serial the next game acquire should get (+1; 0 = none)
std::atomic<uint64_t> g_cam_in_packet{0}, g_cam_outside{0};
std::atomic<uint64_t> g_pinned_packets{0};

// Main thread: the last packet played and whether one was played since the last swap.
std::atomic<uint32_t> g_play_serial{0};
std::atomic<bool> g_played_since_swap{false};
uint32_t g_last_played_serial_seen = 0xFFFFFFFFu;

// Swaps, entered by number before VdSwap is issued, read by sdk-patches/08 on the GPU thread.
// tag = ((index + 1) << 8) | (eye + 2); 0 = empty. A slot is reused 1024 swaps later, by which
// time the GPU thread has long read it.
struct SwapSlot {
  std::atomic<uint64_t> tag{0};
  std::atomic<uint32_t> serial{0};
};
constexpr uint64_t kSwapSlots = 1024u;
SwapSlot g_swap_slots[kSwapSlots];
uint64_t g_swap_index = 0;  // main thread only
uint64_t g_sw_eye[2] = {}, g_sw_none = 0, g_sw_same_twice = 0, g_sw_serial_gap = 0;
int g_sw_last_eye = -1;
uint32_t g_sw_last_serial = 0;
bool g_sw_have_last_serial = false;
std::atomic<uint64_t> g_lookup_miss{0};

// The eye for a camera or projection being made on THIS thread right now.
float EyeSignForThisThread(uint8_t* base, bool count) {
  if (!ByPacket()) {
    return g_eye_sign;
  }
  if (t_packet.has) {
    if (count) {
      g_cam_in_packet.fetch_add(1, std::memory_order_relaxed);
    }
    return SignFor(t_packet.serial);
  }
  if (count) {
    g_cam_outside.fetch_add(1, std::memory_order_relaxed);
  }
  return SignFor(REX_LOAD_U32(kSerialGlobal) + 1u);
}

// Game thread, after acquire and after submit: hold the clock while the coming packet is a
// second eye. "Coming" = the one being filled (after acquire) or the next one (after submit).
void SetPinFor(uint32_t serial) {
  if (!Stereo().freeze) {
    return;
  }
  const bool pin = SecondEye(serial);
  if (pin && !g_pin_clock.load(std::memory_order_relaxed)) {
    g_pinned_packets.fetch_add(1, std::memory_order_relaxed);
  }
  g_pin_clock.store(pin, std::memory_order_relaxed);
}

}  // namespace

// Asked by the side-by-side compose (sdk-patches/08, D3D12CommandProcessor::IssueSwap) once per
// swap, by swap number counted from 0. Returns 0 left, 1 right, -1 no eye (show on both halves),
// -2 switched off (use the old parity rule), -3 that swap is not in the table (a desync: counted).
extern "C" __declspec(dllexport) int dk_sbs_swap_eye(uint64_t swap_index, uint32_t* serial_out) {
  if (!ByPacket()) {
    return -2;
  }
  const SwapSlot& slot = g_swap_slots[swap_index % kSwapSlots];
  const uint64_t tag = slot.tag.load(std::memory_order_acquire);
  if ((tag >> 8) != swap_index + 1u) {
    g_lookup_miss.fetch_add(1, std::memory_order_relaxed);
    return -3;
  }
  if (serial_out != nullptr) {
    *serial_out = slot.serial.load(std::memory_order_relaxed);
  }
  return static_cast<int>(tag & 0xFFu) - 2;
}

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
      // shows what the game actually used. Step 6: the eye of the packet on this thread.
      const float sign = EyeSignForThisThread(base, false);
      for (int i = 0; i < 4; ++i) {
        p[12 + i] += sign * stereo.eye * p[i];
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
  // This function returns WITHOUT calling VdSwap when device+21508 is set. It is read
  // part-way through; reading it here assumes the calls before that point do not change it
  // (the no_swap_calls count in the log is the check). Counting those calls as
  // frames flipped the eye on a frame nobody saw, so two SHOWN frames in a row carried the
  // same eye: half the side-by-side pairs had one eye on both halves (2026-09-28).
  static bool thread_logged = false;
  if (!thread_logged) {
    thread_logged = true;
    REXGPU_INFO("[VP] frame end thread={}", std::hash<std::thread::id>{}(std::this_thread::get_id()));
  }
  const uint32_t device = ctx.r3.u32;
  const bool no_swap = device != 0u && REX_LOAD_U32(device + kNoSwapFlagOffset) != 0u;

  // Step 6: enter this swap in the table BEFORE the original issues VdSwap, so the GPU thread
  // can never reach this swap before its entry exists. Entered in every mode, so the swap
  // numbers stay aligned with the GPU side if the switch is ever read differently later.
  if (!no_swap) {
    const bool played = g_played_since_swap.exchange(false, std::memory_order_relaxed);
    const uint32_t serial = g_play_serial.load(std::memory_order_relaxed);
    int eye = -1;
    if (played && ByPacket()) {
      eye = SecondEye(serial) ? 1 : 0;
    }
    SwapSlot& slot = g_swap_slots[g_swap_index % kSwapSlots];
    slot.serial.store(serial, std::memory_order_relaxed);
    slot.tag.store(((g_swap_index + 1u) << 8) | static_cast<uint64_t>(eye + 2), std::memory_order_release);
    ++g_swap_index;

    if (eye >= 0) {
      ++g_sw_eye[eye];
      if (eye == g_sw_last_eye) {
        ++g_sw_same_twice;
      }
      g_sw_last_eye = eye;
      if (g_sw_have_last_serial && serial != g_sw_last_serial + 1u) {
        ++g_sw_serial_gap;
      }
      g_sw_last_serial = serial;
      g_sw_have_last_serial = true;
    } else {
      ++g_sw_none;
    }

    if (ByPacket() && (g_swap_index % kStatsEverySwaps) == 0u) {
      auto ld = [](const std::atomic<uint64_t>& a) { return a.load(std::memory_order_relaxed); };
      REXGPU_INFO("[VP] PACKETS game L={} R={} filler L={} R={} other={} timeouts={} drained L={} R={} "
                  "played L={} R={} predict_miss={} cam_in_packet={} cam_outside={} pinned_packets={}",
                  ld(g_pk_game[0]), ld(g_pk_game[1]), ld(g_pk_filler[0]), ld(g_pk_filler[1]),
                  ld(g_pk_other), ld(g_pk_timeouts), ld(g_pk_drained[0]), ld(g_pk_drained[1]),
                  ld(g_pk_played[0]), ld(g_pk_played[1]), ld(g_predict_miss), ld(g_cam_in_packet),
                  ld(g_cam_outside), ld(g_pinned_packets));
      REXGPU_INFO("[VP] SWAPS n={} L={} R={} no_eye={} same_eye_twice={} serial_gaps={} gpu_lookup_miss={} "
                  "last_serial={}",
                  g_swap_index, g_sw_eye[0], g_sw_eye[1], g_sw_none, g_sw_same_twice, g_sw_serial_gap,
                  ld(g_lookup_miss), serial);
    }
  }

  __imp__sub_82867620(ctx, base);

  ++g_frame_end_calls;
  if (no_swap) {
    ++g_no_swap_calls;
    return;
  }

  ++g_frames;
  ++g_cam_builds_histogram[g_cam_builds_this_frame < 3 ? g_cam_builds_this_frame : 3];
  g_cam_builds_this_frame = 0;
  ++g_applies_histogram[g_applies_this_frame < 7 ? g_applies_this_frame : 7];
  g_applies_this_frame = 0;

  const StereoSettings& stereo = Stereo();
  // Step 6 (DK_EYE_BY_PACKET=1) picks the eye and the freeze per packet instead; the per-frame
  // rule below is step 5, kept as it was for the A/B.
  if (!ByPacket() && (stereo.eye != 0.0f || stereo.cam_eye != 0.0f) && stereo.period > 0) {
    const float sign = ((g_frames / static_cast<uint64_t>(stereo.period)) & 1u) ? -1.0f : 1.0f;
    if (sign != g_eye_sign) {
      g_eye_sign = sign;
      // +e slides the scene right, which is what the LEFT eye sees.
      REXGPU_INFO("[VP] EYE={} frame={}", sign > 0.0f ? "L" : "R", g_frames);
    }
  }

  // The swap hook runs at the END of a frame, so g_frames is now the index of the frame
  // about to be drawn. With period 1 the pair is (even, odd): the odd frame is the second
  // eye and must see the same instant as the even one.
  if (!ByPacket() && stereo.freeze && stereo.period > 0) {
    const bool second_eye = ((g_frames / static_cast<uint64_t>(stereo.period)) & 1u) != 0u;
    if (second_eye != g_pin_clock.load(std::memory_order_relaxed)) {
      g_pin_clock.store(second_eye, std::memory_order_relaxed);
      if (second_eye) {
        ++g_pinned_frames;
      }
    }
  }

  if ((g_frames % 300u) == 0u) {
    REXGPU_INFO("[VP] FREEZE frames={} pinned_frames={} clock_calls={} pinned_calls={} max_hold={:.1f}ms"
                " frame_end_calls={} no_swap_calls={}",
                g_frames, g_pinned_frames, g_clock_calls, g_clock_pinned, g_max_hold_ms,
                g_frame_end_calls, g_no_swap_calls);
    g_max_hold_ms = 0.0;
  }

  if ((g_frames % 600u) == 0u) {
    REXGPU_INFO("[VP] frames={} near-viewport applies per frame: 0:{} 1:{} 2:{} 3:{} 4:{} 5:{} 6:{} 7+:{}",
                g_frames, g_applies_histogram[0], g_applies_histogram[1], g_applies_histogram[2],
                g_applies_histogram[3], g_applies_histogram[4], g_applies_histogram[5],
                g_applies_histogram[6], g_applies_histogram[7]);
    REXGPU_INFO("[VP] frames={} camera builds per frame: 0:{} 1:{} 2:{} 3+:{}", g_frames,
                g_cam_builds_histogram[0], g_cam_builds_histogram[1], g_cam_builds_histogram[2],
                g_cam_builds_histogram[3]);
  }
}

// The client view build. Hooked to move the EYE rather than the projection - see the
// StereoSettings comment. r4 is the output buffer and must be read BEFORE the original
// runs, because the original is free to clobber the register.
DECLARE_REX_FUNC(sub_823F9B00);

REX_HOOK_RAW(sub_823F9B00) {
  const uint32_t out = ctx.r4.u32;
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);

  __imp__sub_823F9B00(ctx, base);

  ++g_cam_builds_this_frame;
  static bool thread_logged = false;
  if (!thread_logged) {
    thread_logged = true;
    // Expected caller 0x8249B588 (sub_8249B238, a vtable call); anything else is a second
    // camera path and is logged once below.
    REXGPU_INFO("[VP] camera build thread={} caller=0x{:08X} in_packet={}",
                std::hash<std::thread::id>{}(std::this_thread::get_id()), caller,
                t_packet.has ? 1 : 0);
  }
  static bool other_caller_logged = false;
  if (caller != 0x8249B588u && !other_caller_logged) {
    other_caller_logged = true;
    REXGPU_INFO("[VP] camera build from a SECOND caller=0x{:08X} thread={}", caller,
                std::hash<std::thread::id>{}(std::this_thread::get_id()));
  }

  // The eye of the packet this thread is filling (step 6), or the per-frame eye (step 5).
  // Counted whether or not an offset is applied, so an offset-0 freeze run still shows
  // whether builds happen inside the packet window.
  const float sign = EyeSignForThisThread(base, true);

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
                    pos[i] + sign * stereo.cam_eye * right[i]);
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

// The engine's one wall clock. Pinned while the second eye of a pair is being drawn, so
// both eyes see the same instant. See the StereoSettings comment for why it advances by
// a single tick rather than freezing dead.
DECLARE_REX_FUNC(sub_828A7DB8);

REX_HOOK_RAW(sub_828A7DB8) {
  const uint32_t out = ctx.r3.u32;

  __imp__sub_828A7DB8(ctx, base);

  if (out == 0u) {
    return;
  }

  const uint64_t real = (static_cast<uint64_t>(REX_LOAD_U32(out)) << 32) |
                        static_cast<uint64_t>(REX_LOAD_U32(out + 4u));

  ++g_clock_calls;

  if (!g_pin_clock) {
    // Free-running: follow the real clock, and remember where we are.
    if (real > g_last_tick) {
      g_last_tick = real;
    }
    return;
  }

  // Pinned: hand back the held value, nudged one 20 ns tick so a polling loop can finish.
  // The guest tick is the console's real 50 MHz, so one tick is 20 ns.
  ++g_clock_pinned;
  const double hold_ms = (real > g_last_tick) ? (real - g_last_tick) / 50000.0 : 0.0;
  if (hold_ms > g_max_hold_ms) {
    g_max_hold_ms = hold_ms;
  }
  ++g_last_tick;
  REX_STORE_U32(out, static_cast<uint32_t>(g_last_tick >> 32));
  REX_STORE_U32(out + 4u, static_cast<uint32_t>(g_last_tick & 0xFFFFFFFFu));
}

// ---- Step 6 hooks: the packet queue between the game thread and the main thread ---------------

namespace {
// For the counters only: the eye a serial belongs to, also when the switch is off.
int EyeIndexForCount(uint32_t serial) {
  const int period = Stereo().period;
  return static_cast<int>(period > 0 ? ((serial / static_cast<uint32_t>(period)) & 1u) : (serial & 1u));
}
}  // namespace

// Acquire a free packet and stamp the next serial into it. The caller's return address says who
// asked: the game thread (two sites) or the main loop's filler.
DECLARE_REX_FUNC(sub_825A3FD8);

REX_HOOK_RAW(sub_825A3FD8) {
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);

  __imp__sub_825A3FD8(ctx, base);

  const uint32_t packet = ctx.r3.u32;
  if (packet == 0u) {
    g_pk_timeouts.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const uint32_t serial = REX_LOAD_U32(packet + kPacketSerialOffset);
  const int eye = EyeIndexForCount(serial);

  static bool logged = false;
  if (!logged) {
    logged = true;
    REXGPU_INFO("[VP] packet acquire thread={} caller=0x{:08X} packet=0x{:08X} serial={}",
                std::hash<std::thread::id>{}(std::this_thread::get_id()), caller, packet, serial);
  }

  // This thread now fills this packet: its camera builds take this packet's eye.
  t_packet.has = true;
  t_packet.serial = serial;

  if (caller == kAcquireLrGame || caller == kAcquireLrGameRetry) {
    g_pk_game[eye].fetch_add(1, std::memory_order_relaxed);
    const uint32_t predicted = g_predicted.load(std::memory_order_relaxed);
    if (predicted != 0u && predicted != serial) {
      g_predict_miss.fetch_add(1, std::memory_order_relaxed);
    }
    if (ByPacket()) {
      SetPinFor(serial);
    }
  } else if (caller == kAcquireLrFiller) {
    g_pk_filler[eye].fetch_add(1, std::memory_order_relaxed);
    static bool filler_logged = false;
    if (!filler_logged) {
      filler_logged = true;
      REXGPU_INFO("[VP] first FILLER packet (main loop ready-wait timeout) serial={} thread={}", serial,
                  std::hash<std::thread::id>{}(std::this_thread::get_id()));
    }
  } else {
    g_pk_other.fetch_add(1, std::memory_order_relaxed);
  }
}

// Submit a filled packet to the ready list (r3 = queue, r4 = packet). After the game thread's
// submit, its next camera build and simulation belong to the NEXT packet: predict its serial and
// set the freeze for it.
DECLARE_REX_FUNC(sub_825A45A8);

REX_HOOK_RAW(sub_825A45A8) {
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);

  __imp__sub_825A45A8(ctx, base);

  t_packet.has = false;
  if (caller == kSubmitLrGame) {
    const uint32_t next = REX_LOAD_U32(kSerialGlobal) + 1u;
    g_predicted.store(next, std::memory_order_relaxed);
    if (ByPacket()) {
      SetPinFor(next);
    }
  }
}

// Play a packet to D3D (main thread, r3 = packet). Called a second time for the same packet with
// r5 = 2 (an extra pass), so only a change of serial is a new packet.
DECLARE_REX_FUNC(sub_8259E050);

REX_HOOK_RAW(sub_8259E050) {
  const uint32_t packet = ctx.r3.u32;
  const uint32_t serial = packet != 0u ? REX_LOAD_U32(packet + kPacketSerialOffset) : 0u;

  static bool logged = false;
  if (!logged) {
    logged = true;
    REXGPU_INFO("[VP] packet play thread={} packet=0x{:08X} serial={}",
                std::hash<std::thread::id>{}(std::this_thread::get_id()), packet, serial);
  }

  if (packet != 0u) {
    if (serial != g_last_played_serial_seen) {
      g_last_played_serial_seen = serial;
      g_pk_played[EyeIndexForCount(serial)].fetch_add(1, std::memory_order_relaxed);
    }
    g_play_serial.store(serial, std::memory_order_relaxed);
    g_played_since_swap.store(true, std::memory_order_relaxed);
    // Anything this thread computes while playing (the step-2 projection offset) takes this
    // packet's eye.
    t_packet.has = true;
    t_packet.serial = serial;
  }

  __imp__sub_8259E050(ctx, base);

  // The main thread fills nothing between plays (its filler is acquired and played in one go).
  t_packet.has = false;
}

// Release a packet back to the free list (r3 = queue, r4 = packet). Only counted: from the
// drain's call site it is a packet that was built and never played.
DECLARE_REX_FUNC(sub_825A46A0);

REX_HOOK_RAW(sub_825A46A0) {
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  const uint32_t packet = ctx.r4.u32;
  if (caller == kReleaseLrDrain && packet != 0u) {
    g_pk_drained[EyeIndexForCount(REX_LOAD_U32(packet + kPacketSerialOffset))].fetch_add(
        1, std::memory_order_relaxed);
  }

  __imp__sub_825A46A0(ctx, base);
}

// ---- Step 7 (2026-10-12): VR screen effects off. DK_VR_SCREEN_FX_OFF=1 ---------------------------
//
// Standing rule: in VR every effect painted on the screen goes off. The XR engine's post-process
// sub_825E91D8 (profiler name "CXR_Engine::Engine_PostProcess", r3 = the engine object) reads fields
// that the engine's xr_* script commands write (registered in sub_825EC690 on the same object);
// there is no console to type them, so this hook writes the fields on r3 before the original runs.
//   engine+1192 |= 0x40      xr_ppmbdebugflags(1): the motion-blur pass runs only while this bit is
//                            clear (and engine+1460 has 0x8)            [inferred-static 2026-10-12]
//   engine+1212  = 0.0       xr_ppmbmaxradius: scales the blur vector    [inferred-static 2026-10-12]
//   engine+228  &= ~0x40000000  xr_flares off                            [inferred-static 2026-10-12]
// That the gate is motion blur and that the flare bit hides lens flares is [hypothesis] until a run
// shows it. Old values are logged once, and again whenever the game changes them back.
DECLARE_REX_FUNC(sub_825E91D8);

namespace {
constexpr uint32_t kEngineFlareFlags = 228u;
constexpr uint32_t kFlareEnableBit = 0x40000000u;
constexpr uint32_t kEnginePpMbFlags = 1192u;
constexpr uint32_t kMbDisableBit = 0x40u;
constexpr uint32_t kEngineMbMaxRadius = 1212u;

bool ScreenFxOff() {
  static const bool off = [] {
    const char* e = std::getenv("DK_VR_SCREEN_FX_OFF");
    const bool on = e != nullptr && e[0] == '1';
    REXGPU_INFO("[VP] screen effects off (motion blur, lens flares) = {}", on ? 1 : 0);
    return on;
  }();
  return off;
}
}  // namespace

REX_HOOK_RAW(sub_825E91D8) {
  const uint32_t engine = ctx.r3.u32;
  if (ScreenFxOff() && engine != 0u) {
    const uint32_t flare = REX_LOAD_U32(engine + kEngineFlareFlags);
    const uint32_t mb_flags = REX_LOAD_U32(engine + kEnginePpMbFlags);
    const float mb_radius = LoadGuestFloat(base, engine + kEngineMbMaxRadius);
    const bool changed = (flare & kFlareEnableBit) != 0u || (mb_flags & kMbDisableBit) == 0u ||
                         mb_radius != 0.0f;
    static uint32_t times_logged = 0;
    if (changed && times_logged < 5u) {
      ++times_logged;
      REXGPU_INFO("[VP] screen fx: engine=0x{:08X} before: flares_bit={} mb_flags=0x{:X} mb_radius={:.3f} "
                  "mb_gate(+1460&8)={}",
                  engine, (flare & kFlareEnableBit) ? 1 : 0, mb_flags, mb_radius,
                  (REX_LOAD_U32(engine + 1460u) & 0x8u) ? 1 : 0);
    }
    REX_STORE_U32(engine + kEngineFlareFlags, flare & ~kFlareEnableBit);
    REX_STORE_U32(engine + kEnginePpMbFlags, mb_flags | kMbDisableBit);
    StoreGuestFloat(base, engine + kEngineMbMaxRadius, 0.0f);
  }

  __imp__sub_825E91D8(ctx, base);
}
