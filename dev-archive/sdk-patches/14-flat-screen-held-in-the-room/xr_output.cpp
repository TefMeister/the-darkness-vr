// Patch 09 (DK_XR=1): hand each eye of the stereo pair to an OpenXR headset. See xr_output.h.
//
// The pair comes from patch 08b (DK_SBS=1 DK_SBS_HOLD=1): two W x H textures, the same moment
// seen from two eyes. Each pair becomes one OpenXR frame: wait, begin, draw both eyes into the
// headset's images, end with one projection layer.
//
// Which pose goes with the picture: the game program samples the head (rex_xr_sample_head) when it
// builds a pair's camera and tells us which sample it used (its dk_xr_pair_sample). We send the eye
// poses of that very sample, so the headset can correct for the time in between. A game program
// without that export gets the newest pose instead: the picture then stays fixed to the face.
//
// Which field of view goes with the picture: the game program's dk_xr_eye_fov (the tangents it
// really draws with) when it has one, else the headset's own.
//
// Settings: DK_XR=1 on. DK_XR_LOOKAHEAD=<frames> (default 2): how far ahead a head sample is
// predicted, since a pair is shown a frame or two after its camera is built. XR_RUNTIME_JSON picks
// the runtime for this process only (the OpenXR loader reads it). openxr_loader.dll is loaded from
// beside the exe.

#include "rex/graphics/d3d12/xr_output.h"

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "xr_blit.h"

namespace rex::graphics::d3d12 {

namespace {

constexpr int kSampleRing = 64;
constexpr uint64_t kReportEvery = 300;

struct Sample {
  uint32_t id = 0;
  XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
};

struct Chain {
  XrSwapchain handle = XR_NULL_HANDLE;
  std::vector<XrSwapchainImageD3D12KHR> images;
};

#define DK_XR_FUNCS(X)                                                                         \
  X(xrDestroyInstance) X(xrGetSystem) X(xrGetD3D12GraphicsRequirementsKHR) X(xrCreateSession)  \
  X(xrDestroySession) X(xrBeginSession) X(xrEndSession) X(xrPollEvent)                         \
  X(xrCreateReferenceSpace) X(xrEnumerateSwapchainFormats) X(xrCreateSwapchain)                \
  X(xrDestroySwapchain) X(xrEnumerateSwapchainImages) X(xrAcquireSwapchainImage)               \
  X(xrWaitSwapchainImage) X(xrReleaseSwapchainImage) X(xrWaitFrame) X(xrBeginFrame)            \
  X(xrEndFrame) X(xrLocateViews) X(xrLocateSpace)

// Patch 10 (draft): the controllers. Loaded apart from the above so that a runtime missing any of
// them only switches the hands off, never the headset picture.
#define DK_XR_HAND_FUNCS(X)                                                                    \
  X(xrStringToPath) X(xrCreateActionSet) X(xrCreateAction) X(xrSuggestInteractionProfileBindings) \
  X(xrAttachSessionActionSets) X(xrCreateActionSpace) X(xrSyncActions) X(xrGetActionStatePose) \
  X(xrGetActionStateBoolean) X(xrGetActionStateFloat) X(xrGetActionStateVector2f)

struct State {
  bool tried = false, failed = false, exiting = false;
  std::atomic<bool> running{false};
  PFN_xrGetInstanceProcAddr gipa = nullptr;
#define DK_XR_DECLARE(f) PFN_##f f = nullptr;
  DK_XR_FUNCS(DK_XR_DECLARE)
#undef DK_XR_DECLARE
  XrInstance instance = XR_NULL_HANDLE;
  XrSystemId system = XR_NULL_SYSTEM_ID;
  XrSession session = XR_NULL_HANDLE;
  XrSpace local = XR_NULL_HANDLE, view = XR_NULL_HANDLE;
  Chain chains[2];
  uint32_t chain_w = 0, chain_h = 0;
  int64_t format = 0;
  xr::EyeBlit blit;
  bool blit_ready = false;

