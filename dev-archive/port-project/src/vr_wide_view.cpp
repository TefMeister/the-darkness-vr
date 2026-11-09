// The headset's full view (2026-11-10, b018). Tefa 2026-11-07: "i'm still looking through the 2d window at a depth
// gameplay in it".
//
// WHY IT WAS A WINDOW: the player view is authored as tan_x 0.934 x tan_y 0.525 (86 x 55 degrees) and the headset was
// handed exactly that strip. A Quest 3 eye sees roughly 1.38 x 1.28 in tangents.
//
// WHERE THE VIEW IS WIDENED: in the game's own projection builder, sub_8275EEF8 (dossier section 6: FOV in degrees at
// viewport+260, aspect at +264, "needs rebuilding" byte at +336, kind byte at +256, 0 = perspective by FOV). The matrix
// at RC+17088 is NOT the place: culling, the per-light screen rectangles and the lighting's eye position are worked out
// host-side from the viewport, not from that matrix (dossier, 2026-09-18), so a wider matrix alone would cut lights and
// drop objects at the new edges. Changing the viewport's own two numbers lets the game derive everything itself.
//
// Settings: DK_VIEW_FOV (degrees, the engine's own measure) and DK_VIEW_ASPECT at launch; live through the bridge as
// view.fov / view.aspect (0 = the game's own). view.tan_x / view.tan_y report the half-width tangents the last
// widened view was really built with (read back from the built matrix), and dk_xr_eye_fov hands those to the headset.

#include "generated/default/darknessrecomp_pch.h"

#include <rex/hook.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <windows.h>

extern "C" {
#include "bridge/bridge.h"
}
#include "vr_wide_view.h"

