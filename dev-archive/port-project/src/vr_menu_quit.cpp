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

// 2026-11-19: QUIT AT THE BOTTOM (Tefa: "replace multiplayer with quit and move it to the bottom"). The loaded menu data
// is the file's text table, each text stored once and used by both main-menu pages; nothing points at the texts
// (looked for live, 2026-11-19), so a line moves only by rewriting texts in place, and a text cannot grow. The three
// lines' texts sit at fixed distances from each other (measured live in b033, guest 0xB03B77EA..0xB03B78C1):
//     Multiplayer label -0x20 | Multiplayer command 0 | Extra Content label +0x7C | Extra Content command +0x9C
// Extra Content's command (27 characters) does not fit Multiplayer's place (25), so that place runs the short
// cheat("dk_extra"), which our cheat() hook (vr_devmenu_unlock.cpp) turns into cg_submenu('extra_content').
// Result: ... CHECKPOINTS / EXTRA CONTENT / OPTIONS / QUIT (Options keeps its own place; its label does not fit elsewhere).
// All four are written together, and only where all four originals are found, so a second look (or the same memory
// seen again through the console's mirrored address ranges) finds nothing left to change.
constexpr ptrdiff_t kMpLabelAt = -0x20, kExLabelAt = 0x7C, kExCmdAt = 0x9C;
constexpr size_t kUnitBefore = 0x20, kUnitAfter = 0x9C + 28;
const char kMpLabel[] = "nc, \xa7LMENU_MULTIPLAYER";              // 22 + null + one pad byte
const char kMpCmd[] = "cg_submenu('multiplayer')";               // 25
const char kExLabel[] = "nc, \xa7LMENU_EXTRACONTENT";             // 23
const char kExCmd[] = "cg_submenu('extra_content')";             // 27
const char kTopLabel[] = "nc, \xa7LMENU_EXTRACONTENT";            // 23 + null: uses the pad byte
const char kTopCmd[] = "cheat(\"dk_extra\")        ";             // 25
const char kBottomLabel[] = "nc,         \xa7LMENU_QUIT";         // 23
const char kBottomCmd[] = "cg_submenu('quit_ask')     ";         // 27
static_assert(sizeof(kTopLabel) - 1 == sizeof(kMpLabel), "the label uses the pad byte: one longer than Multiplayer's");
static_assert(sizeof(kTopCmd) == sizeof(kMpCmd), "same length");
static_assert(sizeof(kBottomLabel) == sizeof(kExLabel), "same length");
static_assert(sizeof(kBottomCmd) == sizeof(kExCmd), "same length");

std::atomic<int> g_bottom{0};   // complete menus where QUIT went to the bottom

// 2026-11-19, seen live: each text is stored ONCE and shared. The Extra Content label is also that page's heading,
// and Multiplayer's label is also a line on the OPTIONS page. So the two labels are set by where the player is: the
// game copies a page's texts when it builds the page, and the menu commands' own handlers are hooked (DkMenuQuitPage).
//     main menu:           place 4 reads EXTRA CONTENT, the bottom line reads QUIT
//     inside EXTRA CONTENT: the bottom line's text is EXTRA CONTENT again (the page's heading)
//     inside OPTIONS:       place 4's text is MULTIPLAYER again (the options line the game shipped with)
std::atomic<uint8_t*> g_unit{nullptr};   // the Multiplayer command of the menu that was moved (host address)
enum Where { kMain, kExtra, kOptions };
int g_where = kMain, g_depth = 0;        // the game's own thread only

void ApplyLabels(uint8_t* q, int where) {
  uint8_t* top = q + kMpLabelAt;
  if (where == kOptions) {
    std::memcpy(top, kMpLabel, sizeof(kMpLabel));   // 22 + null
    top[sizeof(kMpLabel)] = 0;                      // the pad byte
  } else {
    std::memcpy(top, kTopLabel, sizeof(kTopLabel));
  }
  std::memcpy(q + kExLabelAt, where == kExtra ? kExLabel : kBottomLabel, sizeof(kExLabel) - 1);
}

