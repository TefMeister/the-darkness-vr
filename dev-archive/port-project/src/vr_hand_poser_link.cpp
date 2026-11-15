// The Darkness VR: the link to the Hand Poser window (DRAFT 2026-11-15, plan step 2; unrun).
//
// Hand Poser (hand-poser-vr) is a window that holds finger joints at angles Tefa sets with sliders. In the RE games a
// Lua script in the game does the game's half; here this file does it, in C++, through the same two files in
// <folder of darknessrecomp.exe>\reframework\data\hand_poser\ (the window finds a game by a running exe with a
// reframework folder beside it, so this file creates that folder):
//   state.json    written here about once a second (every 0.12 s while joints are held or read): game, character,
//                 connected, gun, seq_seen, error, the whole skeleton as [name, parent index], and for every held or
//                 asked-for joint its local angles ("angles") and its turn as seen from its hand ("rel"), degrees.
//   command.json  written by the window: {seq, hold: {name: [x, y, z] degrees}, want_read: [names], go_name}.
// File work runs on a thread of its own; the game thread only copies numbers in and out under a lock.
//
// THE SKELETON (engine-research/inbox/2026-11-15-reader-hand-skeleton.md): the player's body j_e_noguns.xmd, bones
// 0-67 the engine's shared human layout. Left hand 16 (thumb 38-40, index 41-43, middle 44-46, ring 47-49, little
// 50-52), right hand 21 (53-55, 56-58, 59-61, 62-64, 65-67); joint 1 = next to the hand. Names in Village's style so
// the window's finger groups work unchanged (L_IndexF1 ..; thumbs L_Thumb1 .. as Village names them); every other
// bone is bone_N. Parents are read from the live skeleton when it is there, else the table below.
//
// THE LOCAL TURNS [inferred-static 2026-11-15]: *(SI+32) + 64*b is bone b's local 4x4 (row-major, rows = axes, row 3
// = translation; v' = v * M), *(SI+36) + 64*b the composed bone-to-world 4x4. The composer sub_82420678(r3 = skeleton,
// r4 = start bone, r5 = &parent's world 4x4, r6 = SI) rebuilds the start bone and every bone below it as
//   world[b] = (rotate by local[b] about b's bind pivot) * world[parent],
// so a local rotation is a turn about the joint expressed in the MODEL's bind axes (X forward, Y across the body,
// Z up), and IDENTITY = THE BIND POSE (fingers straight, T-pose). So 0, 0, 0 in the window = straight fingers.
//
// ⚠️ GUESSES, to settle on the first live run:
//   - EULER ORDER: the same as Hand Poser's own "from the hand" numbers (hand_poser.lua euler_deg): the turn is
//     R = Rz(z) * Ry(y) * Rx(x) on column vectors (applied X first, then Y, then Z, about the fixed bind axes); the
//     guest's row-vector matrix is its transpose. [hypothesis: chosen to match the window, not read from the game]
//   - WHICH AXIS CURLS A FINGER: in the bind pose the fingers run along +Y (right hand) / -Y (left), so a curl is a
//     turn about X (forward), opposite signs per hand. [hypothesis: the palm's facing in the bind pose is not known]
//   - Row 3 of the local (translation) is left as the game wrote it. The composer replaces it with a pivot term; the
//     pivot's .w decides whether the local translation counts at all [hypothesis: not read].
//   - Nothing after sub_821BF988 rewrites the finger locals or re-composes from older locals in the same frame
//     [hypothesis]; the later animation-driven hand IK (sub_821B8300 -> solver) re-composes from the hand down using
//     the locals, which keeps ours.
// Only the hands' subtrees are posed (the hand bones 16 / 21 and their fingers); other names are ignored (logged once).
// [unbuilt into the port; syntax-checked only, see the inbox note]

#include "generated/default/darknessrecomp_pch.h"

extern "C" {
#include "bridge/bridge.h"
}

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include "vr_hand_poser_link.h"

DECLARE_REX_FUNC(sub_82420678);   // the composer: (skeleton, start bone, &parent world, skeleton instance)

