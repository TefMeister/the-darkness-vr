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
    REXGPU_INFO("[VP] screen effects off (motion blur, lens flares, hurt screen) = {}", on ? 1 : 0);
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

// ---- 2026-11-06: the HURT SCREEN off (part of DK_VR_SCREEN_FX_OFF; /pd, static, nothing run) ---------------
//
// sub_82125088 (r3 = a player object, its client data at +396) is the per-tick update that eases the hurt amount
// at clientdata+1712 toward a target worked out from health, keeping last tick's value at +1716:
//   stfs prev,1716(r31) ; cur += rate * (target - cur) ; stfs cur,1712(r31)           [inferred-static 2026-11-06]
// The ONLY readers of +1712 in the whole program: the client screen pass sub_821570E8 (3 sites) and sub_821C5C00.
// Each lerps +1716 -> +1712 by +40 and draws its hurt passes (the XREngine_*Hurt shaders, CC_Hurt) only when the
// result is above 0.0001 (float at 0x8209DDDC)                                         [inferred-static 2026-11-06]
// The other writers: the respawn reset sub_821A38C0 writes 0.0 (float at 0x8209DCBC) and sub_821A4820 copies it
// from another client data. So 0.0 in both fields after every update means no hurt screen; the DamageBreath
// sounds are separate and stay. That these passes are the red tint / blur Tefa sees: [hypothesis] until a run.
// fx.hurt_off (live): -1 = follow DK_VR_SCREEN_FX_OFF, 0 = the game's own, 1 = off.
// fx.hurt_seen: the amount the game set on the last update, before we zeroed it (above 0 = hurt, the hook runs).
DECLARE_REX_FUNC(sub_82125088);

namespace {
constexpr uint32_t kObjClientData = 396u;
constexpr uint32_t kCdHurtNow = 1712u;
constexpr uint32_t kCdHurtPrev = 1716u;
constexpr uint32_t kHurtLogLimit = 5u;
int32_t g_hurt_off = -1;
float g_hurt_seen = 0.0f;

void HurtVars() {
  static bool done = false;
  if (done) return;
  done = true;
  bridge_var("fx.hurt_off", BRIDGE_I32, &g_hurt_off, 1,
             "hurt screen (clientdata+1712): -1 = follow DK_VR_SCREEN_FX_OFF, 0 = the game's own, 1 = off");
  bridge_var("fx.hurt_seen", BRIDGE_F32, &g_hurt_seen, 0,
             "hurt amount the game set on the last update, before fx.hurt_off zeroed it (above 0 = hurt)");
}
}  // namespace

REX_HOOK_RAW(sub_82125088) {
  const uint32_t obj = ctx.r3.u32;  // r3 is not kept by the original, so read it first
  __imp__sub_82125088(ctx, base);
  HurtVars();
  if (obj == 0u) return;
  const uint32_t cd = REX_LOAD_U32(obj + kObjClientData);
  if (cd == 0u) return;
  const float now = LoadGuestFloat(base, cd + kCdHurtNow);
  g_hurt_seen = now;
  const bool off = g_hurt_off == 1 || (g_hurt_off == -1 && ScreenFxOff());
  static uint32_t times_logged = 0;
  if (now > 0.0001f && times_logged < kHurtLogLimit) {
    ++times_logged;
    REXGPU_INFO("[VP] hurt screen: clientdata=0x{:08X} amount={:.4f} off={}", cd, now, off ? 1 : 0);
  }
  if (off) {
    StoreGuestFloat(base, cd + kCdHurtNow, 0.0f);
    StoreGuestFloat(base, cd + kCdHurtPrev, 0.0f);
  }
}