  // Shared with the game's frame thread (rex_xr_sample_head).
  std::mutex mutex;
  Sample ring[kSampleRing];
  uint32_t next_id = 0;
  XrTime predicted = 0;
  XrDuration period = 0;
  int lookahead = 2;
  // Patch 09b: the headset's own view, both eyes together (left, right, up, down tangents, all
  // positive), refreshed with every head sample, for rex_xr_headset_tangents.
  float head_tan[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  bool head_tan_ok = false;
  float eye_dist_m = 0.0f;  // distance between the two eye positions, for rex_xr_eye_distance

  // From the game program, looked up once.
  int (*pair_sample)(uint32_t pair, uint32_t* id) = nullptr;
  int (*eye_fov)(float tangents[4]) = nullptr;
  // Patch 14 (2026-11-10): the flat pictures (menus, films) as a screen that stays put in the room. The game program's
  // dk_xr_flat() says when the picture is a flat one; the layer is then submitted with the eye poses of the moment it
  // began (turned level, yaw only) instead of the head's newest, so the runtime keeps it where it was while the head
  // turns, and with its tangents scaled by DK_XR_FLAT_SCALE (default 0.75) so it is smaller.
  int (*flat)() = nullptr;
  bool flat_have = false;
  XrPosef flat_pose[2] = {};
  float flat_scale = 0.75f;

  uint64_t frames = 0, not_rendered = 0, sample_hits = 0, sample_misses = 0, blit_errors = 0;
  double wait_ms_sum = 0.0;

  // Patch 10 (draft): the controllers. Set up once in Start, used from the game's frame thread.
#define DK_XR_DECLARE(f) PFN_##f f = nullptr;
  DK_XR_HAND_FUNCS(DK_XR_DECLARE)
#undef DK_XR_DECLARE
  bool hands_ready = false;
  std::atomic<bool> focused{false};  // xrSyncActions only takes effect in the FOCUSED state
  XrActionSet hand_set = XR_NULL_HANDLE;
  XrAction grip_action = XR_NULL_HANDLE, aim_action = XR_NULL_HANDLE;
  // Patch 11: the buttons. One action per button (no subaction: a and b live on the right hand, x,
  // y and menu on the left), and three two-handed actions (trigger, squeeze, thumbstick).
  enum { BTN_A, BTN_B, BTN_X, BTN_Y, BTN_MENU, BTN_LSTICK, BTN_RSTICK, BTN_COUNT };
  XrAction btn_action[BTN_COUNT] = {};
  XrAction trigger_action = XR_NULL_HANDLE, squeeze_action = XR_NULL_HANDLE, stick_action = XR_NULL_HANDLE;
  uint64_t button_calls = 0;
  XrPath hand_path[2] = {XR_NULL_PATH, XR_NULL_PATH};  // /user/hand/left, /user/hand/right
  XrSpace grip_space[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
  XrSpace aim_space[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
  std::mutex hand_mutex;  // our own sync + locate calls run one at a time
  uint32_t hand_next_id = 0, aim_id = 0;
  float aim[2][7] = {{0, 0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 0, 1}};
  uint64_t hand_calls = 0, hand_unsynced = 0, hand_left_ok = 0, hand_right_ok = 0;
};

State& S() {
  static State s;
  return s;
}

int64_t RtvTwin(int64_t f) {  // the non-sRGB format with the same bytes
  switch (f) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
    default: return f;
  }
}

bool IsTypeless(DXGI_FORMAT f) {
  return f == DXGI_FORMAT_R8G8B8A8_TYPELESS || f == DXGI_FORMAT_B8G8R8A8_TYPELESS ||
         f == DXGI_FORMAT_R10G10B10A2_TYPELESS;
}

bool IsSrgb(int64_t f) {
  return f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}

XrFovf FovFromTangents(const float t[4]) {
  return {-std::atan(t[0]), std::atan(t[1]), std::atan(t[2]), -std::atan(t[3])};
}

}  // namespace

// Patch 13 (2026-11-06, Tefa: "light is too bright"): the brightness dial, pow(c, dial) on every
// stored value before the headset gets it. DK_XR_GAMMA sets it at launch (1.0 = unchanged); the
// game's bridge can turn it live through rex_xr_brightness_dial(). DK_XR_EYE_LIGHT=0 stops the
// per-eye brightness log.
float g_xr_dial = [] {
  const char* e = std::getenv("DK_XR_GAMMA");
  const float v = e ? float(std::atof(e)) : 1.0f;
  return (v > 0.2f && v < 5.0f) ? v : 1.0f;
}();
bool XrEyeLightWanted() {
  static const bool on = [] {
    const char* e = std::getenv("DK_XR_EYE_LIGHT");
    return e && e[0] == '1';  // 2026-11-10: off unless asked; it copies both eye pictures off the card every pair
  }();
  return on;
}

XrOutput& XrOutput::Get() {
  static XrOutput o;
  return o;
}

bool XrOutput::Wanted() {
  static const bool on = [] {
    const char* e = std::getenv("DK_XR");
    return e && e[0] == '1';
  }();
  return on;
}

void XrOutput::Log(const char* fmt, ...) {
  char line[512];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  if (log_) {
    log_(line);
  } else {
    OutputDebugStringA(line);
    OutputDebugStringA("\n");
  }
}

namespace {

bool Fail(XrOutput& o, State& s, const char* what, XrResult r) {
  o.Log("[XR] OFF: %s (XrResult %d)", what, int(r));
  s.failed = true;
  return false;
}

// Patch 10 (draft): one action set with a grip pose and an aim pose for each hand, bound for the
// common controller profiles, attached to the session, with one action space per hand and pose.
// Must run after xrCreateSession and before the first xrSyncActions; attach is once per session.
// Any failure leaves hands_ready false and the headset untouched.
bool HandsOff(XrOutput& o, const char* what, XrResult r) {
  o.Log("[XR] hands OFF: %s (XrResult %d)", what, int(r));
  return false;
}

bool StartHands(XrOutput& o, State& s) {
#define DK_XR_LOAD(f)                                                                       \
  if (XR_FAILED(s.gipa(s.instance, #f, reinterpret_cast<PFN_xrVoidFunction*>(&s.f))))     \
    return HandsOff(o, "missing " #f, XR_ERROR_FUNCTION_UNSUPPORTED);
  DK_XR_HAND_FUNCS(DK_XR_LOAD)
#undef DK_XR_LOAD
  XrResult r;
  if (XR_FAILED(r = s.xrStringToPath(s.instance, "/user/hand/left", &s.hand_path[0])) ||
      XR_FAILED(r = s.xrStringToPath(s.instance, "/user/hand/right", &s.hand_path[1])))
    return HandsOff(o, "hand paths", r);

  XrActionSetCreateInfo asi = {XR_TYPE_ACTION_SET_CREATE_INFO};
  std::strcpy(asi.actionSetName, "dk_hands");
  std::strcpy(asi.localizedActionSetName, "Hands");
  asi.priority = 0;
  if (XR_FAILED(r = s.xrCreateActionSet(s.instance, &asi, &s.hand_set)))
    return HandsOff(o, "xrCreateActionSet", r);

  XrActionCreateInfo aci = {XR_TYPE_ACTION_CREATE_INFO};
  aci.actionType = XR_ACTION_TYPE_POSE_INPUT;
  aci.countSubactionPaths = 2;
  aci.subactionPaths = s.hand_path;
  std::strcpy(aci.actionName, "grip_pose");
  std::strcpy(aci.localizedActionName, "Grip pose");
  if (XR_FAILED(r = s.xrCreateAction(s.hand_set, &aci, &s.grip_action)))
    return HandsOff(o, "grip action", r);
  std::strcpy(aci.actionName, "aim_pose");
  std::strcpy(aci.localizedActionName, "Aim pose");
  if (XR_FAILED(r = s.xrCreateAction(s.hand_set, &aci, &s.aim_action)))
    return HandsOff(o, "aim action", r);

  // Patch 11: the buttons (Tefa 2026-10-29: "map quest 3 buttons to the darkness, at least the main ones").
  {
    static const char* const kBtnNames[State::BTN_COUNT][2] = {
        {"a_click", "A"}, {"b_click", "B"}, {"x_click", "X"}, {"y_click", "Y"},
        {"menu_click", "Menu"}, {"lstick_click", "Left stick click"}, {"rstick_click", "Right stick click"}};
    XrActionCreateInfo bci = {XR_TYPE_ACTION_CREATE_INFO};
    bci.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    for (int i = 0; i < rex::graphics::d3d12::State::BTN_COUNT; ++i) {
      std::strcpy(bci.actionName, kBtnNames[i][0]);
      std::strcpy(bci.localizedActionName, kBtnNames[i][1]);
      if (XR_FAILED(r = s.xrCreateAction(s.hand_set, &bci, &s.btn_action[i])))
        return HandsOff(o, kBtnNames[i][0], r);
    }
    aci.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    std::strcpy(aci.actionName, "trigger_value");
    std::strcpy(aci.localizedActionName, "Trigger");
    if (XR_FAILED(r = s.xrCreateAction(s.hand_set, &aci, &s.trigger_action)))
      return HandsOff(o, "trigger action", r);
    std::strcpy(aci.actionName, "squeeze_value");
    std::strcpy(aci.localizedActionName, "Grip");
    if (XR_FAILED(r = s.xrCreateAction(s.hand_set, &aci, &s.squeeze_action)))
      return HandsOff(o, "squeeze action", r);
    aci.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
    std::strcpy(aci.actionName, "thumbstick");
    std::strcpy(aci.localizedActionName, "Thumbstick");
    if (XR_FAILED(r = s.xrCreateAction(s.hand_set, &aci, &s.stick_action)))
      return HandsOff(o, "thumbstick action", r);
  }

  // Every profile below has input/grip/pose and input/aim/pose on both hands. The runtime picks
  // whichever matches the controllers in use; one it does not know is refused and skipped.
  // Patch 11: each profile also gets the button bindings IT has (a path a profile lacks would get
  // the whole profile refused), so Touch and Index get the full pad, the older ones what they can.
  struct Bind { XrAction action; const char* path; };
  const Bind kPose[] = {
      {s.grip_action, "/user/hand/left/input/grip/pose"}, {s.grip_action, "/user/hand/right/input/grip/pose"},
      {s.aim_action, "/user/hand/left/input/aim/pose"}, {s.aim_action, "/user/hand/right/input/aim/pose"}};
  const Bind kTouch[] = {
      {s.btn_action[State::BTN_A], "/user/hand/right/input/a/click"}, {s.btn_action[State::BTN_B], "/user/hand/right/input/b/click"},
      {s.btn_action[State::BTN_X], "/user/hand/left/input/x/click"}, {s.btn_action[State::BTN_Y], "/user/hand/left/input/y/click"},
      {s.btn_action[State::BTN_MENU], "/user/hand/left/input/menu/click"},
      {s.btn_action[State::BTN_LSTICK], "/user/hand/left/input/thumbstick/click"}, {s.btn_action[State::BTN_RSTICK], "/user/hand/right/input/thumbstick/click"},
      {s.trigger_action, "/user/hand/left/input/trigger/value"}, {s.trigger_action, "/user/hand/right/input/trigger/value"},
      {s.squeeze_action, "/user/hand/left/input/squeeze/value"}, {s.squeeze_action, "/user/hand/right/input/squeeze/value"},
      {s.stick_action, "/user/hand/left/input/thumbstick"}, {s.stick_action, "/user/hand/right/input/thumbstick"}};
  const Bind kIndex[] = {   // Index: a and b on both hands; the left pair stands in for X and Y
      {s.btn_action[State::BTN_A], "/user/hand/right/input/a/click"}, {s.btn_action[State::BTN_B], "/user/hand/right/input/b/click"},
      {s.btn_action[State::BTN_X], "/user/hand/left/input/a/click"}, {s.btn_action[State::BTN_Y], "/user/hand/left/input/b/click"},
      {s.btn_action[State::BTN_LSTICK], "/user/hand/left/input/thumbstick/click"}, {s.btn_action[State::BTN_RSTICK], "/user/hand/right/input/thumbstick/click"},
      {s.trigger_action, "/user/hand/left/input/trigger/value"}, {s.trigger_action, "/user/hand/right/input/trigger/value"},
      {s.squeeze_action, "/user/hand/left/input/squeeze/value"}, {s.squeeze_action, "/user/hand/right/input/squeeze/value"},
      {s.stick_action, "/user/hand/left/input/thumbstick"}, {s.stick_action, "/user/hand/right/input/thumbstick"}};
  const Bind kSimple[] = {   // the simple profile: select = trigger, menu = menu
      {s.trigger_action, "/user/hand/left/input/select/click"}, {s.trigger_action, "/user/hand/right/input/select/click"},
      {s.btn_action[State::BTN_MENU], "/user/hand/left/input/menu/click"}};
  const Bind kViveWmr[] = {   // trigger, grip (a click) and menu; the trackpad as the stick
      {s.trigger_action, "/user/hand/left/input/trigger/value"}, {s.trigger_action, "/user/hand/right/input/trigger/value"},
      {s.squeeze_action, "/user/hand/left/input/squeeze/click"}, {s.squeeze_action, "/user/hand/right/input/squeeze/click"},
      {s.btn_action[State::BTN_MENU], "/user/hand/left/input/menu/click"},
      {s.stick_action, "/user/hand/left/input/trackpad"}, {s.stick_action, "/user/hand/right/input/trackpad"}};
  struct Profile { const char* path; const Bind* extra; size_t count; };
  const Profile kProfiles[] = {
      {"/interaction_profiles/khr/simple_controller", kSimple, sizeof(kSimple) / sizeof(kSimple[0])},
      {"/interaction_profiles/oculus/touch_controller", kTouch, sizeof(kTouch) / sizeof(kTouch[0])},
      {"/interaction_profiles/valve/index_controller", kIndex, sizeof(kIndex) / sizeof(kIndex[0])},
      {"/interaction_profiles/htc/vive_controller", kViveWmr, sizeof(kViveWmr) / sizeof(kViveWmr[0])},
      {"/interaction_profiles/microsoft/motion_controller", kViveWmr, sizeof(kViveWmr) / sizeof(kViveWmr[0])},
  };
  int accepted = 0;
  for (const Profile& profile : kProfiles) {
    XrInteractionProfileSuggestedBinding ipb = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    if (XR_FAILED(s.xrStringToPath(s.instance, profile.path, &ipb.interactionProfile))) continue;
    std::vector<XrActionSuggestedBinding> sb;
    bool ok = true;
    auto add = [&](const Bind& b) {
      XrActionSuggestedBinding one;
      one.action = b.action;
      if (XR_FAILED(s.xrStringToPath(s.instance, b.path, &one.binding))) { ok = false; return; }
      sb.push_back(one);
    };
    for (const Bind& b : kPose) add(b);
    for (size_t i = 0; i < profile.count; ++i) add(profile.extra[i]);
    if (!ok) continue;
    ipb.countSuggestedBindings = static_cast<uint32_t>(sb.size());
    ipb.suggestedBindings = sb.data();
    const XrResult sr = s.xrSuggestInteractionProfileBindings(s.instance, &ipb);
    if (XR_SUCCEEDED(sr)) ++accepted;
    else o.Log("[XR] profile %s refused its bindings (%d)", profile.path, int(sr));
  }
  if (accepted == 0) return HandsOff(o, "no controller profile accepted", XR_SUCCESS);

  XrSessionActionSetsAttachInfo ati = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  ati.countActionSets = 1;
  ati.actionSets = &s.hand_set;
  if (XR_FAILED(r = s.xrAttachSessionActionSets(s.session, &ati)))
    return HandsOff(o, "xrAttachSessionActionSets", r);

  for (int h = 0; h < 2; ++h) {
    XrActionSpaceCreateInfo sci = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
    sci.poseInActionSpace.orientation.w = 1.0f;
    sci.subactionPath = s.hand_path[h];
    sci.action = s.grip_action;
    if (XR_FAILED(r = s.xrCreateActionSpace(s.session, &sci, &s.grip_space[h])))
      return HandsOff(o, "grip space", r);
    sci.action = s.aim_action;
    if (XR_FAILED(r = s.xrCreateActionSpace(s.session, &sci, &s.aim_space[h])))
      return HandsOff(o, "aim space", r);
  }
  s.hands_ready = true;
  o.Log("[XR] hands ready: %d of %d controller profiles accepted", accepted,
        int(sizeof(kProfiles) / sizeof(kProfiles[0])));
  return true;
}

bool Start(XrOutput& o, State& s, ID3D12Device* device, ID3D12CommandQueue* queue) {
  if (const char* e = std::getenv("DK_XR_LOOKAHEAD")) s.lookahead = std::atoi(e);
  HMODULE exe = GetModuleHandleW(nullptr);
  s.pair_sample = reinterpret_cast<int (*)(uint32_t, uint32_t*)>(
      reinterpret_cast<void*>(GetProcAddress(exe, "dk_xr_pair_sample")));
  s.eye_fov = reinterpret_cast<int (*)(float*)>(
      reinterpret_cast<void*>(GetProcAddress(exe, "dk_xr_eye_fov")));
  s.flat = reinterpret_cast<int (*)()>(reinterpret_cast<void*>(GetProcAddress(exe, "dk_xr_flat")));
  if (const char* e = std::getenv("DK_XR_FLAT_SCALE")) {
    const float v = float(std::atof(e));
    if (v > 0.2f && v <= 1.5f) s.flat_scale = v;
  }

  wchar_t path[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  while (n > 0 && path[n - 1] != L'\\') --n;
  path[n] = 0;
  wcsncat_s(path, L"openxr_loader.dll", _TRUNCATE);
  HMODULE loader = LoadLibraryW(path);
  if (!loader) loader = LoadLibraryW(L"openxr_loader.dll");
  if (!loader) return Fail(o, s, "openxr_loader.dll not found beside the exe", XR_SUCCESS);
  s.gipa = reinterpret_cast<PFN_xrGetInstanceProcAddr>(
      reinterpret_cast<void*>(GetProcAddress(loader, "xrGetInstanceProcAddr")));
  PFN_xrCreateInstance create_instance = nullptr;
  if (!s.gipa || XR_FAILED(s.gipa(XR_NULL_HANDLE, "xrCreateInstance",
                                  reinterpret_cast<PFN_xrVoidFunction*>(&create_instance)))) {
    return Fail(o, s, "the loader has no xrCreateInstance", XR_SUCCESS);
  }

  const char* exts[] = {XR_KHR_D3D12_ENABLE_EXTENSION_NAME};
  XrInstanceCreateInfo ici = {XR_TYPE_INSTANCE_CREATE_INFO};
  std::strcpy(ici.applicationInfo.applicationName, "ReXGlue DK_XR");
  std::strcpy(ici.applicationInfo.engineName, "ReXGlue");
  ici.applicationInfo.apiVersion = XR_API_VERSION_1_0;
  ici.enabledExtensionCount = 1;
  ici.enabledExtensionNames = exts;
  XrResult r = create_instance(&ici, &s.instance);
  if (XR_FAILED(r)) return Fail(o, s, "xrCreateInstance (no D3D12 OpenXR runtime?)", r);
#define DK_XR_LOAD(f)                                                                       \
  if (XR_FAILED(s.gipa(s.instance, #f, reinterpret_cast<PFN_xrVoidFunction*>(&s.f))))     \
    return Fail(o, s, "missing " #f, XR_ERROR_FUNCTION_UNSUPPORTED);
  DK_XR_FUNCS(DK_XR_LOAD)
#undef DK_XR_LOAD

  XrSystemGetInfo sgi = {XR_TYPE_SYSTEM_GET_INFO};
  sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  if (XR_FAILED(r = s.xrGetSystem(s.instance, &sgi, &s.system))) return Fail(o, s, "no headset", r);
  XrGraphicsRequirementsD3D12KHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
  if (XR_FAILED(r = s.xrGetD3D12GraphicsRequirementsKHR(s.instance, s.system, &req)))
    return Fail(o, s, "graphics requirements", r);
  const LUID luid = device->GetAdapterLuid();
  if (luid.LowPart != req.adapterLuid.LowPart || luid.HighPart != req.adapterLuid.HighPart)
    return Fail(o, s, "the headset is on a different graphics card than the game", XR_SUCCESS);

  XrGraphicsBindingD3D12KHR binding = {XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
  binding.device = device;
  binding.queue = queue;
  XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
  sci.next = &binding;
  sci.systemId = s.system;
  if (XR_FAILED(r = s.xrCreateSession(s.instance, &sci, &s.session)))
    return Fail(o, s, "xrCreateSession", r);
  XrReferenceSpaceCreateInfo rsi = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  rsi.poseInReferenceSpace.orientation.w = 1.0f;
  rsi.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  if (XR_FAILED(r = s.xrCreateReferenceSpace(s.session, &rsi, &s.local)))
    return Fail(o, s, "local space", r);
  rsi.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  if (XR_FAILED(r = s.xrCreateReferenceSpace(s.session, &rsi, &s.view)))
    return Fail(o, s, "view space", r);
  StartHands(o, s);  // patch 10 (draft): a failure here switches only the hands off

  uint32_t count = 0;
  s.xrEnumerateSwapchainFormats(s.session, 0, &count, nullptr);
  std::vector<int64_t> formats(count);
  s.xrEnumerateSwapchainFormats(s.session, count, &count, formats.data());
  const int64_t wanted[] = {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                            DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
                            DXGI_FORMAT_R10G10B10A2_UNORM};
  for (int64_t w : wanted) {
    for (int64_t f : formats) {
      if (f == w && s.format == 0) s.format = f;
    }
  }
  if (s.format == 0) return Fail(o, s, "the headset offers no 8-bit or 10-bit colour format", r);
  o.Log("[XR] runtime up: %u formats offered, using DXGI %lld; game fov export %s, pair export %s",
        count, (long long)s.format, s.eye_fov ? "yes" : "no", s.pair_sample ? "yes" : "no");
  return true;
}

void PollEvents(XrOutput& o, State& s) {
  XrEventDataBuffer ev = {XR_TYPE_EVENT_DATA_BUFFER};
  while (s.xrPollEvent(s.instance, &ev) == XR_SUCCESS) {
    if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
      const auto& sc = *reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
      o.Log("[XR] session state %d", int(sc.state));
      s.focused = (sc.state == XR_SESSION_STATE_FOCUSED);  // patch 10: when actions take input
      if (sc.state == XR_SESSION_STATE_READY) {
        XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
        bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        s.running = XR_SUCCEEDED(s.xrBeginSession(s.session, &bi));
      } else if (sc.state == XR_SESSION_STATE_STOPPING) {
        s.xrEndSession(s.session);
        s.running = false;
      } else if (sc.state == XR_SESSION_STATE_EXITING ||
                 sc.state == XR_SESSION_STATE_LOSS_PENDING) {
        s.running = false;
        s.exiting = true;
      }
    } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
      s.running = false;
      s.exiting = true;
    }
    ev = {XR_TYPE_EVENT_DATA_BUFFER};
  }
}

bool MakeChains(XrOutput& o, State& s, ID3D12Device* device, uint32_t w, uint32_t h) {
  for (Chain& c : s.chains) {
    if (c.handle != XR_NULL_HANDLE) s.xrDestroySwapchain(c.handle);
    c = Chain{};
  }
  for (Chain& c : s.chains) {
    XrSwapchainCreateInfo ci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    ci.format = s.format;
    ci.sampleCount = 1;
    ci.width = w;
    ci.height = h;
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1;
    XrResult r = s.xrCreateSwapchain(s.session, &ci, &c.handle);
    if (XR_FAILED(r)) return Fail(o, s, "xrCreateSwapchain", r);
    uint32_t n = 0;
    s.xrEnumerateSwapchainImages(c.handle, 0, &n, nullptr);
    c.images.assign(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
    s.xrEnumerateSwapchainImages(c.handle, n, &n,
                                 reinterpret_cast<XrSwapchainImageBaseHeader*>(c.images.data()));
  }
  s.chain_w = w;
  s.chain_h = h;
  if (!s.blit_ready) {
    // A typeless image takes a plain view, so the stored bytes are exactly the picture's (which is
    // already gamma-encoded). A typed sRGB image needs an sRGB view: undo the gamma first.
    const DXGI_FORMAT actual = s.chains[0].images[0].texture->GetDesc().Format;
    const bool typeless = IsTypeless(actual);
    const DXGI_FORMAT rtv = DXGI_FORMAT(typeless ? RtvTwin(s.format) : s.format);
    const bool decode = !typeless && IsSrgb(s.format);
    const char* err = "";
    if (!s.blit.Init(device, rtv, decode, &err)) return Fail(o, s, err, XR_SUCCESS);
    s.blit_ready = true;
    o.Log("[XR] eye images %ux%u, image format %d (%s), drawn as %d, gamma undone: %s", w, h,
          int(actual), typeless ? "typeless" : "typed", int(rtv), decode ? "yes" : "no");
    if (!typeless && !IsSrgb(s.format))
      o.Log("[XR] note: the headset treats this format as linear; the picture may look pale");
  }
  return true;
}

bool LocateViews(State& s, XrTime t, XrView out[2]) {
  XrViewLocateInfo li = {XR_TYPE_VIEW_LOCATE_INFO};
  li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  li.displayTime = t;
  li.space = s.local;
  XrViewState vs = {XR_TYPE_VIEW_STATE};
  uint32_t n = 0;
  out[0] = out[1] = {XR_TYPE_VIEW};
  return XR_SUCCEEDED(s.xrLocateViews(s.session, &li, &vs, 2, &n, out)) && n == 2 &&
         (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
}

}  // namespace

void XrOutput::SubmitPair(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Resource* left,
                          ID3D12Resource* right, uint32_t width, uint32_t height,
                          D3D12_RESOURCE_STATES state, uint32_t pair) {
  State& s = S();
  if (s.failed || s.exiting) return;
  if (!s.tried) {
    s.tried = true;
    if (!Start(*this, s, device, queue)) return;
  }
  PollEvents(*this, s);
  if (!s.running) return;
  if ((s.chain_w != width || s.chain_h != height) && !MakeChains(*this, s, device, width, height))
    return;

  XrFrameState fs = {XR_TYPE_FRAME_STATE};
  XrFrameWaitInfo wi = {XR_TYPE_FRAME_WAIT_INFO};
  const ULONGLONG t0 = GetTickCount64();
  if (XR_FAILED(s.xrWaitFrame(s.session, &wi, &fs))) return;
  s.wait_ms_sum += double(GetTickCount64() - t0);
  {
    std::lock_guard<std::mutex> lock(s.mutex);
    s.predicted = fs.predictedDisplayTime;
    s.period = fs.predictedDisplayPeriod;
  }
  XrFrameBeginInfo bi = {XR_TYPE_FRAME_BEGIN_INFO};
  if (XR_FAILED(s.xrBeginFrame(s.session, &bi))) return;

  XrCompositionLayerProjectionView views[2] = {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
                                               {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
  XrCompositionLayerProjection layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  const XrCompositionLayerBaseHeader* layers[] = {
      reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer)};
  bool drawn = false;

  if (fs.shouldRender) {
    // The eye poses the picture was drawn with: the game's sample for this pair, else now.
    XrView eye[2];
    bool have = false;
    uint32_t id = 0;
    if (pair != UINT32_MAX && s.pair_sample && s.pair_sample(pair, &id)) {
      std::lock_guard<std::mutex> lock(s.mutex);
      const Sample& smp = s.ring[id % kSampleRing];
      if (smp.id == id && id != 0) {
        eye[0] = smp.views[0];
        eye[1] = smp.views[1];
        have = true;
      }
    }
    ++(have ? s.sample_hits : s.sample_misses);
    if (!have) have = LocateViews(s, fs.predictedDisplayTime, eye);

    float tangents[4];
    const bool game_fov = s.eye_fov && s.eye_fov(tangents);
    const bool flat = have && s.flat && s.flat() == 1;
    if (flat && !s.flat_have) {
      for (int e = 0; e < 2; ++e) {
        s.flat_pose[e] = eye[e].pose;
        // level: the yaw of the head only (forward = q * (0, 0, -1))
        const XrQuaternionf& q = eye[e].pose.orientation;
        const float fx = -(2.0f * (q.x * q.z + q.w * q.y)), fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
        const float yaw = std::atan2(-fx, -fz);
        s.flat_pose[e].orientation = {0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f)};
      }
      // both eyes share the middle height and keep their own sideways place
      s.flat_have = true;
      Log("[XR] a flat picture began: held in the room at (%.2f %.2f %.2f), scale %.2f", s.flat_pose[0].position.x,
          s.flat_pose[0].position.y, s.flat_pose[0].position.z, s.flat_scale);
    } else if (!flat && s.flat_have) {
      s.flat_have = false;
      Log("[XR] the flat picture ended: back on the head");
    }
    ID3D12Resource* src[2] = {left, right};
    ID3D12Resource* dst[2];
    uint32_t index[2];
    bool acquired[2] = {false, false};
    bool ok = have;
    for (int e = 0; e < 2 && ok; ++e) {
      XrSwapchainImageAcquireInfo ai = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
      XrSwapchainImageWaitInfo swi = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
      swi.timeout = XR_INFINITE_DURATION;
      acquired[e] = XR_SUCCEEDED(s.xrAcquireSwapchainImage(s.chains[e].handle, &ai, &index[e]));
      ok = acquired[e] && XR_SUCCEEDED(s.xrWaitSwapchainImage(s.chains[e].handle, &swi));
      if (ok) dst[e] = s.chains[e].images[index[e]].texture;
    }
    const char* err = "";
    s.blit.SetGamma(g_xr_dial);
    s.blit.SetLightLog(XrEyeLightWanted());
    if (ok && !s.blit.Draw(queue, src, state, dst, width, height, &err)) {
      ++s.blit_errors;
      if (s.blit_errors <= 3) Log("[XR] draw failed: %s", err);
    }
    char light_line[256];
    if (s.blit.TakeLightReport(light_line, sizeof(light_line))) Log("%s", light_line);
    for (int e = 0; e < 2; ++e) {
      XrSwapchainImageReleaseInfo ri = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
      if (acquired[e]) s.xrReleaseSwapchainImage(s.chains[e].handle, &ri);
      views[e].pose = (flat && s.flat_have) ? s.flat_pose[e] : eye[e].pose;
      if (flat && s.flat_have && game_fov) {
        const float t[4] = {tangents[0] * s.flat_scale, tangents[1] * s.flat_scale, tangents[2] * s.flat_scale,
                            tangents[3] * s.flat_scale};
        views[e].fov = FovFromTangents(t);
      } else {
        views[e].fov = game_fov ? FovFromTangents(tangents) : eye[e].fov;
      }
      views[e].subImage.swapchain = s.chains[e].handle;
      views[e].subImage.imageRect = {{0, 0}, {int32_t(width), int32_t(height)}};
    }
    drawn = ok;
    layer.space = s.local;
    layer.viewCount = 2;
    layer.views = views;
  } else {
    ++s.not_rendered;
  }

  XrFrameEndInfo ei = {XR_TYPE_FRAME_END_INFO};
  ei.displayTime = fs.predictedDisplayTime;
  ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  ei.layerCount = drawn ? 1 : 0;
  ei.layers = layers;
  const XrResult r = s.xrEndFrame(s.session, &ei);
  if (XR_FAILED(r) && s.frames < 3) Log("[XR] xrEndFrame failed (XrResult %d)", int(r));

  if (++s.frames == 1 || (s.frames % kReportEvery) == 0) {
    Log("[XR] frames=%llu not_rendered=%llu sample_hits=%llu sample_misses=%llu "
        "draw_errors=%llu wait_ms_mean=%.2f",
        (unsigned long long)s.frames, (unsigned long long)s.not_rendered,
        (unsigned long long)s.sample_hits, (unsigned long long)s.sample_misses,
        (unsigned long long)s.blit_errors, s.wait_ms_sum / double(s.frames));
  }
}

}  // namespace rex::graphics::d3d12

extern "C" __declspec(dllexport) int rex_xr_sample_head(uint32_t* id_out, float orient_xyzw[4],
                                                        float pos_xyz[3]) {
  using rex::graphics::d3d12::Sample;
  auto& s = rex::graphics::d3d12::S();
  orient_xyzw[0] = orient_xyzw[1] = orient_xyzw[2] = 0.0f;
  orient_xyzw[3] = 1.0f;
  pos_xyz[0] = pos_xyz[1] = pos_xyz[2] = 0.0f;
  *id_out = 0;
  if (!s.running || s.predicted == 0) return 0;
  XrTime t;
  {
    std::lock_guard<std::mutex> lock(s.mutex);
    t = s.predicted + XrTime(s.lookahead) * s.period;
  }
  XrSpaceLocation head = {XR_TYPE_SPACE_LOCATION};
  if (XR_FAILED(s.xrLocateSpace(s.view, s.local, t, &head)) ||
      !(head.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
    return 0;
  }
  XrView eyes[2];
  if (!rex::graphics::d3d12::LocateViews(s, t, eyes)) return 0;
  const XrQuaternionf& q = head.pose.orientation;
  orient_xyzw[0] = q.x;
  orient_xyzw[1] = q.y;
  orient_xyzw[2] = q.z;
  orient_xyzw[3] = q.w;
  if (head.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
    pos_xyz[0] = head.pose.position.x;
    pos_xyz[1] = head.pose.position.y;
    pos_xyz[2] = head.pose.position.z;
  }
  std::lock_guard<std::mutex> lock(s.mutex);
  uint32_t id = ++s.next_id;
  if (id == 0) id = ++s.next_id;  // 0 means "no sample"
  Sample& slot = s.ring[id % rex::graphics::d3d12::kSampleRing];
  slot.id = id;
  slot.views[0] = eyes[0];
  slot.views[1] = eyes[1];
  *id_out = id;
  float tan4[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  for (const XrView& v : eyes) {
    tan4[0] = std::fmax(tan4[0], std::tan(-v.fov.angleLeft));
    tan4[1] = std::fmax(tan4[1], std::tan(v.fov.angleRight));
    tan4[2] = std::fmax(tan4[2], std::tan(v.fov.angleUp));
    tan4[3] = std::fmax(tan4[3], std::tan(-v.fov.angleDown));
  }
  const XrVector3f& a = eyes[0].pose.position;
  const XrVector3f& b = eyes[1].pose.position;
  const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
  const float eye_dist = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (!s.head_tan_ok) {
    rex::graphics::d3d12::XrOutput::Get().Log(
        "[XR] headset view (both eyes): tangents left %.4f right %.4f up %.4f down %.4f, eyes %.4f m "
        "apart",
        tan4[0], tan4[1], tan4[2], tan4[3], eye_dist);
  }
  std::memcpy(s.head_tan, tan4, sizeof(tan4));
  s.head_tan_ok = true;
  s.eye_dist_m = eye_dist;
  return 1;
}

// Patch 13: the brightness dial itself, so the game's bridge can show and turn it (xr.gamma).
extern "C" __declspec(dllexport) float* rex_xr_brightness_dial() {
  return &rex::graphics::d3d12::g_xr_dial;
}

// Patch 09b: how far apart the headset's two eyes are, in metres (the wearer's IPD setting).
// 0 until a head sample.
extern "C" __declspec(dllexport) int rex_xr_eye_distance(float* metres) {
  auto& s = rex::graphics::d3d12::S();
  std::lock_guard<std::mutex> lock(s.mutex);
  if (!s.head_tan_ok || s.eye_dist_m <= 0.0f) return 0;
  *metres = s.eye_dist_m;
  return 1;
}

// Patch 09b: the view the headset itself shows, so the game can draw exactly that much.
// Tangents left, right, up, down (all positive), the union of both eyes. 0 until a head sample.
extern "C" __declspec(dllexport) int rex_xr_headset_tangents(float t[4]) {
  auto& s = rex::graphics::d3d12::S();
  std::lock_guard<std::mutex> lock(s.mutex);
  if (!s.head_tan_ok) return 0;
  std::memcpy(t, s.head_tan, sizeof(s.head_tan));
  return 1;
}

// Patch 10 (draft): the controllers. See xr_output.h for the layout and the flags.
namespace {

void IdentityPose(float p[7]) {
  p[0] = p[1] = p[2] = 0.0f;
  p[3] = p[4] = p[5] = 0.0f;
  p[6] = 1.0f;
}

// Locates one action space in the local space at time t. Writes the pose and returns the
// location flags (0 when the locate failed or the action is not bound to a live controller).
XrSpaceLocationFlags LocateHand(rex::graphics::d3d12::State& s, XrAction action, XrPath hand,
                                XrSpace space, XrTime t, float out[7]) {
  IdentityPose(out);
  XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = action;
  gi.subactionPath = hand;
  XrActionStatePose st = {XR_TYPE_ACTION_STATE_POSE};
  if (XR_FAILED(s.xrGetActionStatePose(s.session, &gi, &st)) || !st.isActive) return 0;
  XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
  if (XR_FAILED(s.xrLocateSpace(space, s.local, t, &loc))) return 0;
  if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
    out[0] = loc.pose.position.x;
    out[1] = loc.pose.position.y;
    out[2] = loc.pose.position.z;
  }
  if (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
    out[3] = loc.pose.orientation.x;
    out[4] = loc.pose.orientation.y;
    out[5] = loc.pose.orientation.z;
    out[6] = loc.pose.orientation.w;
  }
  return loc.locationFlags;
}

}  // namespace

extern "C" __declspec(dllexport) int rex_xr_sample_hands(uint32_t* sample_id, float grip_left[7],
                                                         float grip_right[7], uint32_t* flags) {
  auto& s = rex::graphics::d3d12::S();
  IdentityPose(grip_left);
  IdentityPose(grip_right);
  *sample_id = 0;
  *flags = 0;
  if (!s.running || !s.hands_ready || s.predicted == 0) return 0;
  XrTime t;  // the same moment rex_xr_sample_head predicts for
  {
    std::lock_guard<std::mutex> lock(s.mutex);
    t = s.predicted + XrTime(s.lookahead) * s.period;
  }
  std::lock_guard<std::mutex> hand_lock(s.hand_mutex);
  ++s.hand_calls;
  // Once per call (the game calls this once per frame). Outside FOCUSED the runtime answers
  // XR_SESSION_NOT_FOCUSED and the poses go inactive, which the flags then show.
  uint32_t f = 0;
  if (s.focused) {
    XrActiveActionSet active = {s.hand_set, XR_NULL_PATH};
    XrActionsSyncInfo si = {XR_TYPE_ACTIONS_SYNC_INFO};
    si.countActiveActionSets = 1;
    si.activeActionSets = &active;
    if (s.xrSyncActions(s.session, &si) == XR_SUCCESS) f |= DK_XR_HAND_SYNCED;
  }
  if (!(f & DK_XR_HAND_SYNCED)) ++s.hand_unsynced;

  const XrSpaceLocationFlags kTracked =
      XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
  float* grip[2] = {grip_left, grip_right};
  float aim[2][7];
  for (int h = 0; h < 2; ++h) {
    const uint32_t shift = h ? 4u : 0u;
    const XrSpaceLocationFlags g =
        LocateHand(s, s.grip_action, s.hand_path[h], s.grip_space[h], t, grip[h]);
    if (g & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) f |= DK_XR_HAND_L_ORIENT << shift;
    if (g & XR_SPACE_LOCATION_POSITION_VALID_BIT) f |= DK_XR_HAND_L_POS << shift;
    if ((g & kTracked) == kTracked) f |= DK_XR_HAND_L_TRACKED << shift;
    const XrSpaceLocationFlags a =
        LocateHand(s, s.aim_action, s.hand_path[h], s.aim_space[h], t, aim[h]);
    const XrSpaceLocationFlags kValid =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    if ((a & kValid) == kValid) f |= h ? DK_XR_HAND_R_AIM : DK_XR_HAND_L_AIM;
  }
  if (f & DK_XR_HAND_L_ORIENT) ++s.hand_left_ok;
  if (f & DK_XR_HAND_R_ORIENT) ++s.hand_right_ok;

  uint32_t id = ++s.hand_next_id;
  if (id == 0) id = ++s.hand_next_id;  // 0 means "no sample"
  s.aim_id = id;
  std::memcpy(s.aim, aim, sizeof(aim));
  *sample_id = id;
  *flags = f;
  if (s.hand_calls == 1 || (s.hand_calls % 300) == 0) {
    rex::graphics::d3d12::XrOutput::Get().Log(
        "[XR] hands calls=%llu unsynced=%llu left_ok=%llu right_ok=%llu flags=0x%08X",
        (unsigned long long)s.hand_calls, (unsigned long long)s.hand_unsynced,
        (unsigned long long)s.hand_left_ok, (unsigned long long)s.hand_right_ok, f);
  }
  return (f & (DK_XR_HAND_L_ORIENT | DK_XR_HAND_R_ORIENT)) ? 1 : 0;
}

// Patch 11: the pad. Synced here as well (the game's pad driver polls on its own thread, and the
// hands may be switched off); xrSyncActions more than once a frame is allowed and cheap.
extern "C" __declspec(dllexport) int rex_xr_sample_buttons(uint32_t* buttons, float axes[6]) {
  auto& s = rex::graphics::d3d12::S();
  *buttons = 0;
  for (int i = 0; i < 6; ++i) axes[i] = 0.0f;
  if (!s.running || !s.hands_ready || !s.focused) return 0;
  std::lock_guard<std::mutex> hand_lock(s.hand_mutex);
  ++s.button_calls;
  XrActiveActionSet active = {s.hand_set, XR_NULL_PATH};
  XrActionsSyncInfo si = {XR_TYPE_ACTIONS_SYNC_INFO};
  si.countActiveActionSets = 1;
  si.activeActionSets = &active;
  if (s.xrSyncActions(s.session, &si) != XR_SUCCESS) return 0;
  // XInput bits (rex/input/input.h): START 0x10, LEFT_THUMB 0x40, RIGHT_THUMB 0x80, LB 0x100, RB 0x200,
  // A 0x1000, B 0x2000, X 0x4000, Y 0x8000
  static const uint32_t kBits[rex::graphics::d3d12::State::BTN_COUNT] = {0x1000, 0x2000, 0x4000, 0x8000, 0x0010, 0x0040, 0x0080};
  uint32_t b = 0;
  for (int i = 0; i < rex::graphics::d3d12::State::BTN_COUNT; ++i) {
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = s.btn_action[i];
    XrActionStateBoolean st = {XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_SUCCEEDED(s.xrGetActionStateBoolean(s.session, &gi, &st)) && st.isActive && st.currentState) b |= kBits[i];
  }
  for (int h = 0; h < 2; ++h) {
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.subactionPath = s.hand_path[h];
    gi.action = s.trigger_action;
    XrActionStateFloat fl = {XR_TYPE_ACTION_STATE_FLOAT};
    if (XR_SUCCEEDED(s.xrGetActionStateFloat(s.session, &gi, &fl)) && fl.isActive) axes[h] = fl.currentState;
    gi.action = s.squeeze_action;
    fl = {XR_TYPE_ACTION_STATE_FLOAT};
    if (XR_SUCCEEDED(s.xrGetActionStateFloat(s.session, &gi, &fl)) && fl.isActive && fl.currentState > 0.5f)
      b |= h ? 0x0200u : 0x0100u;
    gi.action = s.stick_action;
    XrActionStateVector2f v2 = {XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_SUCCEEDED(s.xrGetActionStateVector2f(s.session, &gi, &v2)) && v2.isActive) {
      axes[2 + 2 * h] = v2.currentState.x;
      axes[3 + 2 * h] = v2.currentState.y;
    }
  }
  *buttons = b;
  if (s.button_calls == 1 || (s.button_calls % 600) == 0)
    rex::graphics::d3d12::XrOutput::Get().Log("[XR] pad calls=%llu buttons=0x%04X lt=%.2f rt=%.2f",
                                              (unsigned long long)s.button_calls, b, axes[0], axes[1]);
  return 1;
}

extern "C" __declspec(dllexport) int rex_xr_hands_aim(uint32_t sample_id, float aim_left[7],
                                                      float aim_right[7]) {
  auto& s = rex::graphics::d3d12::S();
  IdentityPose(aim_left);
  IdentityPose(aim_right);
  std::lock_guard<std::mutex> hand_lock(s.hand_mutex);
  if (sample_id == 0 || sample_id != s.aim_id) return 0;
  std::memcpy(aim_left, s.aim[0], sizeof(s.aim[0]));
  std::memcpy(aim_right, s.aim[1], sizeof(s.aim[1]));
  return 1;
}