namespace {

// ---- the guest layout (inbox 2026-11-15-reader-hand-skeleton.md) -------------------------------------------------
constexpr uint32_t kSiLocal = 32u, kSiWorld = 36u, kBoneBytes = 64u;
constexpr uint32_t kSkelNodes = 12u;          // *(skel+12): +0 node count, +16 the 48-byte node records
constexpr uint32_t kNodesHeader = 16u, kNodeBytes = 48u, kNodeParent = 28u;
constexpr int kBones = 293;                   // j_e_noguns.xmd [measured]; the live count wins when read
constexpr int kHand[2] = {16, 21};            // 0 left, 1 right
constexpr int kHandParent[2] = {14, 19};      // the forearms
constexpr int kFingerRoot[2][5] = {{38, 41, 44, 47, 50}, {53, 56, 59, 62, 65}};
constexpr const char* kFingerName[5] = {"Thumb", "IndexF", "MiddleF", "RingF", "PinkyF"};
constexpr uint32_t kSharedBones = 68u;   // bones 0-67: the engine's shared human skeleton, checked against the table
constexpr uint32_t kMaxBones = 1024u;    // a skeleton bigger than this is not read (a bad pointer)
constexpr int kUnknownLogMax = 20;       // log lines for joint names that are not hand joints

// ---- timing, the same as hand_poser.lua -------------------------------------------------------------------------
constexpr int kPollMs = 50;                   // command.json looked at (the Lua: every 3 frames)
constexpr int kStateEveryMs = 1000;           // state.json when nothing is held or read
constexpr int kStateLiveMs = 120;             // while joints are held or read
constexpr int kConnectedMs = 2000;            // no AfterIk call for this long = not connected (menus, loading)
constexpr float kDeg = 57.29577951f;
constexpr const char* kGame = "the-darkness";
constexpr const char* kCharacter = "Jackie";

// ---- shared state (g_mu) ----------------------------------------------------------------------------------------
std::mutex g_mu;
int32_t g_on = 0;                                 // poser.on
uint32_t g_applied = 0;                           // poser.applied: joints written so far
uint32_t g_composes = 0;                          // poser.composes: composer calls made
char g_text[200] = "poser off";                   // poser.text
std::map<int, std::array<float, 3>> g_held;       // bone -> radians (x, y, z), from command.json
std::vector<int> g_want;                          // bones whose angles the window asked for
std::map<int, std::array<float, 3>> g_angles;     // bone -> local degrees, last seen in the game
std::map<int, std::array<float, 3>> g_rel;        // bone -> degrees as seen from its hand
std::vector<int> g_parent;                        // per bone; filled from the live skeleton once
bool g_parent_live = false;
double g_seq_seen = -1;
std::string g_error;                              // "" = none
std::string g_gun;                                // "" = empty hands
std::chrono::steady_clock::time_point g_last_call;
bool g_ever_called = false;
std::atomic<bool> g_state_now{false};

// The bone table's names. Hand and finger bones by name, the rest bone_N.
std::string BoneName(int b) {
  for (int h = 0; h < 2; ++h) {
    // index 0 = bones 16 / 38-52, index 1 = bones 21 / 53-67. Since 2026-10-29 the RIGHT controller drives the chain that
    // ends at bone 16 (vr_hand_ik.cpp ik.right_chain = 1; 0 crossed the hands in the headset), and on 2026-11-15 holding
    // bone 16 turned the hand on the right controller [verified-live 2026-11-15, simulator, n=1]: so 16 is "R_", 21 "L_".
    const char* side = h ? "L_" : "R_";
    if (b == kHand[h]) return std::string(side) + "Hand";
    for (int f = 0; f < 5; ++f)
      for (int j = 0; j < 3; ++j)
        if (b == kFingerRoot[h][f] + j) return std::string(side) + kFingerName[f] + std::to_string(j + 1);
  }
  return "bone_" + std::to_string(b);
}

int BoneOf(const std::string& name) {
  static std::map<std::string, int> index;
  if (index.empty())
    for (int b = 0; b < kBones; ++b) index[BoneName(b)] = b;
  auto it = index.find(name);
  return it == index.end() ? -1 : it->second;
}

// Which hand a bone belongs to (0 / 1), -1 for none; *root = the bone to re-compose from (the finger's first joint,
// or the hand itself).
int HandOf(int b, int* root) {
  for (int h = 0; h < 2; ++h) {
    if (b == kHand[h]) { *root = b; return h; }
    for (int f = 0; f < 5; ++f)
      if (b >= kFingerRoot[h][f] && b < kFingerRoot[h][f] + 3) { *root = kFingerRoot[h][f]; return h; }
  }
  return -1;
}

// The static table's parents (inbox 2026-11-15-reader-hand-skeleton.md); used until the live skeleton is read.
int TableParent(int b) {
  int root;
  const int h = HandOf(b, &root);
  if (h >= 0) {
    if (b == kHand[h]) return kHandParent[h];
    return b == root ? kHand[h] : b - 1;
  }
  return -1;   // unknown here: the live skeleton fills it
}

inline float GuestFloat(uint8_t* base, uint32_t a) {
  const uint32_t bits = REX_LOAD_U32(a);
  float v;
  std::memcpy(&v, &bits, sizeof(v));
  return v;
}
inline void StoreGuestFloat(uint8_t* base, uint32_t a, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, sizeof(bits));
  REX_STORE_U32(a, bits);
}

