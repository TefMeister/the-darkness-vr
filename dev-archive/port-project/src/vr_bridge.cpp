// MCP Game Bridge³ in The Darkness (2026-10-23).
//
// Lets an assistant on the PC see and drive the running game through the bridge's named pipe
// (\\.\pipe\mcp-game-bridge-<pid>), with the game window behind other windows or not:
//   shot     the guest's finished frame, read back by the SDK's own D3D12Presenter::CaptureGuestOutput
//   log      the game's log, mirrored line by line through an extra spdlog sink
//   in       a virtual Xbox pad added to the SDK's input system as one more driver (see BridgePad)
//   get      switches (every DK_* variable this run started with), pad (the virtual pad right now)
//   quit     closes the game the way its own window close does
//
// Why a driver and not 0ut-0f-f0cus: the guest never hears about Windows focus at all. The SDK keeps
// running behind other windows, and only its real-pad drivers care whether the window is in front
// (SDL drops background pad events unless input_background_events is on). A driver of our own is read
// by XamInputGetState like any pad, whatever window is in front, so no focus trick is needed.
//
// The pad is synthetic, so it feeds guest user 0 without taking an ordinal: a real pad plugged in
// stays player one, and both are merged (buttons OR'd, larger stick wins).
//
// On by default; DK_BRIDGE=0 switches it off. It changes nothing the game sees until a command
// arrives: the idle pad reads neutral, and neutral input never takes the "active device" slot.
//
// Not wired yet: bridge_frame_mark (frames and fps read 0). The once-per-frame point is the swap
// hook in vr_viewport_probe.cpp, which waits on that file's move-only split first.

#include "vr_bridge.h"

#include <windows.h>

#include <rex/input/input_driver.h>
#include <rex/input/input_system.h>
#include <rex/logging/api.h>
#include <rex/logging/macros.h>
#include <rex/runtime.h>
#include <rex/system/interfaces/graphics.h>
#include <rex/ui/presenter.h>
#include <rex/ui/virtual_key.h>

#include <spdlog/sinks/base_sink.h>

#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "bridge/bridge.h"
}

