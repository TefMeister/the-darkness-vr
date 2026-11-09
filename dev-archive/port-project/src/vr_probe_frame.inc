// Part of vr_viewport_probe.cpp (split move-only 2026-11-05: the file had passed the 800-line soft limit).
// It is #included there, in the same translation unit, so it shares that file's state; never compile it alone.
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
    uint32_t slot_serial = serial;  // fx2: game packets carry their GAME sequence number (08b pairs on it)
    if (played && ByPacket()) {
      if (g_play_filler.load(std::memory_order_relaxed)) {
        ++g_sw_filler;
        eye = SkipFillers() ? -4 : g_last_game_eye.load(std::memory_order_relaxed);
      } else {
        ++g_sw_game;
        const uint32_t seq = g_play_game_seq.load(std::memory_order_relaxed);
        const uint64_t fs = g_play_frame_start.load(std::memory_order_relaxed);
        eye = SecondEye(seq) ? 1 : 0;
        slot_serial = seq;
        // A second eye right after its own first eye: how far apart did their frames start?
        if (eye == 1 && g_sw_have_last_game && g_sw_last_game_seq + 1u == seq && fs != 0u &&
            g_sw_last_frame_start != 0u) {
          const double dt_ms = (static_cast<double>(fs) - static_cast<double>(g_sw_last_frame_start)) / 50000.0;
          ++g_pair_n;
          g_pair_dt_sum_ms += dt_ms;
          if (std::fabs(dt_ms) > g_pair_dt_max_ms) {
            g_pair_dt_max_ms = std::fabs(dt_ms);
          }
        }
        g_sw_last_game_seq = seq;
        g_sw_last_frame_start = fs;
        g_sw_have_last_game = true;
      }
    }
    SwapSlot& slot = g_swap_slots[g_swap_index % kSwapSlots];
    slot.serial.store(slot_serial, std::memory_order_relaxed);
    slot.tag.store(((g_swap_index + 1u) << 8) | static_cast<uint64_t>(eye + 8), std::memory_order_release);
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
      REXGPU_INFO("[VP] PAIRS game_swaps={} filler_swaps={} pairs={} frame_start_dt mean={:.2f}ms max={:.2f}ms "
                  "| pins={} capped={} pinned_calls={} exempt_wait_calls={} max_calls_in_one_pin={} "
                  "exact={} cap={:.0f}ms skip_fillers={}",
                  g_sw_game, g_sw_filler, g_pair_n, g_pair_n ? g_pair_dt_sum_ms / g_pair_n : 0.0,
                  g_pair_dt_max_ms, ld(g_fx2_pins), ld(g_fx2_capped), ld(g_fx2_pinned_calls),
                  ld(g_fx2_exempt_calls), ld(g_fx2_max_window_calls), FreezeExact() ? 1 : 0,
                  FreezeCapMs(), SkipFillers() ? 1 : 0);
      g_pair_dt_max_ms = 0.0;
      std::string callers;
      for (const CallerCount& c : g_pin_callers) {
        const uint32_t lr = c.lr.load(std::memory_order_relaxed);
        if (lr != 0u) {
          callers += fmt::format(" 0x{:08X}:{}", lr, c.n.load(std::memory_order_relaxed));
        }
      }
      REXGPU_INFO("[VP] PINCALLERS{}", callers.empty() ? std::string(" none") : callers);
    }
  }

  __imp__sub_82867620(ctx, base);

  ++g_frame_end_calls;
  if (no_swap) {
    ++g_no_swap_calls;
    return;
  }

  ++g_frames;
  bridge_frame_mark();   // 2026-11-05: the bridge's frames / fps (they read 0 before), once per frame that is shown
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
  dkview::NoteCamera();   // b018: in a level (vr_wide_view.cpp)
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

  // The view buffer's rows, 16 bytes each, xyz in the first 3 floats. Measured 2026-10-28 on the
  // OpenXR simulator (see the StereoSettings comment): +0 right, +16 screen-DOWN (minus up),
  // +32 forward, +48 position.
  constexpr uint32_t kRowRight = 0u;
  constexpr uint32_t kRowUp = 16u;
  constexpr uint32_t kRowFwd = 32u;
  constexpr uint32_t kPos = 48u;
  if (out == 0u) return;

  // Once, all four rows, so the layout can be checked against reality rather than assumed.
  // (The 2026-09-18 one-shot printed one unit row and called the layout confirmed; a unit
  // vector proves nothing about which row it is.)
  static bool logged = false;
  if (!logged) {
    logged = true;
    float m[4][3];
    for (uint32_t r = 0; r < 4u; ++r)
      for (uint32_t i = 0; i < 3u; ++i) m[r][i] = LoadGuestFloat(base, out + 16u * r + 4u * i);
    REXGPU_INFO("[VP] camera view rows: +0 right=({:.4f} {:.4f} {:.4f}) +16 up=({:.4f} {:.4f} {:.4f}) "
                "+32 fwd=({:.4f} {:.4f} {:.4f}) +48 pos=({:.2f} {:.2f} {:.2f})",
                m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2],
                m[3][0], m[3][1], m[3][2]);
  }

  // DK_XR_HEAD=1 (vr_xr_head.cpp, 2026-10-25): the head turns the rows first, so the eye shift
  // below follows the turned right row.
  // The +16 row points DOWN on the screen, not up (2026-10-28, simulator, second run: with it
  // handed over as "up" a head turn panned the right way but a nod and a tilt both went the
  // wrong way, which only a flipped up axis does; a tilt of 20 degrees measured as a 20 degree
  // turn of the picture the wrong way). So TurnCamera gets minus that row and writes minus back.
  if (dkxr::HeadOn()) {
    float fwd[3], right[3], up[3];
    for (uint32_t i = 0; i < 3u; ++i) {
      right[i] = LoadGuestFloat(base, out + kRowRight + 4u * i);
      up[i] = -LoadGuestFloat(base, out + kRowUp + 4u * i);
      fwd[i] = LoadGuestFloat(base, out + kRowFwd + 4u * i);
    }
    // The SDK's pair number for this build (08b/09: game sequence / (2 * period)), so both eyes of a
    // pair share one head sample and the SDK can show the pair with it.
    const int period = Stereo().period;
    const uint32_t pair = (ByPacket() && t_packet.has && t_packet.game_seq != 0u && period > 0)
                              ? t_packet.game_seq / (2u * static_cast<uint32_t>(period))
                              : dkxr::kNoPair;
    dkxr::TurnCamera(fwd, right, up, sign > 0.0f, pair);
    for (uint32_t i = 0; i < 3u; ++i) {
      StoreGuestFloat(base, out + kRowRight + 4u * i, right[i]);
      StoreGuestFloat(base, out + kRowUp + 4u * i, -up[i]);
      StoreGuestFloat(base, out + kRowFwd + 4u * i, fwd[i]);
    }
  }

  dkxr::NoteView(base, out);  // the hand hooks read the newest camera from here (2026-10-28)

  const StereoSettings& stereo = Stereo();
  if (stereo.cam_eye == 0.0f) {
    return;
  }

  float right[3], pos[3];
  for (int i = 0; i < 3; ++i) {
    right[i] = LoadGuestFloat(base, out + kRowRight + static_cast<uint32_t>(i) * 4u);
    pos[i] = LoadGuestFloat(base, out + kPos + static_cast<uint32_t>(i) * 4u);
  }

  // The first eye of a pair (sign +1, even serial) lands in the LEFT half of the picture, and the
  // left eye sits on the camera's LEFT: minus the right row. (2026-10-28, simulator: with +e the
  // near hand sat further right in the right half, which is the left eye's view; the halves were
  // swapped. Same note as Condemned 2's "its sign is physical: left eye = -right axis".)
  for (int i = 0; i < 3; ++i) {
    StoreGuestFloat(base, out + kPos + static_cast<uint32_t>(i) * 4u,
                    pos[i] - sign * stereo.cam_eye * right[i]);
  }
}