// ---- the angles <-> 3x3 (row-vector guest form) ----------------------------------------------------------------
// R = Rz(z) Ry(y) Rx(x) on column vectors; the guest matrix M = R^T (rows = where the bind axes go). GUESS (see top).
void EulerToRows(const float e[3], float m[3][3]) {
  const float cx = std::cos(e[0]), sx = std::sin(e[0]);
  const float cy = std::cos(e[1]), sy = std::sin(e[1]);
  const float cz = std::cos(e[2]), sz = std::sin(e[2]);
  const float r[3][3] = {{cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz},
                         {cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz},
                         {-sy, sx * cy, cx * cy}};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) m[i][j] = r[j][i];
}

void RowsToEulerDeg(const float m[3][3], float out[3]) {   // the inverse of EulerToRows, in degrees
  // r[i][j] = m[j][i]
  float s = -m[0][2];                                       // r20 = -sin(y)
  s = s > 1.f ? 1.f : (s < -1.f ? -1.f : s);
  out[0] = std::atan2(m[1][2], m[2][2]) * kDeg;             // x = atan2(r21, r22)
  out[1] = std::asin(s) * kDeg;
  out[2] = std::atan2(m[0][1], m[0][0]) * kDeg;             // z = atan2(r10, r00)
}

void ReadRows(uint8_t* base, uint32_t a, float m[3][3]) {
  for (int k = 0; k < 3; ++k)
    for (int i = 0; i < 3; ++i) m[k][i] = GuestFloat(base, a + 16u * k + 4u * i);
}

// ---- calling the game's composer --------------------------------------------------------------------------------
// The volatile registers the caller of the hooked function may still look at (r3 = its return value) are put back;
// the non-volatile ones the composer restores itself. A small frame is dropped below r1 first, as RunOwnText does in
// vr_devmenu_unlock.cpp.
constexpr uint32_t kFrameBytes = 256u, kStackAlignMask = ~0xFu;

