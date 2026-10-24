// Developer-menu unlock: give the empty cheat() script command a body.  OFF unless DK_DEVMENU=1.
//
// What the retail game does (static reading, 2026-10-24; tags in the README beside this file):
//   * The front end registers its script commands in sub_8236D738. "cheat" is bound, with the
//     void(CStr) binder sub_8236EB50, to the vcall thunk sub_8236F300 = "jump to vtable slot +260".
//   * Slot +260 of both front-end vtables (0x82071A10, 0x82075E20) is sub_8276AA20 =
//     "mr r3,r4; b sub_821F8AD0": it destroys its string argument and returns. Nothing else.
//   * The sibling command "cachecommand" (slot +264, sub_8236D420) shows the calling convention:
//     r3 = front end, r4 = guest address of a CStr passed BY VALUE, which the callee must destroy.
//   * Every retail "run this command string now" goes through sub_8277B388:
//     r3 = CStr* (by value, consumed/destroyed by the callee), r4 = const char* tag (a name used
//     for logging), r5 = 0. It looks up "SYSTEM.CONSOLE" and runs the text. Returns r3 = 1 if a
//     console was found. "docachedcommand" (+268) and "cg_backout" (+272) both call it, from
//     inside a running script command, so calling it from inside cheat() is a retail pattern.
//
// So this hook makes cheat(s) do what its name says: run s. With DK_DEVMENU=1:
//   main menu: Space / pad button that the menu calls GUI_BUTTON2 or JOY_BUTTON_02
//              -> cheat("cg_submenu('DevMenu')")         -> the developer menu opens
//   pause menu (GAMEMENU_real): the same buttons
//              -> cheat("cg_rootmenu('GAMEMENU2')")      -> noclip, god-mode, FreezeCam, debug menus
// Optional shortcuts for unattended runs (replace what the doorway would open):
//   DK_DEVMENU_MAP=NY1_Chinatown   the main-menu doorway jumps straight to that map, the same
//                                  command line the DevMenu "Milestone" entries use
//   DK_DEVMENU_MAIN_CMD=<text>     the main-menu doorway runs <text> instead (wins over _MAP)
//   DK_DEVMENU_GAME_CMD=<text>     the pause-menu doorway runs <text> instead,
//                                  e.g.  cmd_forced(godmode); cg_clearmenus()
// Every cheat() call is logged ([DEVMENU] lines) so a run shows whether the button reached it.

#include "generated/default/darknessrecomp_pch.h"

#include <rex/hook.h>

#include <cstdlib>
#include <cstring>
#include <string>

DECLARE_REX_FUNC(sub_8236F300);  // cheat(): vcall thunk to front-end vtable +260 (empty in retail)
DECLARE_REX_FUNC(sub_8277B388);  // ExecuteString(CStr byval, const char* tag, int) -> bool
DECLARE_REX_FUNC(sub_821F86A8);  // CStr::CStr(const char*)
DECLARE_REX_FUNC(sub_821F8AD0);  // CStr::~CStr()

