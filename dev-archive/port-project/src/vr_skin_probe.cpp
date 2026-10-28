// The Darkness VR: where do the bones go? A read-only probe on the pre-draw commit (DK_SKIN_PROBE=1).
//
// Static reading (dossier 6e, 2026-10-28) found the vertex constants' host shadow: an object pointed to
// by RC+15748 (RC = 0x82A69B00, the render context), a 64-bit dirty mask at its +0 (sub_82248A78 sets
// bits 62-63 of it when it writes c0-c7) and the constants themselves at +1920, 16 bytes each. The
// vertex shader skins from a matrix palette above c7 (System/Gl/VP.xrg), but no function was found that
// writes it. So, instead of reading more code: watch the mask at every pre-draw commit (sub_82249488,
// the only caller of the c0-c7 writer) BEFORE it runs, and the writer shows itself by the bits it set.
//
// Logs, as [SKIN] lines: every kReportEvery commits a summary (commits, commits with bits below 62 set,
// the lowest and highest such bit seen, a table of the distinct masks seen), and once, for the first
// commit whose mask has at least kDumpMinBits bits set below 62, the first kDumpFloats floats of the
// lowest dirty group under the guess "bit 63 - k = constants 4k..4k+3" (c0-c3 at bit 63 is what
// sub_82248A78's 3<<62 suggests; the dump says whether the guess holds).
//
// The dump half reads only. The nudge half (below, bridge vars skin.*) writes one bone's three constants in the
// shadow before the commit; off unless skin.bone is set.

#include "generated/default/darknessrecomp_pch.h"

#include <rex/hook.h>