namespace {

using rex::X_RESULT;
using rex::X_STATUS;

// ---- settings ------------------------------------------------------------------------------------
constexpr const char* kHostName = "the-darkness-vr";
constexpr const char* kHostVersion = "2026-10-23";
constexpr int kShotPollMs = 5;          // how often the picture thread looks for a waiting `shot`
constexpr int kTapMs = 150;             // a tap holds the button this long: several guest frames even at 15 fps
constexpr int kMaxHoldMs = 60000;       // longest hold one command may ask for
constexpr float kStickMax = 32767.0f;   // full stick deflection in X_INPUT_GAMEPAD units
constexpr size_t kMaxKeystrokes = 64;   // queued button events the guest has not read yet
// One device, so one handle; never reused, and distinct from the SDK's own (NOP is 0x4E4F5000).
constexpr rex::input::DeviceId kBridgePadId = static_cast<rex::input::DeviceId>(0xB21D6E000001ull);
constexpr uint8_t kDevTypeGamepad = 0x01;      // XINPUT_DEVTYPE_GAMEPAD
constexpr uint8_t kDevSubTypeGamepad = 0x01;   // XINPUT_DEVSUBTYPE_GAMEPAD
constexpr uint8_t kTriggerFull = 0xFF;
constexpr uint16_t kAllButtons = 0xFFFF;
constexpr int kButtonBits = 16;         // X_INPUT_GAMEPAD.buttons is 16 bits
constexpr uint8_t kAlphaOpaque = 0xFF;
constexpr size_t kInLineMax = 256;      // longest `in` line read
constexpr int kInMaxWords = 9;          // `pad` plus its seven numbers, and one spare
constexpr size_t kDescribeMax = 160;

using Clock = std::chrono::steady_clock;

bool Enabled() {
  static const bool on = [] {
    const char* e = std::getenv("DK_BRIDGE");
    return !(e && e[0] == '0');
  }();
  return on;
}

// ---- the virtual pad -----------------------------------------------------------------------------
struct PadState {
  uint16_t buttons = 0;
  uint8_t lt = 0, rt = 0;
  int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

// One timed part of the pad: when `until` passes, that part goes back to rest.
struct Timed {
  bool active = false;
  Clock::time_point until;
};

struct ButtonName {
  const char* name;
  uint16_t bit;
  rex::ui::VirtualKey vk;
  bool alias;  // a second name for a button listed before it: no second key event
};

using VK = rex::ui::VirtualKey;
// Keyboard-style aliases (enter, esc, arrows) let the bridge's generic `key` tool work here too.
constexpr ButtonName kButtons[] = {
    {"a", 0x1000, VK::kXInputPadA, false},          {"enter", 0x1000, VK::kXInputPadA, true},
    {"b", 0x2000, VK::kXInputPadB, false},          {"esc", 0x2000, VK::kXInputPadB, true},
    {"x", 0x4000, VK::kXInputPadX, false},          {"y", 0x8000, VK::kXInputPadY, false},
    {"start", 0x0010, VK::kXInputPadStart, false},  {"back", 0x0020, VK::kXInputPadBack, false},
    {"lb", 0x0100, VK::kXInputPadLShoulder, false}, {"rb", 0x0200, VK::kXInputPadRShoulder, false},
    {"ls", 0x0040, VK::kXInputPadLThumbPress, false}, {"rs", 0x0080, VK::kXInputPadRThumbPress, false},
    {"up", 0x0001, VK::kXInputPadDpadUp, false},    {"down", 0x0002, VK::kXInputPadDpadDown, false},
    {"left", 0x0004, VK::kXInputPadDpadLeft, false}, {"right", 0x0008, VK::kXInputPadDpadRight, false},
};
constexpr const char* kButtonHelp = "a b x y start back lb rb ls rs up down left right (enter = a, esc = b)";

class Pad {
 public:
  // Everything is read and written under one lock: the guest polls from its own thread, the
  // pipe thread writes, and the timers are checked on every read.
  PadState Read() {
    std::lock_guard lock(mutex_);
    Expire();
    ++reads_;
    return state_;
  }

  bool PopKeystroke(rex::input::X_INPUT_KEYSTROKE* out) {
    std::lock_guard lock(mutex_);
    Expire();
    if (keys_.empty()) return false;
    *out = keys_.front();
    keys_.pop_front();
    return true;
  }

  void SetButton(uint16_t bit, bool down, int hold_ms) {
    std::lock_guard lock(mutex_);
    Expire();
    SetButtonsLocked(down ? (state_.buttons | bit) : (state_.buttons & ~bit));
    for (int i = 0; i < kButtonBits; ++i) {
      if (bit & (1u << i)) {
        button_timer_[i].active = down && hold_ms > 0;
        button_timer_[i].until = Clock::now() + std::chrono::milliseconds(hold_ms);
      }
    }
  }

  void SetStick(bool right, int16_t x, int16_t y, int hold_ms) {
    std::lock_guard lock(mutex_);
    (right ? state_.rx : state_.lx) = x;
    (right ? state_.ry : state_.ly) = y;
    Timed& t = stick_timer_[right ? 1 : 0];
    t.active = hold_ms > 0;
    t.until = Clock::now() + std::chrono::milliseconds(hold_ms);
  }

  void SetTrigger(bool right, uint8_t v, int hold_ms) {
    std::lock_guard lock(mutex_);
    (right ? state_.rt : state_.lt) = v;
    Timed& t = trigger_timer_[right ? 1 : 0];
    t.active = hold_ms > 0;
    t.until = Clock::now() + std::chrono::milliseconds(hold_ms);
  }

  void SetAll(const PadState& s) {
    std::lock_guard lock(mutex_);
    ClearTimersLocked();
    SetButtonsLocked(s.buttons);
    const uint16_t buttons = state_.buttons;
    state_ = s;
    state_.buttons = buttons;
  }

  void Release() { SetAll(PadState{}); }

