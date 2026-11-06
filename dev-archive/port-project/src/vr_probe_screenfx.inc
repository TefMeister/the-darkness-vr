// Part of vr_viewport_probe.cpp (split move-only 2026-11-05: the file had passed the 800-line soft limit).
// It is #included there, in the same translation unit, so it shares that file's state; never compile it alone.
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

// 2026-11-06 (Tefa: "it was like the brightness dial flickered super light constantly"): the game's AUTO
// EXPOSURE is the first suspect. Its script command xr_pptoggledynamicexposure (sub_825EC5C8) flips bit 0 of
// engine+656 [inferred-static 2026-11-06]; which state means "on" is not known yet, so the switch has three
// settings and the game's own bit is shown. fx.exposure (DK_VR_EXPOSURE at launch): -1 = leave the game's
// bit alone, 0 = force the bit clear, 1 = force it set. fx.exposure_seen: the bit as the game left it.
constexpr uint32_t kEngineExposureFlags = 656u;
constexpr uint32_t kExposureToggleBit = 0x1u;
int32_t g_exposure_force = [] {
  const char* e = std::getenv("DK_VR_EXPOSURE");
  return e ? std::atoi(e) : -1;
}();
uint32_t g_exposure_seen = 0xFFFFFFFFu;  // 0xFFFFFFFF = the post-process has not run yet

void ExposureVars() {
  static bool done = false;
  if (done) return;
  done = true;
  bridge_var("fx.exposure", BRIDGE_I32, &g_exposure_force, 1,
             "game auto exposure bit (engine+656 bit 0): -1 = leave it, 0 = force clear, 1 = force set");
  bridge_var("fx.exposure_seen", BRIDGE_U32, &g_exposure_seen, 0,
             "engine+656 bit 0 as the game left it (before fx.exposure)");
  REXGPU_INFO("[VP] auto exposure switch: fx.exposure={} (-1 = the game's own)", g_exposure_force);
}

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
  ExposureVars();
  if (engine != 0u) {
    const uint32_t flags = REX_LOAD_U32(engine + kEngineExposureFlags);
    const uint32_t seen = flags & kExposureToggleBit;
    static int32_t logged_force = -2;
    static uint32_t logged_seen = 0xFFFFFFFFu;
    if (g_exposure_force != logged_force || seen != logged_seen) {
      // When we force the bit, the game's own value is overwritten, so "seen" then reads ours unless the game
      // writes it again; only changes are logged.
      REXGPU_INFO("[VP] auto exposure: engine=0x{:08X} game bit={} force={}", engine, seen, g_exposure_force);
      logged_force = g_exposure_force;
      logged_seen = seen;
    }
    g_exposure_seen = seen;
    if (g_exposure_force == 0) {
      REX_STORE_U32(engine + kEngineExposureFlags, flags & ~kExposureToggleBit);
    } else if (g_exposure_force == 1) {
      REX_STORE_U32(engine + kEngineExposureFlags, flags | kExposureToggleBit);
    }
  }
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
