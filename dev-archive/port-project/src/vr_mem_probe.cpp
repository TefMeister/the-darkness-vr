// Guest memory through the bridge (2026-11-10): read, snapshot, compare. A dev tool for finding which bytes of a game
// object change when something happens in the game (the held gun, first).
//
//   gread <hex addr> [words]        big-endian 32-bit words from guest memory (default 16, at most 256)
//   gsnap <hex addr> <hex len>      keep a copy of that stretch (the copy before it becomes "the older one")
//   gdiff [max]                     the words that differ between the two newest copies of the SAME stretch
//   gfind <hex value> <hex addr> <hex len>   where that 32-bit value sits in a stretch (word-aligned)
//
// Reads go through ReadProcessMemory on our own process, so an unmapped guest page is an error line, not a crash.
// Nothing here writes.

#include "generated/default/darknessrecomp_pch.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#include <windows.h>

extern "C" {
#include "bridge/bridge.h"
}

namespace {
uint8_t* g_base = nullptr;
std::mutex g_mu;
struct Snap {
  uint32_t addr = 0;
  std::vector<uint8_t> bytes;
};
Snap g_new, g_old;

bool Read(uint32_t addr, void* out, size_t n) {
  if (g_base == nullptr) return false;
  SIZE_T got = 0;
  return ReadProcessMemory(GetCurrentProcess(), g_base + addr, out, n, &got) && got == n;
}
uint32_t Be(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint32_t Hex(const char* s) { return static_cast<uint32_t>(std::strtoul(s, nullptr, 16)); }

void CmdRead(int argc, const char** argv, bridge_reply* r, void*) {
  if (argc < 2) { bridge_reply_err(r, "gread <hex addr> [words]"); return; }
  const uint32_t addr = Hex(argv[1]);
  int words = argc >= 3 ? std::atoi(argv[2]) : 16;
  if (words < 1) words = 1;
  if (words > 256) words = 256;
  std::vector<uint8_t> b(size_t(words) * 4);
  if (!Read(addr, b.data(), b.size())) { bridge_reply_err(r, "cannot read %08X (%d words)", addr, words); return; }
  for (int i = 0; i < words; i += 8) {
    char line[160];
    int n = std::snprintf(line, sizeof line, "%08X:", addr + uint32_t(i) * 4u);
    for (int k = i; k < words && k < i + 8; ++k) n += std::snprintf(line + n, sizeof line - n, " %08X", Be(&b[size_t(k) * 4]));
    bridge_reply_line(r, "%s", line);
  }
}

void CmdSnap(int argc, const char** argv, bridge_reply* r, void*) {
  if (argc < 3) { bridge_reply_err(r, "gsnap <hex addr> <hex len>"); return; }
  const uint32_t addr = Hex(argv[1]);
  uint32_t len = Hex(argv[2]);
  if (len == 0 || len > 0x400000u) { bridge_reply_err(r, "length 1..400000 hex"); return; }
  len &= ~3u;
  std::vector<uint8_t> b(len);
  // page by page, so one unmapped page does not lose the stretch (it reads as zeros)
  uint32_t bad = 0;
  for (uint32_t off = 0; off < len; off += 0x1000u) {
    const uint32_t n = (len - off) < 0x1000u ? (len - off) : 0x1000u;
    if (!Read(addr + off, b.data() + off, n)) { std::memset(b.data() + off, 0, n); ++bad; }
  }
  std::lock_guard<std::mutex> lock(g_mu);
  g_old = std::move(g_new);
  g_new.addr = addr;
  g_new.bytes = std::move(b);
  bridge_reply_line(r, "kept %08X + %X (%u unreadable pages); the older copy: %08X + %zX", addr, len, bad, g_old.addr, g_old.bytes.size());
}

void CmdDiff(int argc, const char** argv, bridge_reply* r, void*) {
  int max = argc >= 2 ? std::atoi(argv[1]) : 60;
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_new.bytes.empty() || g_old.bytes.size() != g_new.bytes.size() || g_old.addr != g_new.addr) {
    bridge_reply_err(r, "two copies of the same stretch are needed (gsnap twice)");
    return;
  }
  int shown = 0, total = 0;
  for (size_t i = 0; i + 4 <= g_new.bytes.size(); i += 4) {
    const uint32_t a = Be(&g_old.bytes[i]), b = Be(&g_new.bytes[i]);
    if (a == b) continue;
    ++total;
    if (shown < max) { bridge_reply_line(r, "+%zX (%08X): %08X -> %08X", i, g_new.addr + uint32_t(i), a, b); ++shown; }
  }
  bridge_reply_line(r, "%d words differ (%d shown)", total, shown);
}

void CmdFind(int argc, const char** argv, bridge_reply* r, void*) {
  if (argc < 4) { bridge_reply_err(r, "gfind <hex value> <hex addr> <hex len>"); return; }
  const uint32_t want = Hex(argv[1]), addr = Hex(argv[2]);
  uint32_t len = Hex(argv[3]) & ~3u;
  if (len == 0 || len > 0x4000000u) { bridge_reply_err(r, "length 1..4000000 hex"); return; }
  int found = 0;
  std::vector<uint8_t> page(0x1000);
  for (uint32_t off = 0; off < len && found < 40; off += 0x1000u) {
    const uint32_t n = (len - off) < 0x1000u ? (len - off) : 0x1000u;
    if (!Read(addr + off, page.data(), n)) continue;
    for (uint32_t i = 0; i + 4 <= n && found < 40; i += 4) {
      if (Be(&page[i]) == want) { bridge_reply_line(r, "%08X", addr + off + i); ++found; }
    }
  }
  bridge_reply_line(r, "%d found", found);
}
}  // namespace

// called from a guest hook (it has the guest memory's base); registers the commands once
void DkMemProbeInit(uint8_t* base) {
  static bool done = false;
  if (done) return;
  done = true;
  g_base = base;
  bridge_cmd("gread", CmdRead, nullptr, 0, "gread <hex addr> [words]: guest memory as 32-bit words");
  bridge_cmd("gsnap", CmdSnap, nullptr, 0, "gsnap <hex addr> <hex len>: keep a copy of a stretch of guest memory");
  bridge_cmd("gdiff", CmdDiff, nullptr, 0, "gdiff [max]: words that differ between the two newest copies");
  bridge_cmd("gfind", CmdFind, nullptr, 0, "gfind <hex value> <hex addr> <hex len>: where a 32-bit value sits");
}