void Compose(PPCContext& ctx, uint8_t* base, uint32_t skel, int start, uint32_t parent_world, uint32_t si) {
  const PPCRegister r0 = ctx.r0, r1 = ctx.r1, r3 = ctx.r3, r4 = ctx.r4, r5 = ctx.r5, r6 = ctx.r6, r7 = ctx.r7,
                    r8 = ctx.r8, r9 = ctx.r9, r10 = ctx.r10, r11 = ctx.r11, r12 = ctx.r12, ctr = ctx.ctr;
  const uint64_t lr = ctx.lr;
  const PPCCRRegister cr0 = ctx.cr0, cr1 = ctx.cr1, cr5 = ctx.cr5, cr6 = ctx.cr6, cr7 = ctx.cr7;

  const uint32_t old_sp = ctx.r1.u32;
  const uint32_t frame = (old_sp - kFrameBytes) & kStackAlignMask;
  REX_STORE_U32(frame, old_sp);   // back chain, as stwu would leave it
  ctx.r1.u64 = frame;
  ctx.r3.u64 = skel;
  ctx.r4.u64 = static_cast<uint32_t>(start);
  ctx.r5.u64 = parent_world;
  ctx.r6.u64 = si;
  sub_82420678(ctx, base);

  ctx.r0 = r0; ctx.r1 = r1; ctx.r3 = r3; ctx.r4 = r4; ctx.r5 = r5; ctx.r6 = r6; ctx.r7 = r7; ctx.r8 = r8;
  ctx.r9 = r9; ctx.r10 = r10; ctx.r11 = r11; ctx.r12 = r12; ctx.ctr = ctr; ctx.lr = lr;
  ctx.cr0 = cr0; ctx.cr1 = cr1; ctx.cr5 = cr5; ctx.cr6 = cr6; ctx.cr7 = cr7;
}

// Reads the live skeleton's node count and parents once (called with g_mu held). Checks them against the table: a
// mismatch on a hand bone switches the poser off for this skeleton (wrong body, or the layout guess is wrong).
bool ReadLiveSkeleton(uint8_t* base, uint32_t skel) {
  const uint32_t nodes = REX_LOAD_U32(skel + kSkelNodes);
  if (nodes == 0u) return false;
  const uint32_t count = REX_LOAD_U32(nodes);
  if (count < kSharedBones || count > kMaxBones) {
    g_error = "skeleton has " + std::to_string(count) + " bones, expected 68 or more";
    return false;
  }
  std::vector<int> parent(count, -1);
  for (uint32_t b = 1; b < count; ++b)
    parent[b] = REX_LOAD_U16(nodes + kNodesHeader + kNodeBytes * b + kNodeParent);
  for (int b = 0; b < static_cast<int>(kSharedBones); ++b) {
    const int want = TableParent(b);
    if (want >= 0 && parent[b] != want) {
      g_error = "bone " + std::to_string(b) + " has parent " + std::to_string(parent[b]) + ", the table says " +
                std::to_string(want);
      REXGPU_INFO("[POSER] live skeleton does not match the table: {}; poser idle for it", g_error);
      return false;
    }
  }
  g_parent = parent;
  g_parent_live = true;
  REXGPU_INFO("[POSER] live skeleton read: {} bones, hand parents match the table", count);
  return true;
}

// ---- the files --------------------------------------------------------------------------------------------------
std::wstring DataDir() {
  wchar_t exe[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring dir(exe, n);
  dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
  for (const wchar_t* part : {L"reframework\\", L"data\\", L"hand_poser\\"}) {
    dir += part;
    CreateDirectoryW(dir.c_str(), nullptr);   // already there is fine
  }
  return dir;
}

std::string JsonStr(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (static_cast<unsigned char>(c) < 0x20) { char u[8]; std::snprintf(u, sizeof(u), "\\u%04x", c); out += u; }
    else out += c;
  }
  return out + "\"";
}

std::string Vec3(const std::array<float, 3>& v) {
  char t[96];
  std::snprintf(t, sizeof(t), "[%.3f,%.3f,%.3f]", v[0], v[1], v[2]);
  return t;
}