// The engine's one wall clock. Pinned while the second eye of a pair is being drawn, so
// both eyes see the same instant. See the StereoSettings comment for why it advances by
// a single tick rather than freezing dead.
DECLARE_REX_FUNC(sub_828A7DB8);

REX_HOOK_RAW(sub_828A7DB8) {
  const uint32_t out = ctx.r3.u32;
  const uint32_t caller_lr = static_cast<uint32_t>(ctx.lr);

  __imp__sub_828A7DB8(ctx, base);

  if (out == 0u) {
    return;
  }

  const uint64_t real = (static_cast<uint64_t>(REX_LOAD_U32(out)) << 32) |
                        static_cast<uint64_t>(REX_LOAD_U32(out + 4u));

  ++g_clock_calls;

  if (ByPacket()) {
    // fx2: only the game thread's clock is ever changed, and never inside the queue waits.
    if (!t_is_game_thread) {
      return;
    }
    uint64_t v = real;
    if (t_gc.pin && t_in_queue_wait > 0) {
      g_fx2_exempt_calls.fetch_add(1, std::memory_order_relaxed);
    } else if (t_gc.pin) {
      const double held_ms = std::chrono::duration<double, std::milli>(
                                 std::chrono::steady_clock::now() - t_gc.pin_since).count();
      if (held_ms > FreezeCapMs()) {
        t_gc.pin = false;  // never stall: let go, and count it
        g_fx2_capped.fetch_add(1, std::memory_order_relaxed);
      } else {
        v = FreezeExact() ? t_gc.hold + (++t_gc.k) : t_gc.last_returned + 1u;
        ++t_gc.pin_calls;
        g_fx2_pinned_calls.fetch_add(1, std::memory_order_relaxed);
        CountPinCaller(caller_lr);
        const double hold_ms = (real > v) ? (real - v) / 50000.0 : 0.0;
        if (hold_ms > g_max_hold_ms) {
          g_max_hold_ms = hold_ms;
        }
      }
    }
    if (!t_gc.pin && v <= t_gc.last_returned) {
      v = t_gc.last_returned + 1u;  // after a hold ends the real clock is far ahead; this is a guard
    }
    if (t_gc.first_pending && t_in_queue_wait == 0) {
      t_gc.first_pending = false;
      t_gc.frame_start = v;  // the first read of this frame: taken as the frame's simulation time
    }
    t_gc.last_returned = v;
    if (v != real) {
      REX_STORE_U32(out, static_cast<uint32_t>(v >> 32));
      REX_STORE_U32(out + 4u, static_cast<uint32_t>(v & 0xFFFFFFFFu));
    }
    return;
  }

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