extern "C" {
#include "bridge/bridge.h"
}

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {

constexpr uint32_t kRenderContext = 0x82A69B00u;   // RC (dossier 6)
constexpr uint32_t kShadowPtrOffset = 15748u;      // RC+15748 -> the constant shadow object
constexpr uint32_t kConstantsOffset = 1920u;       // shadow + 1920 + 16 * i = constant i
constexpr uint64_t kReportEvery = 4000;            // commits between summary lines
constexpr int kMaskTable = 12;                     // distinct masks remembered

bool On() {
  static const bool on = [] {
    const char* v = std::getenv("DK_SKIN_PROBE");
    const bool r = v != nullptr && v[0] == '1';
    if (r) REXGPU_INFO("[SKIN] probe on: watching the constant dirty mask at every pre-draw commit");
    return r;
  }();
  return on;
}

inline float GuestFloat(uint8_t* base, uint32_t guest_address) {
  const uint32_t bits = REX_LOAD_U32(guest_address);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::mutex g_mu;
uint64_t g_commits = 0, g_low_commits = 0, g_no_shadow = 0;
int g_low_min = 64, g_low_max = -1;
uint64_t g_masks[kMaskTable];
uint64_t g_mask_counts[kMaskTable];
int g_mask_n = 0;
constexpr int kDumps = 12;             // commits dumped in full
constexpr int kDumpParts = 8;          // 16 constants per part: c8..c135
constexpr int kDumpMinConstants = 16;  // non-zero constants in that window that make a dump worth taking
constexpr uint64_t kDumpGap = 997;     // commits between two dumps
int g_dumps = 0;
uint64_t g_last_dump = 0;

void Note(uint64_t mask) {
  for (int i = 0; i < g_mask_n; ++i) {
    if (g_masks[i] == mask) { ++g_mask_counts[i]; return; }
  }
  if (g_mask_n < kMaskTable) { g_masks[g_mask_n] = mask; g_mask_counts[g_mask_n] = 1; ++g_mask_n; }
}

// ---- which bone is the hand? Nudge ONE bone, picked live through the bridge (2026-10-28) ----------------
// The bone palette starts at c96, three constants per bone (a 4x3: rows = rotation rows, translation in .w)
// [verified-live 2026-10-28]. To learn which index is the hand without a relaunch per bone, the bridge vars
// below pick a bone and a nudge; the hook applies it to that bone's three constants, in the shadow, before the
// original commit uploads them. Mode 1 collapses the bone (rotation rows zeroed: its vertices pinch to a point),
// mode 2 shifts it (dx dy dz added to the three .w translations, in the model's own units ~ inches). A run with
// skin.bone = -1 (the default) changes nothing. DK_SKIN_BONE=<b> sets the start value for runs without a bridge.
constexpr uint32_t kPaletteFirst = 96u;   // c96 = bone 0
constexpr int kBoneMax = 53;              // c96 + 3*53 = c255, the last vertex constant
constexpr uint32_t kMaxShadowMask = 0xFFFFFFFFu;

int32_t g_bone = -1;          // skin.bone: which bone to nudge, -1 = none
int32_t g_mode = 2;           // skin.mode: 1 collapse, 2 shift
float g_dx = 10.f, g_dy = 10.f, g_dz = 10.f;   // skin.dx/dy/dz: the shift (mode 2)
int32_t g_dirty = 0;          // skin.dirty: 1 = also set the shadow's 64-bit mask to all ones
uint32_t g_hits = 0;          // skin.hits: commits where a bone was nudged (a table was there)
uint32_t g_tables = 0;        // skin.tables: commits whose c96 row looked like a bone row
int32_t g_scan_done = 0;      // skin.scan: 1 = next commit, log the first row of every bone (which look real)

inline void StoreGuestFloat(uint8_t* base, uint32_t guest_address, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  REX_STORE_U32(guest_address, bits);
}

bool UnitRow(uint8_t* base, uint32_t c) {
  const float x = GuestFloat(base, c), y = GuestFloat(base, c + 4u), z = GuestFloat(base, c + 8u);
  const float l2 = x * x + y * y + z * z;
  return l2 > 0.9f && l2 < 1.1f;
}

void RegisterVars() {
  static bool done = false;
  if (done) return;
  done = true;
  if (const char* v = std::getenv("DK_SKIN_BONE")) g_bone = std::atoi(v);
  bridge_var("skin.bone", BRIDGE_I32, &g_bone, 1, "bone to nudge in the skin palette (c96 + 3*bone); -1 = none");
  bridge_var("skin.mode", BRIDGE_I32, &g_mode, 1, "1 = collapse the bone (rotation rows zeroed), 2 = shift it by dx dy dz");
  bridge_var("skin.dx", BRIDGE_F32, &g_dx, 1, "shift along the model's x (mode 2), model units");
  bridge_var("skin.dy", BRIDGE_F32, &g_dy, 1, "shift along the model's y (mode 2)");
  bridge_var("skin.dz", BRIDGE_F32, &g_dz, 1, "shift along the model's z (mode 2)");
  bridge_var("skin.dirty", BRIDGE_I32, &g_dirty, 1, "1 = also mark the whole constant shadow dirty when nudging");
  bridge_var("skin.hits", BRIDGE_U32, &g_hits, 0, "commits where the picked bone was nudged");
  bridge_var("skin.tables", BRIDGE_U32, &g_tables, 0, "commits whose c96 looked like a bone row");
  bridge_var("skin.scan", BRIDGE_I32, &g_scan_done, 1, "set 1: at the next table, log each bone's first row once");
}

void Perturb(uint8_t* base, uint32_t shadow) {
  RegisterVars();
  const uint32_t c96 = shadow + kConstantsOffset + 16u * kPaletteFirst;
  if (!UnitRow(base, c96)) return;   // no palette in the shadow at this commit (menus, films, unskinned draws)
  ++g_tables;
  if (g_scan_done == 1) {
    g_scan_done = 2;
    for (int b = 0; b < kBoneMax; ++b) {
      const uint32_t c = c96 + 48u * b;
      REXGPU_INFO("[SKIN] SCAN bone {} c{}: row0=({:.3f} {:.3f} {:.3f} | {:.2f}) unit={} row1=({:.3f} {:.3f} {:.3f} | {:.2f}) row2=({:.3f} {:.3f} {:.3f} | {:.2f})",
                  b, kPaletteFirst + 3 * b, GuestFloat(base, c), GuestFloat(base, c + 4u), GuestFloat(base, c + 8u),
                  GuestFloat(base, c + 12u), UnitRow(base, c) ? 1 : 0, GuestFloat(base, c + 16u), GuestFloat(base, c + 20u),
                  GuestFloat(base, c + 24u), GuestFloat(base, c + 28u), GuestFloat(base, c + 32u), GuestFloat(base, c + 36u),
                  GuestFloat(base, c + 40u), GuestFloat(base, c + 44u));
    }
  }
  const int bone = g_bone;
  if (bone < 0 || bone >= kBoneMax) return;
  const uint32_t c = c96 + 48u * bone;
  if (g_mode == 1) {
    for (uint32_t row = 0; row < 3; ++row)
      for (uint32_t k = 0; k < 3; ++k) StoreGuestFloat(base, c + 16u * row + 4u * k, 0.f);
  } else {
    const float d[3] = {g_dx, g_dy, g_dz};
    for (uint32_t row = 0; row < 3; ++row) {
      const uint32_t a = c + 16u * row + 12u;
      StoreGuestFloat(base, a, GuestFloat(base, a) + d[row]);
    }
  }
  if (g_dirty == 1) {
    REX_STORE_U32(shadow, kMaxShadowMask);
    REX_STORE_U32(shadow + 4u, kMaxShadowMask);
  }
  ++g_hits;
}

}  // namespace

DECLARE_REX_FUNC(sub_82249488);

REX_HOOK_RAW(sub_82249488) {
  {
    const uint32_t shadow = REX_LOAD_U32(kRenderContext + kShadowPtrOffset);
    if (shadow != 0u) Perturb(base, shadow);
  }
  if (On()) {
    const uint32_t shadow = REX_LOAD_U32(kRenderContext + kShadowPtrOffset);
    std::lock_guard<std::mutex> lock(g_mu);
    ++g_commits;
    if (shadow == 0u) {
      ++g_no_shadow;
    } else {
      const uint64_t mask = (static_cast<uint64_t>(REX_LOAD_U32(shadow)) << 32) | REX_LOAD_U32(shadow + 4u);
      Note(mask);
      const uint64_t low = mask & ~(3ull << 62);
      if (low != 0u) {
        ++g_low_commits;
        int lo = -1, hi = -1, n = 0;
        for (int b = 0; b < 62; ++b) {
          if (low & (1ull << b)) { if (lo < 0) lo = b; hi = b; ++n; }
        }
        if (lo < g_low_min) g_low_min = lo;
        if (hi > g_low_max) g_low_max = hi;
        (void)n;
      }
      // Second form (the mask turned out to be all-or-nothing): dump constants c8..c71 of the
      // first kDumps commits that have anything non-zero in c8..c10, at least kDumpGap commits
      // apart, so the bone palette can be recognised offline by its shape (3-constant groups whose
      // rows are unit length and at right angles).
      if (g_dumps < kDumps && g_commits - g_last_dump >= kDumpGap) {
        const uint32_t c8 = shadow + kConstantsOffset + 16u * 8u;
        // c8 and c9 are always set (the $BASE+8/+9 constants of VP.xrg), so ask for a table's worth
        int nonzero = 0;
        for (uint32_t i = 0; i < 16u * kDumpParts; ++i) {
          const uint32_t a = c8 + 16u * i;
          if (REX_LOAD_U32(a) | REX_LOAD_U32(a + 4u) | REX_LOAD_U32(a + 8u) | REX_LOAD_U32(a + 12u)) ++nonzero;
        }
        if (nonzero >= kDumpMinConstants) {
          ++g_dumps;
          g_last_dump = g_commits;
          for (int part = 0; part < kDumpParts; ++part) {  // lines of 16 constants x 4 floats
            char line[1400];
            int p = std::snprintf(line, sizeof(line), "[SKIN] DUMP %llu/%d c%d..c%d:", static_cast<unsigned long long>(g_commits),
                                  g_dumps, 8 + 16 * part, 8 + 16 * part + 15);
            for (int i = 0; i < 64 && p > 0 && p < static_cast<int>(sizeof(line)) - 16; ++i)
              p += std::snprintf(line + p, sizeof(line) - p, " %.3f", GuestFloat(base, c8 + 16u * 16u * part + 4u * i));
            REXGPU_INFO("{}", line);
          }
        }
      }
    }
    if (g_commits % kReportEvery == 0) {
      REXGPU_INFO("[SKIN] commits={} with_bits_below_62={} no_shadow={} low_bit_min={} max={} shadow=0x{:08X}",
                  g_commits, g_low_commits, g_no_shadow, g_low_min, g_low_max, shadow);
      for (int i = 0; i < g_mask_n; ++i)
        REXGPU_INFO("[SKIN]   mask {:016x} x{}", g_masks[i], g_mask_counts[i]);
    }
  }
  __imp__sub_82249488(ctx, base);
}