// true when the four original texts sit around q (q = the Multiplayer command), and then rewrites all four
bool MoveQuitToBottom(uint8_t* q) {
  uint8_t* mp_label = q + kMpLabelAt;
  uint8_t* ex_label = q + kExLabelAt;
  uint8_t* ex_cmd = q + kExCmdAt;
  if (std::memcmp(mp_label, kMpLabel, sizeof(kMpLabel)) != 0 || mp_label[sizeof(kMpLabel)] != 0) return false;
  if (std::memcmp(ex_label, kExLabel, sizeof(kExLabel)) != 0) return false;
  if (std::memcmp(ex_cmd, kExCmd, sizeof(kExCmd)) != 0) return false;
  std::memcpy(mp_label, kTopLabel, sizeof(kTopLabel));         // 24 bytes, its own null in the old pad byte
  std::memcpy(q, kTopCmd, sizeof(kTopCmd) - 1);
  std::memcpy(ex_label, kBottomLabel, sizeof(kBottomLabel) - 1);
  std::memcpy(ex_cmd, kBottomCmd, sizeof(kBottomCmd) - 1);
  uint8_t* none = nullptr;
  g_unit.compare_exchange_strong(none, q);
  return true;
}

int Rewrite(uint8_t* p, size_t n, const Swap& s) {
  const size_t len = std::strlen(s.from);
  int hits = 0;
  if (n < len + 1) return 0;
  const bool is_cmd = !s.need_null;
  const uint8_t first = static_cast<uint8_t>(s.from[0]);
  for (uint8_t* q = p; q + len + 1 <= p + n;) {
    q = static_cast<uint8_t*>(std::memchr(q, first, static_cast<size_t>(p + n - q) - len));
    if (q == nullptr) break;
    if (std::memcmp(q, s.from, len) == 0 && (!s.need_null || q[len] == 0)) {
      // the whole menu (all four texts in place): QUIT goes to the bottom; a stray part copy: the old rewrite
      if (is_cmd && static_cast<size_t>(q - p) >= kUnitBefore && static_cast<size_t>(p + n - q) >= kUnitAfter &&
          MoveQuitToBottom(q)) {
        ++g_bottom;
      } else {
        std::memcpy(q, s.to, len);
      }
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
            for (int i = 1; i >= 0; --i) now[i] += Rewrite(base + lo, static_cast<size_t>(hi - lo), kSwaps[i]);
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
  REXGPU_INFO("[QUIT] done looking: {} label(s) and {} command(s) rewritten; QUIT moved to the bottom in {} whole menu(s)",
              total[0], total[1], g_bottom.load());
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

// Called from the hooks on the game's own menu commands (vr_devmenu_unlock.cpp), on the game's thread, BEFORE the
// command runs. what: 0 = cg_submenu(page), 1 = cg_prevmenu, 2 = cg_rootmenu / cg_clearmenus. (Menu clicks do not pass
// the console hook: seen live 2026-11-19, so the commands' own handlers are hooked.)
void DkMenuQuitPage(int what, const char* page) {
  uint8_t* q = g_unit.load();
  if (q == nullptr) return;
  int where = g_where, depth = g_depth;
  if (what == 0) {
    if (page != nullptr && std::strcmp(page, "extra_content") == 0) {
      where = kExtra;
      depth = 1;
    } else if (where == kMain && page != nullptr && std::strcmp(page, "options_main") == 0) {
      where = kOptions;
      depth = 1;
    } else if (where != kMain) {
      ++depth;
    }
  } else if (what == 1) {
    if (where != kMain && --depth <= 0) {
      where = kMain;
      depth = 0;
    }
  } else {
    where = kMain;
    depth = 0;
  }
  if (where != g_where) {
    ApplyLabels(q, where);
    REXGPU_INFO("[QUIT] menu labels set for {}", where == kExtra ? "the EXTRA CONTENT page" : where == kOptions ? "the OPTIONS pages" : "the main menu");
  }
  g_where = where;
  g_depth = depth;
}