namespace {
float EnvF(const char* n) {
  const char* e = std::getenv(n);
  return e ? float(std::atof(e)) : 0.0f;
}
float g_fov = EnvF("DK_VIEW_FOV");
float g_aspect = EnvF("DK_VIEW_ASPECT");
// The builder keeps pixels square: tan_y = tan(fov/2) / aspect, tan_x = tan_y * width / height [measured 2026-11-10:
// fov 110 aspect 1.2 on 1280x720 -> 2.1158 x 1.1901; the game's own 70 / 1.3333 -> 0.9336 x 0.5252]. On the 16:9
// picture that overshoots sideways, so the width is trimmed with the viewport's own two zoom numbers (+224 x, +228 y,
// both 1.0 as shipped; the builder multiplies its focal lengths by them).
float g_zx = EnvF("DK_VIEW_ZX"), g_zy = EnvF("DK_VIEW_ZY");
// DK_VIEW_AUTO=1 (view.auto): fit the view to the headset by itself, from the tangents the SDK reports
// (rex_xr_headset_tangents: both eyes together). fov = 2 atan(tan_y * aspect), zx = tan_y * (w/h) / tan_x.
int32_t g_auto = int32_t(EnvF("DK_VIEW_AUTO"));
int32_t g_level_only = std::getenv("DK_VIEW_LEVEL_ONLY") ? int32_t(EnvF("DK_VIEW_LEVEL_ONLY")) : 1;
float g_margin = EnvF("DK_VIEW_MARGIN") > 0.0f ? EnvF("DK_VIEW_MARGIN") : 1.0f;
float g_head_tx = 0.0f, g_head_ty = 0.0f;
float g_flat_x = EnvF("DK_VIEW_FLAT_X"), g_flat_y = EnvF("DK_VIEW_FLAT_Y");
uint32_t g_flat_count = 0, g_flat_commit = 0;

// ONLY IN A LEVEL: the menus and films are flat pictures on the same image; over the whole headset view they would
// wrap round the face. The camera build (sub_823F9B00) runs only in a level (dossier: "no camera builds on the menu"),
// so the view is widened only while one ran in the last half second; otherwise each viewport gets its own numbers back.
std::atomic<uint64_t> g_cam_ms{0};
uint64_t NowMs() { return GetTickCount64(); }
bool InLevel() { const uint64_t c = g_cam_ms.load(std::memory_order_relaxed); return c != 0 && NowMs() - c < 500u; }
struct Orig { uint32_t vp = 0; float fov = 0.0f, zx = 1.0f, zy = 1.0f, said_tx = 0.0f, said_ty = 0.0f; int said = 0; };
Orig g_orig[16];
Orig* OrigFor(uint32_t vp) {
  for (Orig& o : g_orig) if (o.vp == vp) return &o;
  for (Orig& o : g_orig) if (o.vp == 0) { o.vp = vp; return &o; }
  return nullptr;
}

using TanFn = int (*)(float*);
void AutoFit(float w, float h, float aspect) {
  static const TanFn fn = [] {
    HMODULE gpu = GetModuleHandleW(L"rexgpu-xenos.dll");
    return gpu ? reinterpret_cast<TanFn>(reinterpret_cast<void*>(GetProcAddress(gpu, "rex_xr_headset_tangents"))) : nullptr;
  }();
  float t[4];
  if (!fn || !fn(t) || !(w > 1.0f) || !(h > 1.0f) || !(aspect > 0.01f)) return;
  const float tx = std::fmax(t[0], t[1]) * g_margin, ty = std::fmax(t[2], t[3]) * g_margin;
  if (!(tx > 0.2f) || !(ty > 0.2f) || tx > 5.0f || ty > 5.0f) return;
  const bool news = std::fabs(tx - g_head_tx) >= 0.002f || std::fabs(ty - g_head_ty) >= 0.002f;
  g_head_tx = tx;
  g_head_ty = ty;
  g_fov = 2.0f * std::atan(ty * aspect) * 57.29578f;   // every time: view.auto back to 1 must take the dials back
  g_zx = ty * (w / h) / tx;
  g_zy = 1.0f;
  if (news) REXGPU_INFO("[VIEW] fitted to the headset: tangents {:.4f} x {:.4f} -> fov {:.2f}, zoom x {:.4f}", tx, ty, g_fov, g_zx);
}
float g_tan_x = 0.0f, g_tan_y = 0.0f;       // as built, the last widened perspective view
float g_game_fov = 0.0f, g_game_aspect = 0.0f;  // what the game last asked for itself
std::atomic<uint32_t> g_tan_bits_x{0}, g_tan_bits_y{0};

inline float LoadF(uint8_t* base, uint32_t a) {
  const uint32_t bits = REX_LOAD_U32(a);
  float v;
  std::memcpy(&v, &bits, sizeof v);
  return v;
}
inline void StoreF(uint8_t* base, uint32_t a, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, sizeof bits);
  REX_STORE_U32(a, bits);
}
void Vars() {
  static bool done = false;
  if (done) return;
  done = true;
  bridge_var("view.fov", BRIDGE_F32, &g_fov, 1, "the world view's FOV in the engine's degrees (0 = the game's own, 70)");
  bridge_var("view.aspect", BRIDGE_F32, &g_aspect, 1, "the world view's aspect (0 = the game's own)");
  bridge_var("view.zx", BRIDGE_F32, &g_zx, 1, "sideways zoom of the world view (viewport+224; 0 = leave, 1 = none): trims the width");
  bridge_var("view.zy", BRIDGE_F32, &g_zy, 1, "up-down zoom of the world view (viewport+228; 0 = leave)");
  bridge_var("view.level_only", BRIDGE_I32, &g_level_only, 1, "1 = widen only in a level (menus and films keep the game's own view)");
  bridge_var("view.auto", BRIDGE_I32, &g_auto, 1, "1 = fit fov and zoom to the headset's own view by itself");
  bridge_var("view.flat_x", BRIDGE_F32, &g_flat_x, 1, "half-width (tangent) the HUD / menus / films are shrunk to in the wide view (0 = leave)");
  bridge_var("view.flat_y", BRIDGE_F32, &g_flat_y, 1, "half-height (tangent) for the same");
  bridge_var("view.flat_count", BRIDGE_U32, &g_flat_count, 0, "flat projections shrunk so far");
  bridge_var("view.flat_commit", BRIDGE_U32, &g_flat_commit, 0, "of those, caught at the pre-draw commit");
  bridge_var("view.tan_x", BRIDGE_F32, &g_tan_x, 0, "half-width tangent the widened view was built with");
  bridge_var("view.tan_y", BRIDGE_F32, &g_tan_y, 0, "half-height tangent the widened view was built with");
  bridge_var("view.game_fov", BRIDGE_F32, &g_game_fov, 0, "the FOV the game last asked for itself");
  bridge_var("view.game_aspect", BRIDGE_F32, &g_game_aspect, 0, "the aspect the game last asked for itself");
  REXGPU_INFO("[VIEW] wide view: fov={} aspect={} (0 = the game's own)", g_fov, g_aspect);
}
}  // namespace

