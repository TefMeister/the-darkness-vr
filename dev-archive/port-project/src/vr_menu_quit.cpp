// The Darkness VR: a QUIT line on the main menu, in Multiplayer's place (2026-11-12).
//
// Tefa: "we do need to build a quit button into the main menu ... like an extention of the menu, same font if doable";
// "replace multiplayer with quit and move it to the bottom if you need a slot and can't add one, no one will play this
// multiplayer".
//
// The game's menu data holds a page `quit_ask` ("Are you sure you want to quit?" NO / YES, YES runs quit(), which ends
// the port cleanly [verified-live 2026-11-12]) and the word LMENU_QUIT; the console's main menu never links to them.
// Patching the loose menu files (Content/Gui/CubeWnd.xcr, MP_CubeWnd.xcr) changed nothing on screen: the menu the game
// draws comes from a packed copy [verified-live 2026-11-12]. So the two texts of the MULTIPLAYER line are overwritten
// where the game keeps them once loaded: in its memory.
//     label   "nc, <mark>LMENU_MULTIPLAYER"  ->  "nc,        <mark>LMENU_QUIT"   (same length: spaces after the comma)
//     opens   "cg_submenu('multiplayer')"    ->  "cg_submenu('quit_ask')   "    (same length: spaces after)
// A thread of ours looks through the game's committed memory every kEvery seconds for the first kFor seconds after the
// first frame and rewrites every copy it finds (the label only where a null ends it, so LMENU_MULTIPLAYER_OPTIONS and
// _HEADING are left alone). DK_MENU_QUIT=1 switches it on. [compile-verified] at most until it has run.

#include "generated/default/darknessrecomp_pch.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <windows.h>

namespace {

constexpr int kEvery = 3, kFor = 240;          // seconds
constexpr int kAfterHit = 5;                   // looks after the first hit
constexpr uint64_t kLo = 0x00010000ull, kHi = 0xFFFF0000ull;   // the guest's whole address range

struct Swap { const char* from; const char* to; bool need_null; };
// "\xa7" is the mark the menu data puts before a text key
const Swap kSwaps[] = {
    {"nc, \xa7LMENU_MULTIPLAYER", "nc,        \xa7LMENU_QUIT", true},
    {"cg_submenu('multiplayer')", "cg_submenu('quit_ask')   ", false},
};

std::atomic<bool> g_started{false};

int Rewrite(uint8_t* p, size_t n, const Swap& s) {
  const size_t len = std::strlen(s.from);
  int hits = 0;
  if (n < len + 1) return 0;
  const uint8_t first = static_cast<uint8_t>(s.from[0]);
  for (uint8_t* q = p; q + len + 1 <= p + n;) {
    q = static_cast<uint8_t*>(std::memchr(q, first, static_cast<size_t>(p + n - q) - len));
    if (q == nullptr) break;
    if (std::memcmp(q, s.from, len) == 0 && (!s.need_null || q[len] == 0)) {
      std::memcpy(q, s.to, len);
      ++hits;
      q += len;
    } else {
      ++q;
    }
  }
  return hits;
}

void Worker(uint8_t* base) {
  int total[2] = {0, 0};
  int after_hit = 0;
  for (int t = 0; t < kFor; t += kEvery) {
    std::this_thread::sleep_for(std::chrono::seconds(kEvery));
    int now[2] = {0, 0};
    uint64_t a = kLo;
    while (a < kHi) {
      MEMORY_BASIC_INFORMATION mbi;
      if (VirtualQuery(base + a, &mbi, sizeof(mbi)) == 0) break;
      const uint64_t start = static_cast<uint64_t>(static_cast<uint8_t*>(mbi.BaseAddress) - base);
      const uint64_t end = start + mbi.RegionSize;
      const bool readable = mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) != 0 &&
                            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
      if (readable) {
        const uint64_t lo = start < a ? a : start, hi = end > kHi ? kHi : end;
        if (hi > lo) {
          __try {
            for (int i = 0; i < 2; ++i) now[i] += Rewrite(base + lo, static_cast<size_t>(hi - lo), kSwaps[i]);
          } __except (EXCEPTION_EXECUTE_HANDLER) {
          }
        }
      }
      a = end > a ? end : a + 0x10000ull;
    }
    // once the line has been found, a few more looks (a second copy made later), then stop: a look goes through all the
    // game's memory and must not go on into play
    if (total[1] != 0 && ++after_hit > kAfterHit) break;
    if (now[0] != 0 || now[1] != 0) {
      total[0] += now[0];
      total[1] += now[1];
      REXGPU_INFO("[QUIT] the Multiplayer line rewritten as QUIT: {} label(s), {} command(s) at {} s (so far {} / {})", now[0],
                  now[1], t + kEvery, total[0], total[1]);
    }
  }
  REXGPU_INFO("[QUIT] done looking: {} label(s) and {} command(s) rewritten", total[0], total[1]);
}

}  // namespace

// Called from a hook that runs every frame (vr_viewport_probe.cpp); starts the look-through once.
void DkMenuQuitTick(uint8_t* base) {
  if (g_started.exchange(true)) return;
  const char* v = std::getenv("DK_MENU_QUIT");
  const bool on = v != nullptr && v[0] == '1';
  REXGPU_INFO("[QUIT] DK_MENU_QUIT={}: a QUIT line in Multiplayer's place {}", v ? v : "(unset)", on ? "ON" : "off");
  if (on) std::thread(Worker, base).detach();
}