// Written to a side file and swapped in, as the window does with command.json, so it never reads half a file.
void WriteState(const std::wstring& dir) {
  std::string j;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    const bool connected = g_ever_called && std::chrono::steady_clock::now() - g_last_call <
                                                std::chrono::milliseconds(kConnectedMs);
    char head[160];
    std::snprintf(head, sizeof(head), "{\"game\":\"%s\",\"character\":\"%s\",\"connected\":%s,\"seq_seen\":%.0f,", kGame,
                  connected ? kCharacter : "", connected ? "true" : "false", g_seq_seen);
    j += head;
    j += "\"gun\":" + JsonStr(g_gun) + ",\"spot\":\"\",\"error\":" + (g_error.empty() ? "null" : JsonStr(g_error));
    j += ",\"joints\":[";
    const int n = g_parent_live ? static_cast<int>(g_parent.size()) : kBones;
    for (int b = 0; b < n; ++b) {
      int p = g_parent_live ? g_parent[b] : TableParent(b);
      if (b == 0 || p == b) p = -1;   // the root points at itself in the data
      j += (b ? ",[" : "[") + JsonStr(BoneName(b)) + "," + std::to_string(p) + "]";
    }
    j += "],\"angles\":{";
    bool first = true;
    for (const auto& kv : g_angles) {
      j += (first ? "" : ",") + JsonStr(BoneName(kv.first)) + ":" + Vec3(kv.second);
      first = false;
    }
    j += "},\"rel\":{";
    first = true;
    for (const auto& kv : g_rel) {
      j += (first ? "" : ",") + JsonStr(BoneName(kv.first)) + ":" + Vec3(kv.second);
      first = false;
    }
    j += "}}";
  }
  const std::wstring file = dir + L"state.json", tmp = file + L".tmp";
  if (FILE* f = _wfopen(tmp.c_str(), L"wb")) {
    std::fwrite(j.data(), 1, j.size(), f);
    std::fclose(f);
    MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING);
  }
}

// A small JSON reader, enough for command.json's shape (objects, arrays, strings, numbers, true/false/null).
struct JVal {
  enum Kind { kNull, kBool, kNum, kStr, kArr, kObj } kind = kNull;
  double num = 0;
  std::string str;
  std::vector<JVal> arr;
  std::vector<std::pair<std::string, JVal>> obj;
  const JVal* Get(const char* key) const {
    for (const auto& kv : obj)
      if (kv.first == key) return &kv.second;
    return nullptr;
  }
};

struct JParser {
  const char* p;
  const char* end;
  bool ok = true;
  void Ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p; }
  bool Eat(char c) { Ws(); if (p < end && *p == c) { ++p; return true; } return false; }
  std::string Str() {
    std::string s;
    if (!Eat('"')) { ok = false; return s; }
    while (p < end && *p != '"') {
      if (*p == '\\' && p + 1 < end) {
        ++p;
        if (*p == 'u' && p + 4 < end) { s += '?'; p += 5; continue; }   // names are ASCII; anything else is not ours
        s += (*p == 'n') ? '\n' : (*p == 't') ? '\t' : *p;
        ++p;
      } else {
        s += *p++;
      }
    }
    if (p < end) ++p; else ok = false;
    return s;
  }
  JVal Val(int depth = 0) {
    JVal v;
    Ws();
    if (p >= end || depth > 16) { ok = false; return v; }
    if (*p == '{') {
      ++p; v.kind = JVal::kObj;
      if (Eat('}')) return v;
      do { std::string k = Str(); if (!Eat(':')) { ok = false; return v; } v.obj.emplace_back(k, Val(depth + 1)); } while (ok && Eat(','));
      if (!Eat('}')) ok = false;
    } else if (*p == '[') {
      ++p; v.kind = JVal::kArr;
      if (Eat(']')) return v;
      do { v.arr.push_back(Val(depth + 1)); } while (ok && Eat(','));
      if (!Eat(']')) ok = false;
    } else if (*p == '"') {
      v.kind = JVal::kStr; v.str = Str();
    } else if (end - p >= 4 && !std::strncmp(p, "true", 4)) { v.kind = JVal::kBool; v.num = 1; p += 4; }
    else if (end - p >= 5 && !std::strncmp(p, "false", 5)) { v.kind = JVal::kBool; p += 5; }
    else if (end - p >= 4 && !std::strncmp(p, "null", 4)) { p += 4; }
    else {
      char* q = nullptr;
      std::string tok(p, static_cast<size_t>(std::min<ptrdiff_t>(end - p, 64)));
      v.num = std::strtod(tok.c_str(), &q);
      if (q == tok.c_str()) { ok = false; return v; }
      v.kind = JVal::kNum; p += q - tok.c_str();
    }
    return v;
  }
};