namespace dkview {
void Flat(uint8_t* base, uint32_t projection);
void NoteCamera() { g_cam_ms.store(NowMs(), std::memory_order_relaxed); }
bool Tangents(float* tx, float* ty) {
  if (g_level_only != 0 && !InLevel()) return false;
  if (g_fov <= 0.0f && g_aspect <= 0.0f && g_zx <= 0.0f && g_zy <= 0.0f) return false;
  uint32_t bx = g_tan_bits_x.load(std::memory_order_relaxed), by = g_tan_bits_y.load(std::memory_order_relaxed);
  float x, y;
  std::memcpy(&x, &bx, sizeof x);
  std::memcpy(&y, &by, sizeof y);
  if (!(x > 0.05f) || !(y > 0.05f)) return false;
  *tx = x;
  *ty = y;
  return true;
}
// THE FLAT THINGS (HUD, menus, films): they are drawn straight onto the same picture, which now covers the whole
// headset view, so they would wrap round the face, stretched tall. With view.flat_x / view.flat_y set (half-sizes
// in tangents, 0 = leave them), every flat projection is shrunk about the middle to that size. Called from the
// apply-viewport hook (vr_viewport_probe.cpp) after the game has written its matrix at RC+17088.
// The HUD's flat projection does not come through apply-viewport [measured 2026-11-10: view.flat_count stayed 0 in
// play], so the same shrink is also offered at the pre-draw commit (sub_82249488, vr_skin_probe.cpp), where whatever
// projection is about to reach the shaders sits at RC+17088. A matrix we already shrank is recognised by its bits.
void FlatAtCommit(uint8_t* base, uint32_t render_context) {
  if (!(g_flat_x > 0.0f) || !(g_flat_y > 0.0f)) return;
  const uint32_t projection = render_context + 17088u;
  if (LoadF(base, projection + 44u) != 0.0f || LoadF(base, projection + 60u) != 1.0f) return;
  static thread_local uint32_t done[16];
  static thread_local bool have = false;
  uint32_t now[16];
  for (uint32_t i = 0; i < 16u; ++i) now[i] = REX_LOAD_U32(projection + 4u * i);
  if (have && std::memcmp(now, done, sizeof now) == 0) return;
  const uint64_t before = g_flat_count;
  Flat(base, projection);
  if (g_flat_count == before) return;
  for (uint32_t i = 0; i < 16u; ++i) done[i] = REX_LOAD_U32(projection + 4u * i);
  have = true;
  REX_STORE_U32(render_context + 8228u, REX_LOAD_U32(render_context + 8228u) | 1u);   // "matrices changed"
  ++g_flat_commit;
}
void Flat(uint8_t* base, uint32_t projection) {
  if (!(g_flat_x > 0.0f) || !(g_flat_y > 0.0f)) return;
  float tx, ty;
  if (!Tangents(&tx, &ty)) return;
  if (LoadF(base, projection + 44u) != 0.0f || LoadF(base, projection + 60u) != 1.0f) return;   // flat ones only
  const float sx = g_flat_x / tx, sy = g_flat_y / ty;
  for (uint32_t row = 0; row < 4u; ++row) {
    StoreF(base, projection + row * 16u + 0u, LoadF(base, projection + row * 16u + 0u) * sx);
    StoreF(base, projection + row * 16u + 4u, LoadF(base, projection + row * 16u + 4u) * sy);
  }
  ++g_flat_count;
}
}  // namespace dkview

DECLARE_REX_FUNC(sub_8275EEF8);