  std::string Describe() {
    std::lock_guard lock(mutex_);
    char buf[kDescribeMax];
    std::snprintf(buf, sizeof buf, "buttons=0x%04x lt=%u rt=%u lx=%d ly=%d rx=%d ry=%d reads=%llu keys_queued=%zu",
                  state_.buttons, state_.lt, state_.rt, state_.lx, state_.ly, state_.rx, state_.ry,
                  static_cast<unsigned long long>(reads_), keys_.size());
    return buf;
  }

  uint32_t NextPacket() { return ++packet_; }

 private:
  void Expire() {
    const auto now = Clock::now();
    uint16_t buttons = state_.buttons;
    for (int i = 0; i < kButtonBits; ++i) {
      if (button_timer_[i].active && now >= button_timer_[i].until) {
        button_timer_[i].active = false;
        buttons &= ~(1u << i);
      }
    }
    SetButtonsLocked(buttons);
    for (int s = 0; s < 2; ++s) {
      if (stick_timer_[s].active && now >= stick_timer_[s].until) {
        stick_timer_[s].active = false;
        (s ? state_.rx : state_.lx) = 0;
        (s ? state_.ry : state_.ly) = 0;
      }
      if (trigger_timer_[s].active && now >= trigger_timer_[s].until) {
        trigger_timer_[s].active = false;
        (s ? state_.rt : state_.lt) = 0;
      }
    }
  }

  // A change of buttons also queues key-down / key-up events, for menus that read
  // XInputGetKeystroke instead of the pad state.
  void SetButtonsLocked(uint16_t buttons) {
    const uint16_t changed = buttons ^ state_.buttons;
    if (!changed) return;
    for (const ButtonName& b : kButtons) {
      if (!(changed & b.bit) || b.alias) continue;
      if (keys_.size() >= kMaxKeystrokes) keys_.pop_front();
      rex::input::X_INPUT_KEYSTROKE k = {};
      k.virtual_key = static_cast<uint16_t>(b.vk);
      k.flags = (buttons & b.bit) ? rex::input::X_INPUT_KEYSTROKE_KEYDOWN : rex::input::X_INPUT_KEYSTROKE_KEYUP;
      k.user_index = 0;
      keys_.push_back(k);
    }
    state_.buttons = buttons;
  }

  void ClearTimersLocked() {
    for (Timed& t : button_timer_) t.active = false;
    for (Timed& t : stick_timer_) t.active = false;
    for (Timed& t : trigger_timer_) t.active = false;
  }