// Reads command.json when its seq changed. Holds are kept in radians, as the Lua does.
void ReadCommand(const std::wstring& dir) {
  FILE* f = _wfopen((dir + L"command.json").c_str(), L"rb");
  if (!f) return;
  std::string text;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
  std::fclose(f);
  JParser jp{text.data(), text.data() + text.size()};
  const JVal root = jp.Val();
  if (!jp.ok || root.kind != JVal::kObj) return;   // half-written or not ours: try again next poll
  const JVal* seq = root.Get("seq");
  if (!seq || seq->kind != JVal::kNum) return;
  std::lock_guard<std::mutex> lock(g_mu);
  if (seq->num == g_seq_seen) return;
  g_seq_seen = seq->num;
  g_held.clear();
  g_want.clear();
  static int unknown_lines = 0;
  if (const JVal* hold = root.Get("hold"); hold && hold->kind == JVal::kObj) {
    for (const auto& kv : hold->obj) {
      const int b = BoneOf(kv.first);
      int root_bone;
      if (b < 0 || HandOf(b, &root_bone) < 0) {
        if (unknown_lines++ < kUnknownLogMax) REXGPU_INFO("[POSER] not a hand joint, not held: {}", kv.first);
        continue;
      }
      if (kv.second.kind != JVal::kArr || kv.second.arr.size() < 3) continue;
      g_held[b] = {static_cast<float>(kv.second.arr[0].num / kDeg), static_cast<float>(kv.second.arr[1].num / kDeg),
                   static_cast<float>(kv.second.arr[2].num / kDeg)};
    }
  }
  if (const JVal* want = root.Get("want_read"); want && want->kind == JVal::kArr)
    for (const auto& w : want->arr) {
      const int b = w.kind == JVal::kStr ? BoneOf(w.str) : -1;
      if (b >= 0) g_want.push_back(b);
    }
  std::snprintf(g_text, sizeof(g_text), "seq %.0f: %zu held, %zu read", g_seq_seen, g_held.size(), g_want.size());
  REXGPU_INFO("[POSER] command {}", g_text);
  g_state_now = true;
}

void FileThread() {
  const std::wstring dir = DataDir();
  REXGPU_INFO("[POSER] link folder ready (reframework\\data\\hand_poser beside the exe)");
  auto last_state = std::chrono::steady_clock::now() - std::chrono::seconds(10);
  FILETIME seen = {};
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
    int32_t on;
    bool live;
    {
      std::lock_guard<std::mutex> lock(g_mu);
      on = g_on;
      live = !g_held.empty() || !g_want.empty();
    }
    if (on != 1) continue;
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExW((dir + L"command.json").c_str(), GetFileExInfoStandard, &fa) &&
        CompareFileTime(&fa.ftLastWriteTime, &seen) != 0) {
      seen = fa.ftLastWriteTime;
      ReadCommand(dir);
    }
    const auto now = std::chrono::steady_clock::now();
    if (g_state_now.exchange(false) ||
        now - last_state >= std::chrono::milliseconds(live ? kStateLiveMs : kStateEveryMs)) {
      WriteState(dir);
      last_state = now;
    }
  }
}

}  // namespace

namespace dkposer {

void Start() {
  static bool done = false;
  if (done) return;
  done = true;
  if (const char* v = std::getenv("DK_POSER")) g_on = v[0] == '1' ? 1 : 0;
  bridge_var("poser.on", BRIDGE_I32, &g_on, 1, "1 = talk to the Hand Poser window and hold the joints it asks for");
  bridge_var("poser.applied", BRIDGE_U32, &g_applied, 0, "joint turns written so far");
  bridge_var("poser.composes", BRIDGE_U32, &g_composes, 0, "calls to the game's bone composer so far");
  bridge_var_str("poser.text", g_text, sizeof(g_text), 0, "the link's state");
  std::thread(FileThread).detach();
  REXGPU_INFO("[POSER] link started, poser.on={}", g_on);
}

void NoteGun(uint32_t value) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_gun = value == 0u ? std::string() : "gun " + std::to_string(value);
}