namespace {

// ---- settings: every number this file uses -------------------------------------------------
constexpr uint32_t kTagConExecute = 0x82051210u;  // guest string "ConExecute" (retail's own tag)
constexpr uint32_t kCStrDataPtr = 4u;            // CStr: +0 vtable, +4 pointer to data block
constexpr uint32_t kCStrDataChars = 2u;          // data block: u16 header, then the characters
constexpr uint16_t kCStrHeaderWide = 0x8000u;    // header bit: 16-bit characters
constexpr uint16_t kCStrHeaderNoText = 0x2000u;  // header bit: no text (length reads as 0)
constexpr uint32_t kFrameBytes = 1024u;          // guest stack we borrow for a replacement string
constexpr uint32_t kLinkageBytes = 80u;          // leave the PPC linkage area alone (retail uses +80)
constexpr uint32_t kTextOffset = kLinkageBytes + 16u;  // our text sits after the 8-byte CStr
constexpr size_t kMaxCmdChars = kFrameBytes - kTextOffset - 1u;
constexpr size_t kMaxLoggedChars = 200u;
constexpr int kMaxLogLines = 60;
constexpr uint32_t kStackAlignMask = ~0xFu;
// ---------------------------------------------------------------------------------------------

struct Settings {
  bool on = false;
  std::string main_cmd;  // replaces the main-menu doorway (empty = open DevMenu as the data says)
  std::string game_cmd;  // replaces the pause-menu doorway (empty = open GAMEMENU2)
};

bool SafeForScriptQuotes(const char* s) {
  for (; *s != '\0'; ++s) {
    if (*s == '"' || *s == '\\' || *s == '\'') return false;
  }
  return true;
}

const Settings& Get() {
  static const Settings settings = [] {
    Settings s;
    const char* on = std::getenv("DK_DEVMENU");
    s.on = on != nullptr && on[0] == '1';
    if (const char* m = std::getenv("DK_DEVMENU_MAIN_CMD"); m != nullptr && m[0] != '\0') {
      s.main_cmd = m;
    } else if (const char* map = std::getenv("DK_DEVMENU_MAP"); map != nullptr && map[0] != '\0') {
      if (SafeForScriptQuotes(map)) {
        // Word for word the DevMenu "Milestone" entries, with the map name swapped in.
        s.main_cmd = std::string("cachecommand(\"cg_blackloading(); campaignmap('") + map +
                     "')\"); cg_rootmenu(\"begin_loadtransform\")";
      } else {
        REXLOG_INFO("[DEVMENU] DK_DEVMENU_MAP ignored: quotes or backslashes in '{}'", map);
      }
    }
    if (const char* g = std::getenv("DK_DEVMENU_GAME_CMD"); g != nullptr && g[0] != '\0') {
      s.game_cmd = g;
    }
    for (std::string* c : {&s.main_cmd, &s.game_cmd}) {
      if (c->size() > kMaxCmdChars) {
        REXLOG_INFO("[DEVMENU] command longer than {} characters ignored", kMaxCmdChars);
        c->clear();
      }
    }
    REXLOG_INFO("[DEVMENU] cheat() unlock = {} | main override = '{}' | game override = '{}'",
                s.on ? 1 : 0, s.main_cmd, s.game_cmd);
    return s;
  }();
  return settings;
}

// Read a guest CStr's text for the log and the doorway test. 8-bit text only; a wide or empty
// string reads as "".
std::string ReadCStr(uint8_t* base, uint32_t cstr) {
  std::string out;
  if (cstr == 0u) return out;
  const uint32_t data = REX_LOAD_U32(cstr + kCStrDataPtr);
  if (data == 0u) return out;
  const uint16_t header = REX_LOAD_U16(data);
  if ((header & (kCStrHeaderWide | kCStrHeaderNoText)) != 0u) return out;
  for (uint32_t i = 0; i < kMaxCmdChars; ++i) {
    const uint8_t c = REX_LOAD_U8(data + kCStrDataChars + i);
    if (c == 0u) break;
    out.push_back(static_cast<char>(c));
  }
  return out;
}

// Run `text` on the game's console, from a CStr we build on borrowed guest stack.
// The game copies the characters into its own heap (CStr ctor -> vtable +104 allocates and
// memcpys), and ExecuteString destroys the CStr, so nothing outlives the borrowed frame.
uint32_t RunOwnText(PPCContext& ctx, uint8_t* base, const std::string& text) {
  const uint64_t saved_r1 = ctx.r1.u64;
  const uint64_t saved_lr = ctx.lr;
  const uint32_t old_sp = ctx.r1.u32;
  const uint32_t frame = (old_sp - kFrameBytes) & kStackAlignMask;
  REX_STORE_U32(frame, old_sp);  // back chain, as stwu would leave it
  ctx.r1.u64 = frame;

  const uint32_t cstr = frame + kLinkageBytes;
  const uint32_t chars = frame + kTextOffset;
  for (size_t i = 0; i < text.size(); ++i) {
    REX_STORE_U8(chars + static_cast<uint32_t>(i), static_cast<uint8_t>(text[i]));
  }
  REX_STORE_U8(chars + static_cast<uint32_t>(text.size()), 0u);

  ctx.r3.u64 = cstr;
  ctx.r4.u64 = chars;
  sub_821F86A8(ctx, base);  // CStr(cstr, chars)

  ctx.r3.u64 = cstr;
  ctx.r4.u64 = kTagConExecute;
  ctx.r5.u64 = 0;
  sub_8277B388(ctx, base);  // consumes cstr
  const uint32_t ran = ctx.r3.u32;

  ctx.r1.u64 = saved_r1;
  ctx.lr = saved_lr;
  return ran;
}

}  // namespace

REX_HOOK_RAW(sub_8236F300) {
  const Settings& s = Get();
  if (!s.on) {
    __imp__sub_8236F300(ctx, base);  // retail: vtable +260, which destroys the string
    return;
  }

  const uint32_t front_end = ctx.r3.u32;
  const uint32_t arg = ctx.r4.u32;  // CStr by value: ours to consume
  const std::string text = ReadCStr(base, arg);

  const std::string* replace = nullptr;
  if (!s.main_cmd.empty() && text.find("DevMenu") != std::string::npos) replace = &s.main_cmd;
  if (!s.game_cmd.empty() && text.find("GAMEMENU2") != std::string::npos) replace = &s.game_cmd;

  uint32_t ran = 0;
  const uint64_t saved_lr = ctx.lr;
  if (replace == nullptr) {
    // Hand the caller's own string straight to the console; it destroys it, as cheat() must.
    ctx.r3.u64 = arg;
    ctx.r4.u64 = kTagConExecute;
    ctx.r5.u64 = 0;
    sub_8277B388(ctx, base);
    ran = ctx.r3.u32;
  } else {
    ctx.r3.u64 = arg;
    sub_821F8AD0(ctx, base);  // we own the argument and are not using it
    ran = RunOwnText(ctx, base, *replace);
  }
  ctx.lr = saved_lr;

  static int lines = 0;
  if (lines < kMaxLogLines) {
    ++lines;
    REXLOG_INFO("[DEVMENU] cheat #{} front_end=0x{:08X} text='{}' -> ran='{}' console_found={}", lines,
                front_end, text.substr(0, kMaxLoggedChars),
                (replace != nullptr ? *replace : text).substr(0, kMaxLoggedChars), ran);
  }
}