  std::mutex mutex_;
  PadState state_;
  Timed button_timer_[kButtonBits];
  Timed stick_timer_[2];
  Timed trigger_timer_[2];
  std::deque<rex::input::X_INPUT_KEYSTROKE> keys_;
  uint64_t reads_ = 0;
  uint32_t packet_ = 0;
};

Pad g_pad;

// 2026-10-29 (Tefa: "map quest 3 buttons to the darkness, at least the main ones"): the motion controllers as a pad,
// read from the SDK's OpenXR session (rex_xr_sample_buttons, SDK patch 11) and laid over the bridge's virtual pad; the
// input system then merges this device with the real Xbox pad, so both work at once. A/B/X/Y as they are, the left
// menu button = START, the stick clicks = the stick clicks, triggers = triggers, the grips = LB / RB, sticks = sticks.
// On with DK_XR=1 unless DK_XR_PAD=0.
using XrButtonsFn = int (*)(uint32_t*, float*);
XrButtonsFn XrButtons() {
  static const XrButtonsFn fn = [] {
    const char* xr = std::getenv("DK_XR");
    const char* pad = std::getenv("DK_XR_PAD");
    const bool on = (pad ? pad[0] == '1' : (xr && xr[0] == '1'));
    if (!on) return XrButtonsFn(nullptr);
    HMODULE gpu = GetModuleHandleW(L"rexgpu-xenos.dll");
    XrButtonsFn f = gpu ? reinterpret_cast<XrButtonsFn>(reinterpret_cast<void*>(GetProcAddress(gpu, "rex_xr_sample_buttons"))) : nullptr;
    REXGPU_INFO("[DKXR] controller pad {}", f ? "on: the motion controllers also work the pad (rex_xr_sample_buttons)" : "NOT in this SDK build");
    return f;
  }();
  return fn;
}
constexpr float kXrStickDead = 0.15f;   // a stick this far from the middle takes over from the virtual pad's
// 2026-10-29, first wear (Tefa): "quest A - A, Quest B - X, Quest X - B and Quest Y - Y on a gamepad"; "long press of the
// menu button should be the start button, click the select button". The SDK hands the Quest buttons over as they are
// (A 0x1000 B 0x2000 X 0x4000 Y 0x8000, menu as 0x0010); the swap and the menu timing happen here.
constexpr uint32_t kXrA = 0x1000, kXrB = 0x2000, kXrX = 0x4000, kXrY = 0x8000, kXrMenu = 0x0010;
constexpr uint32_t kPadStart = 0x0010, kPadBack = 0x0020;
constexpr int kMenuHoldMs = 600;    // held this long = START; let go sooner = one tap of BACK (select)
constexpr int kMenuTapFrames = 6;   // how many pad reads the BACK tap is held for
uint32_t RemapXr(uint32_t b) {
  static std::chrono::steady_clock::time_point menu_since;
  static bool menu_was = false, start_sent = false;
  static int back_frames = 0;
  uint32_t out = b & ~(kXrA | kXrB | kXrX | kXrY | kXrMenu);
  if (b & kXrA) out |= kXrA;
  if (b & kXrB) out |= kXrX;   // Quest B = pad X
  if (b & kXrX) out |= kXrB;   // Quest X = pad B
  if (b & kXrY) out |= kXrY;
  const bool menu = (b & kXrMenu) != 0;
  const auto now = std::chrono::steady_clock::now();
  if (menu && !menu_was) { menu_since = now; start_sent = false; }
  if (menu) {
    const auto held = std::chrono::duration_cast<std::chrono::milliseconds>(now - menu_since).count();
    if (held >= kMenuHoldMs) { out |= kPadStart; start_sent = true; }
  } else if (menu_was && !start_sent) {
    back_frames = kMenuTapFrames;   // a short click: one tap of BACK
  }
  if (back_frames > 0) { out |= kPadBack; --back_frames; }
  menu_was = menu;
  return out;
}

class BridgePadDriver final : public rex::input::InputDriver {
 public:
  BridgePadDriver() : InputDriver(nullptr, 0) {}

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }

  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override {
    rex::input::DeviceInfo info;
    info.id = kBridgePadId;
    info.name = "MCP Game Bridge pad";
    info.synthetic = true;
    out.push_back(info);
  }