void AfterIk(PPCContext& ctx, uint8_t* base, uint32_t skel, uint32_t si) {
  Start();
  if (skel == 0u || si == 0u) return;
  std::map<int, std::array<float, 3>> held;
  std::vector<int> want;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_on != 1) return;
    g_last_call = std::chrono::steady_clock::now();
    g_ever_called = true;
    if (!g_parent_live) {
      static uint32_t tried_skel = 0;
      if (tried_skel == skel) return;   // this skeleton did not match: stay idle on it
      tried_skel = skel;
      if (!ReadLiveSkeleton(base, skel)) return;
    }
    held = g_held;
    want = g_want;
  }
  const uint32_t locals = REX_LOAD_U32(si + kSiLocal), worlds = REX_LOAD_U32(si + kSiWorld);
  if (locals == 0u || worlds == 0u) return;

  // 1. the held joints' local 3x3 (rows 0-2, x/y/z; .w and row 3 left as the game wrote them)
  bool hand_held[2] = {false, false};
  bool root_held[2][5] = {};
  for (const auto& kv : held) {
    float m[3][3];
    EulerToRows(kv.second.data(), m);
    const uint32_t a = locals + kBoneBytes * static_cast<uint32_t>(kv.first);
    for (int k = 0; k < 3; ++k)
      for (int i = 0; i < 3; ++i) StoreGuestFloat(base, a + 16u * k + 4u * i, m[k][i]);
    int root;
    const int h = HandOf(kv.first, &root);
    if (root == kHand[h]) hand_held[h] = true;
    for (int f = 0; f < 5; ++f)
      if (root == kFingerRoot[h][f]) root_held[h][f] = true;
  }

  // 2. re-compose: from the hand when the hand itself is held (its fingers come along), else each held finger from
  //    its first joint, under the hand's world matrix
  uint32_t composes = 0;
  for (int h = 0; h < 2; ++h) {
    if (hand_held[h]) {
      Compose(ctx, base, skel, kHand[h], worlds + kBoneBytes * kHandParent[h], si);
      ++composes;
      continue;
    }
    for (int f = 0; f < 5; ++f)
      if (root_held[h][f]) {
        Compose(ctx, base, skel, kFingerRoot[h][f], worlds + kBoneBytes * kHand[h], si);
        ++composes;
      }
  }

  // 3. the numbers the window shows: local angles and the turn from the hand, for held and asked-for joints
  std::map<int, std::array<float, 3>> angles, rel;
  std::vector<int> report = want;
  for (const auto& kv : held) report.push_back(kv.first);
  for (int b : report) {
    if (angles.count(b)) continue;
    float m[3][3], e[3];
    ReadRows(base, locals + kBoneBytes * static_cast<uint32_t>(b), m);
    RowsToEulerDeg(m, e);
    angles[b] = {e[0], e[1], e[2]};
    int root;
    const int h = HandOf(b, &root);
    if (h < 0 || b == kHand[h]) continue;
    float wj[3][3], wh[3][3], r[3][3];
    ReadRows(base, worlds + kBoneBytes * static_cast<uint32_t>(b), wj);
    ReadRows(base, worlds + kBoneBytes * static_cast<uint32_t>(kHand[h]), wh);
    for (int i = 0; i < 3; ++i)   // rel = Wj * Wh^T (rows; rigid matrices assumed)
      for (int k = 0; k < 3; ++k) r[i][k] = wj[i][0] * wh[k][0] + wj[i][1] * wh[k][1] + wj[i][2] * wh[k][2];
    RowsToEulerDeg(r, e);
    rel[b] = {e[0], e[1], e[2]};
  }

  std::lock_guard<std::mutex> lock(g_mu);
  g_applied += static_cast<uint32_t>(held.size());
  g_composes += composes;
  g_angles = std::move(angles);
  g_rel = std::move(rel);
}

}  // namespace dkposer