REX_HOOK_RAW(sub_8275EEF8) {
  Vars();
  const uint32_t vp = ctx.r3.u32;
  const bool by_fov = REX_LOAD_U8(vp + 256) == 0;
  bool ours = false;
  if (by_fov) {
    const float fov = LoadF(base, vp + 260), aspect = LoadF(base, vp + 264);
    const bool active = g_level_only == 0 || InLevel();
    Orig* o = OrigFor(vp);
    // the game's own numbers: whatever is there that is not what we wrote
    const bool fov_is_ours = g_fov > 0.0f && std::fabs(fov - g_fov) <= 0.01f;
    if (!fov_is_ours) { g_game_fov = fov; if (o) o->fov = fov; }
    if (g_aspect <= 0.0f || std::fabs(aspect - g_aspect) > 0.0005f) g_game_aspect = aspect;
    const float zx = LoadF(base, vp + 224), zy = LoadF(base, vp + 228);
    if (o && !(g_zx > 0.0f && std::fabs(zx - g_zx) <= 0.0005f)) o->zx = zx;
    if (o && !(g_zy > 0.0f && std::fabs(zy - g_zy) <= 0.0005f)) o->zy = zy;
    if (active) {
      if (g_auto == 1) {
        AutoFit(float(int32_t(REX_LOAD_U32(vp + 324)) - int32_t(REX_LOAD_U32(vp + 316))),
                float(int32_t(REX_LOAD_U32(vp + 328)) - int32_t(REX_LOAD_U32(vp + 320))),
                g_aspect > 0.0f ? g_aspect : aspect);
      }
      if (g_zx > 0.0f) { ours = true; if (std::fabs(zx - g_zx) > 0.0005f) { StoreF(base, vp + 224, g_zx); REX_STORE_U8(vp + 336, 1); } }
      if (g_zy > 0.0f) { ours = true; if (std::fabs(zy - g_zy) > 0.0005f) { StoreF(base, vp + 228, g_zy); REX_STORE_U8(vp + 336, 1); } }
      if (g_fov > 0.0f) { ours = true; if (!fov_is_ours) { StoreF(base, vp + 260, g_fov); REX_STORE_U8(vp + 336, 1); } }
      if (g_aspect > 0.0f) { ours = true; if (std::fabs(aspect - g_aspect) > 0.0005f) { StoreF(base, vp + 264, g_aspect); REX_STORE_U8(vp + 336, 1); } }
    } else if (o) {   // out of the level: this viewport gets its own numbers back
      if (fov_is_ours && o->fov > 0.0f) { StoreF(base, vp + 260, o->fov); REX_STORE_U8(vp + 336, 1); }
      if (g_zx > 0.0f && std::fabs(zx - g_zx) <= 0.0005f && std::fabs(o->zx - g_zx) > 0.0005f) { StoreF(base, vp + 224, o->zx); REX_STORE_U8(vp + 336, 1); }
      if (g_zy > 0.0f && std::fabs(zy - g_zy) <= 0.0005f && std::fabs(o->zy - g_zy) > 0.0005f) { StoreF(base, vp + 228, o->zy); REX_STORE_U8(vp + 336, 1); }
    }
  }
  const bool rebuilt = REX_LOAD_U8(vp + 336) != 0;
  __imp__sub_8275EEF8(ctx, base);
  if (!by_fov || !rebuilt) return;
  const float w = float(int32_t(REX_LOAD_U32(vp + 324)) - int32_t(REX_LOAD_U32(vp + 316)));
  const float h = float(int32_t(REX_LOAD_U32(vp + 328)) - int32_t(REX_LOAD_U32(vp + 320)));
  const float p00 = LoadF(base, vp + 0), p11 = LoadF(base, vp + 20);
  if (!(w > 0.0f) || !(h > 0.0f) || p00 == 0.0f || p11 == 0.0f) return;
  const float tx = w / (2.0f * std::fabs(p00)), ty = h / (2.0f * std::fabs(p11));
  // one line per viewport and change, at most 6 per viewport (the menu's two views swap numbers every frame)
  Orig* said = OrigFor(vp);
  if (said == nullptr || said->said >= 6) return;
  if (ours) {
    g_tan_x = tx;
    g_tan_y = ty;
    uint32_t bx, by;
    std::memcpy(&bx, &tx, sizeof bx);
    std::memcpy(&by, &ty, sizeof by);
    g_tan_bits_x.store(bx, std::memory_order_relaxed);
    g_tan_bits_y.store(by, std::memory_order_relaxed);
  }
  if (std::fabs(tx - said->said_tx) > 0.001f || std::fabs(ty - said->said_ty) > 0.001f) {
    said->said_tx = tx;
    said->said_ty = ty;
    ++said->said;
    REXGPU_INFO("[VIEW] a perspective view built: {}x{} fov {:.2f} aspect {:.4f} -> tan_x {:.4f} tan_y {:.4f} ({}); "
                "+224 {:.4f} +228 {:.4f} +276 {:.4f} +288 {:.4f}",
                int(w), int(h), LoadF(base, vp + 260), LoadF(base, vp + 264), tx, ty, ours ? "widened" : "the game's own",
                LoadF(base, vp + 224), LoadF(base, vp + 228), LoadF(base, vp + 276), LoadF(base, vp + 288));
  }
}