  X_RESULT GetDeviceState(rex::input::DeviceId id, rex::input::X_INPUT_STATE* out) override {
    if (id != kBridgePadId) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (out) {
      const PadState s = g_pad.Read();
      std::memset(out, 0, sizeof(*out));
      out->packet_number = g_pad.NextPacket();
      out->gamepad.buttons = s.buttons;
      out->gamepad.left_trigger = s.lt;
      out->gamepad.right_trigger = s.rt;
      out->gamepad.thumb_lx = s.lx;
      out->gamepad.thumb_ly = s.ly;
      out->gamepad.thumb_rx = s.rx;
      out->gamepad.thumb_ry = s.ry;
      if (const XrButtonsFn xr = XrButtons()) {   // 2026-10-29: the motion controllers laid over
        uint32_t b = 0; float ax[6];
        if (xr(&b, ax)) {
          out->gamepad.buttons = static_cast<uint16_t>(out->gamepad.buttons | RemapXr(b));
          const auto trig = [](float v) { return static_cast<uint8_t>(v <= 0 ? 0 : v >= 1 ? 255 : v * 255.0f + 0.5f); };
          const auto axis = [](float v) { return static_cast<int16_t>(v <= -1 ? -32768 : v >= 1 ? 32767 : v * 32767.0f); };
          if (trig(ax[0]) > out->gamepad.left_trigger) out->gamepad.left_trigger = trig(ax[0]);
          if (trig(ax[1]) > out->gamepad.right_trigger) out->gamepad.right_trigger = trig(ax[1]);
          if (std::fabs(ax[2]) > kXrStickDead || std::fabs(ax[3]) > kXrStickDead) { out->gamepad.thumb_lx = axis(ax[2]); out->gamepad.thumb_ly = axis(ax[3]); }
          if (std::fabs(ax[4]) > kXrStickDead || std::fabs(ax[5]) > kXrStickDead) { out->gamepad.thumb_rx = axis(ax[4]); out->gamepad.thumb_ry = axis(ax[5]); }
        }
      }
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceCapabilities(rex::input::DeviceId id, uint32_t flags,
                                 rex::input::X_INPUT_CAPABILITIES* out) override {
    (void)flags;
    if (id != kBridgePadId) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (out) {
      std::memset(out, 0, sizeof(*out));
      out->type = kDevTypeGamepad;
      out->sub_type = kDevSubTypeGamepad;
      out->gamepad.buttons = kAllButtons;
      out->gamepad.left_trigger = kTriggerFull;
      out->gamepad.right_trigger = kTriggerFull;
      out->gamepad.thumb_lx = static_cast<int16_t>(kStickMax);
      out->gamepad.thumb_ly = static_cast<int16_t>(kStickMax);
      out->gamepad.thumb_rx = static_cast<int16_t>(kStickMax);
      out->gamepad.thumb_ry = static_cast<int16_t>(kStickMax);
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT SetDeviceVibration(rex::input::DeviceId id, rex::input::X_INPUT_VIBRATION* v) override {
    (void)v;
    return id == kBridgePadId ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED;
  }

  X_RESULT GetDeviceKeystroke(rex::input::DeviceId id, uint32_t flags,
                              rex::input::X_INPUT_KEYSTROKE* out) override {
    (void)flags;
    if (id != kBridgePadId) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (!out) return X_ERROR_BAD_ARGUMENTS;
    return g_pad.PopKeystroke(out) ? X_ERROR_SUCCESS : X_ERROR_EMPTY;
  }
};

// ---- input commands (the bridge's `in`) --------------------------------------------------------------
const ButtonName* FindButton(const char* name) {
  for (const ButtonName& b : kButtons) {
    if (_stricmp(b.name, name) == 0) return &b;
  }
  return nullptr;
}

int ClampHold(const char* text, int fallback) {
  if (!text) return fallback;
  const int ms = std::atoi(text);
  return ms < 0 ? 0 : (ms > kMaxHoldMs ? kMaxHoldMs : ms);
}

int16_t StickValue(const char* text) {
  float v = static_cast<float>(std::atof(text));
  v = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
  return static_cast<int16_t>(v * kStickMax);
}

int Fail(char* reply, size_t room, const char* why) {
  std::snprintf(reply, room, "%s", why);
  return -1;
}

constexpr const char* kInHelp =
    "The Darkness reads only the game pad: tap <button> [ms] | key <button> 0|1 | stick l|r <x> <y> [ms] "
    "(x, y from -1 to 1; up is +y) | trigger l|r <0-255> [ms] | pad <hex> <lt> <rt> <lx> <ly> <rx> <ry> | "
    "pad off | release | state | infront";

int InputCommand(const char* line, char* reply, size_t room) {
  char buf[256];
  std::snprintf(buf, sizeof buf, "%s", line ? line : "");
  const char* w[9] = {};
  int n = 0;
  for (char* tok = std::strtok(buf, " \t"); tok && n < 9; tok = std::strtok(nullptr, " \t")) w[n++] = tok;
  reply[0] = 0;
  if (n == 0) return Fail(reply, room, kInHelp);
  const std::string cmd = w[0];

  if (cmd == "tap" || cmd == "key") {
    if (n < 2) return Fail(reply, room, kInHelp);
    const ButtonName* b = FindButton(w[1]);
    if (!b) {
      std::snprintf(reply, room, "no pad button called %s; buttons: %s", w[1], kButtonHelp);
      return -1;
    }
    if (cmd == "tap") {
      g_pad.SetButton(b->bit, true, ClampHold(n > 2 ? w[2] : nullptr, kTapMs));
    } else {
      g_pad.SetButton(b->bit, n > 2 && w[2][0] == '1', 0);
    }
    return 0;
  }
  if (cmd == "stick" && n >= 4) {
    g_pad.SetStick(w[1][0] == 'r', StickValue(w[2]), StickValue(w[3]), ClampHold(n > 4 ? w[4] : nullptr, 0));
    return 0;
  }
  if (cmd == "trigger" && n >= 3) {
    const int v = std::atoi(w[2]);
    g_pad.SetTrigger(w[1][0] == 'r', static_cast<uint8_t>(v < 0 ? 0 : (v > kTriggerFull ? kTriggerFull : v)),
                     ClampHold(n > 3 ? w[3] : nullptr, 0));
    return 0;
  }
  if (cmd == "pad") {
    if (n >= 2 && std::strcmp(w[1], "off") == 0) {
      g_pad.Release();
      return 0;
    }
    if (n < 8) return Fail(reply, room, kInHelp);
    PadState s;
    s.buttons = static_cast<uint16_t>(std::strtoul(w[1], nullptr, 16));
    s.lt = static_cast<uint8_t>(std::atoi(w[2]));
    s.rt = static_cast<uint8_t>(std::atoi(w[3]));
    s.lx = static_cast<int16_t>(std::atoi(w[4]));
    s.ly = static_cast<int16_t>(std::atoi(w[5]));
    s.rx = static_cast<int16_t>(std::atoi(w[6]));
    s.ry = static_cast<int16_t>(std::atoi(w[7]));
    g_pad.SetAll(s);
    return 0;
  }
  if (cmd == "release") {
    g_pad.Release();
    return 0;
  }
  if (cmd == "state" || cmd == "stats") {
    std::snprintf(reply, room, "%s", g_pad.Describe().c_str());
    return 0;
  }
  if (cmd == "infront") {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    std::snprintf(reply, room, "%d", pid == GetCurrentProcessId() ? 1 : 0);
    return 0;
  }
  if (cmd == "version") {
    std::snprintf(reply, room, "the-darkness bridge pad %s", kHostVersion);
    return 0;
  }
  return Fail(reply, room, kInHelp);
}

// ---- the game window ----------------------------------------------------------------------------------
struct FindCtx {
  HWND best = nullptr;
  LONG best_area = 0;
};

BOOL CALLBACK FindWindowProc(HWND hwnd, LPARAM lp) {
  auto* c = reinterpret_cast<FindCtx*>(lp);
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
  RECT r;
  GetWindowRect(hwnd, &r);
  const LONG area = (r.right - r.left) * (r.bottom - r.top);
  if (area > c->best_area) {
    c->best_area = area;
    c->best = hwnd;
  }
  return TRUE;
}

HWND GameWindow() {
  FindCtx c;
  EnumWindows(FindWindowProc, reinterpret_cast<LPARAM>(&c));
  return c.best;
}

// ---- pictures -------------------------------------------------------------------------------------------
rex::Runtime* g_runtime = nullptr;
std::atomic<bool> g_stop{false};
std::thread g_shot_thread;
HWND g_window = nullptr;

bool SideBySide() {
  const char* e = std::getenv("DK_SBS");
  return e && e[0] == '1';
}

void ShotLoop() {
  rex::ui::RawImage img;
  std::vector<uint8_t> bgra;
  while (!g_stop.load()) {
    if (!g_window) {
      g_window = GameWindow();
      if (g_window) bridge_set_window(g_window);
    }
    if (!bridge_shot_wanted()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kShotPollMs));
      continue;
    }
    auto* gs = g_runtime ? g_runtime->graphics_system() : nullptr;
    auto* presenter = gs ? gs->presenter() : nullptr;
    // No frame yet (still loading): leave the request waiting; the bridge times it out itself.
    if (!presenter || !presenter->CaptureGuestOutput(img) || !img.width || !img.height) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kShotPollMs));
      continue;
    }
    // The presenter gives R8 G8 B8 X8; the bridge wants B G R A, rows packed.
    const size_t row = static_cast<size_t>(img.width) * 4;
    bgra.resize(row * img.height);
    for (uint32_t y = 0; y < img.height; ++y) {
      const uint8_t* src = img.data.data() + y * img.stride;
      uint8_t* dst = bgra.data() + y * row;
      for (uint32_t x = 0; x < img.width; ++x) {
        dst[x * 4 + 0] = src[x * 4 + 2];
        dst[x * 4 + 1] = src[x * 4 + 1];
        dst[x * 4 + 2] = src[x * 4 + 0];
        dst[x * 4 + 3] = kAlphaOpaque;
      }
    }
    bridge_shot_submit(img.width, img.height, static_cast<unsigned>(row), bgra.data(),
                       SideBySide() ? BRIDGE_SHOT_SBS : 0);
  }
}

