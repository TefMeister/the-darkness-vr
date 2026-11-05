// Part of vr_viewport_probe.cpp (split move-only 2026-11-05: the file had passed the 800-line soft limit).
// It is #included there, in the same translation unit, so it shares that file's state; never compile it alone.
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

  ++t_in_queue_wait;  // fx2: its clock polls (the wait for a free packet) are never pinned
  __imp__sub_825A3FD8(ctx, base);
  --t_in_queue_wait;

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
    // fx2: the eye and the freeze follow GAME packets only.
    const uint32_t seq = g_game_seq.fetch_add(1, std::memory_order_relaxed) + 1u;
    t_packet.game_seq = seq;
    t_is_game_thread = true;
    if (ByPacket()) {
      t_packet.sign = SignFor(seq);
      g_last_game_eye.store(SecondEye(seq) ? 1 : 0, std::memory_order_relaxed);
    }
    {
      std::lock_guard<std::mutex> lock(g_meta_mu);
      PacketMeta* m = MetaLocked(packet);
      m->serial = serial;
      m->game_seq = seq;
      m->filler = false;
      m->frame_start = 0;
    }
  } else if (caller == kAcquireLrFiller) {
    t_packet.game_seq = 0u;
    g_pk_filler[eye].fetch_add(1, std::memory_order_relaxed);
    t_packet.sign = g_last_game_eye.load(std::memory_order_relaxed) ? -1.0f : 1.0f;
    {
      std::lock_guard<std::mutex> lock(g_meta_mu);
      PacketMeta* m = MetaLocked(packet);
      m->serial = serial;
      m->game_seq = 0;
      m->filler = true;
      m->frame_start = 0;
    }
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
  const uint32_t packet = ctx.r4.u32;

  __imp__sub_825A45A8(ctx, base);

  t_packet.has = false;
  if (caller == kSubmitLrGame) {
    const uint32_t next = REX_LOAD_U32(kSerialGlobal) + 1u;
    g_predicted.store(next, std::memory_order_relaxed);
    // fx2: remember when this packet's frame started, then decide the freeze for the NEXT frame.
    {
      std::lock_guard<std::mutex> lock(g_meta_mu);
      MetaLocked(packet)->frame_start = t_gc.frame_start;
    }
    if (t_gc.pin_calls > g_fx2_max_window_calls.load(std::memory_order_relaxed)) {
      g_fx2_max_window_calls.store(t_gc.pin_calls, std::memory_order_relaxed);
    }
    const uint64_t this_frame_start = t_gc.frame_start;
    t_gc.first_pending = true;
    t_gc.pin = false;
    if (ByPacket() && Stereo().freeze) {
      const uint32_t next_seq = g_game_seq.load(std::memory_order_relaxed) + 1u;
      if (SecondEye(next_seq)) {
        // The next frame is a second eye: give it this (first-eye) frame's start time.
        t_gc.pin = true;
        t_gc.hold = FreezeExact() ? this_frame_start : t_gc.last_returned;
        t_gc.k = 0;
        t_gc.pin_calls = 0;
        t_gc.pin_since = std::chrono::steady_clock::now();
        g_fx2_pins.fetch_add(1, std::memory_order_relaxed);
        g_pinned_packets.fetch_add(1, std::memory_order_relaxed);
      }
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
    // fx2: what kind of packet this is, its game sequence number and frame start.
    PacketMeta m;
    {
      std::lock_guard<std::mutex> lock(g_meta_mu);
      m = *MetaLocked(packet);
    }
    g_play_filler.store(m.filler || m.game_seq == 0u, std::memory_order_relaxed);
    g_play_game_seq.store(m.game_seq, std::memory_order_relaxed);
    g_play_frame_start.store(m.frame_start, std::memory_order_relaxed);
    g_played_since_swap.store(true, std::memory_order_relaxed);
    // Anything this thread computes while playing (the step-2 projection offset) takes this
    // packet's eye.
    t_packet.has = true;
    t_packet.serial = serial;
    t_packet.sign = (m.game_seq != 0u && ByPacket()) ? SignFor(m.game_seq)
                    : (g_last_game_eye.load(std::memory_order_relaxed) ? -1.0f : 1.0f);
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