// ---- the game's log into the bridge's ring ---------------------------------------------------------------
class BridgeLogSink final : public spdlog::sinks::base_sink<std::mutex> {
 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    std::string line(msg.payload.data(), msg.payload.size());
    bridge_log_line(line.c_str());
  }
  void flush_() override {}
};

// ---- vars and commands ------------------------------------------------------------------------------------
void GetSwitches(char* out, size_t room, void* ctx) {
  (void)ctx;
  std::string all;
  if (char* block = GetEnvironmentStringsA()) {
    for (const char* e = block; *e; e += std::strlen(e) + 1) {
      if (std::strncmp(e, "DK_", 3) == 0) {
        if (!all.empty()) all += ' ';
        all += e;
      }
    }
    FreeEnvironmentStringsA(block);
  }
  std::snprintf(out, room, "%s", all.empty() ? "(none: every DK_ switch at its default)" : all.c_str());
}

void GetPad(char* out, size_t room, void* ctx) {
  (void)ctx;
  std::snprintf(out, room, "%s", g_pad.Describe().c_str());
}

void CmdQuit(int argc, const char** argv, bridge_reply* r, void* ctx) {
  (void)argc;
  (void)argv;
  (void)ctx;
  HWND hwnd = g_window ? g_window : GameWindow();
  if (!hwnd) {
    bridge_reply_err(r, "no game window yet");
    return;
  }
  // The same path as clicking the window's close button: the SDK shuts the runtime down itself.
  PostMessageW(hwnd, WM_CLOSE, 0, 0);
  bridge_reply_line(r, "close requested");
}

}  // namespace

std::unique_ptr<rex::system::IInputSystem> DkCreateInputSystem(bool tool_mode) {
  auto input = rex::input::CreateDefaultInputSystem(tool_mode);
  if (input && !tool_mode && Enabled()) {
    input->AddDriver(std::make_unique<BridgePadDriver>());
  }
  return input;
}

void DkBridgeLogging() {
  if (Enabled()) rex::AddSink(std::make_shared<BridgeLogSink>());
}

void DkBridgeStart(rex::Runtime* runtime) {
  if (!Enabled() || bridge_running()) return;
  g_runtime = runtime;
  bridge_set_api("d3d12 (ReXGlue xenos, Xbox 360 recompilation)");
  bridge_set_input(InputCommand);
  bridge_var_fn("switches", GetSwitches, nullptr, nullptr, "every DK_ switch this run started with");
  bridge_var_fn("pad", GetPad, nullptr, nullptr, "the bridge's virtual pad right now");
  bridge_cmd("quit", CmdQuit, nullptr, 0, "close the game the way its window's close button does");
  if (bridge_start(kHostName, kHostVersion)) {
    g_shot_thread = std::thread(ShotLoop);
  }
}

void DkBridgeStop() {
  g_stop.store(true);
  if (g_shot_thread.joinable()) g_shot_thread.join();
  bridge_stop();
}
