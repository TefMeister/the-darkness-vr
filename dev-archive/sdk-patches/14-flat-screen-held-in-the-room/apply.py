import os
sdk = r"D:\the-darkness-build\rexglue-sdk\src\graphics\d3d12\xr_output.cpp"
s = open(sdk, encoding="utf-8").read()
if "flat_pose" not in s:
    # state
    a = "  int (*eye_fov)(float tangents[4]) = nullptr;"
    assert a in s
    s = s.replace(a, a + '''
  // Patch 14 (2026-11-10): the flat pictures (menus, films) as a screen that stays put in the room. The game program's
  // dk_xr_flat() says when the picture is a flat one; the layer is then submitted with the eye poses of the moment it
  // began (turned level, yaw only) instead of the head's newest, so the runtime keeps it where it was while the head
  // turns, and with its tangents scaled by DK_XR_FLAT_SCALE (default 0.75) so it is smaller.
  int (*flat)() = nullptr;
  bool flat_have = false;
  XrPosef flat_pose[2] = {};
  float flat_scale = 0.75f;''', 1)
    # lookup
    a = '''  s.eye_fov = reinterpret_cast<int (*)(float*)>(
      reinterpret_cast<void*>(GetProcAddress(exe, "dk_xr_eye_fov")));'''
    assert a in s
    s = s.replace(a, a + '''
  s.flat = reinterpret_cast<int (*)()>(reinterpret_cast<void*>(GetProcAddress(exe, "dk_xr_flat")));
  if (const char* e = std::getenv("DK_XR_FLAT_SCALE")) {
    const float v = float(std::atof(e));
    if (v > 0.2f && v <= 1.5f) s.flat_scale = v;
  }''', 1)
    # use
    a = '''    float tangents[4];
    const bool game_fov = s.eye_fov && s.eye_fov(tangents);'''
    assert a in s
    s = s.replace(a, a + '''
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
    }''', 1)
    a = '''      views[e].pose = eye[e].pose;
      views[e].fov = game_fov ? FovFromTangents(tangents) : eye[e].fov;'''
    assert a in s
    s = s.replace(a, '''      views[e].pose = (flat && s.flat_have) ? s.flat_pose[e] : eye[e].pose;
      if (flat && s.flat_have && game_fov) {
        const float t[4] = {tangents[0] * s.flat_scale, tangents[1] * s.flat_scale, tangents[2] * s.flat_scale,
                            tangents[3] * s.flat_scale};
        views[e].fov = FovFromTangents(t);
      } else {
        views[e].fov = game_fov ? FovFromTangents(tangents) : eye[e].fov;
      }''', 1)
    open(sdk, "w", encoding="utf-8").write(s)
    print("patched xr_output.cpp")

src = r"D:\the-darkness-build\src\DarknessRecomp\src\vr_wide_view.cpp"
s = open(src, encoding="utf-8").read()
if "dk_xr_flat" not in s:
    s = s.replace("int32_t g_level_only =", "// view.flat_world (DK_XR_FLAT_WORLD, default 1): out of a level the picture (menus, films) is a screen held in the room,\n// smaller, instead of a picture stuck to the face (SDK patch 14 asks dk_xr_flat())\nint32_t g_flat_world = std::getenv(\"DK_XR_FLAT_WORLD\") ? int32_t(EnvF(\"DK_XR_FLAT_WORLD\")) : 1;\nint32_t g_level_only =", 1)
    s = s.replace('  bridge_var("view.level_only",', '  bridge_var("view.flat_world", BRIDGE_I32, &g_flat_world, 1, "1 = out of a level the picture is a screen held in the room (menus, films)");\n  bridge_var("view.level_only",', 1)
    s += '''
// Asked by SDK patch 14 at every frame handed to the headset: 1 = this picture is a flat one (a menu, a film).
extern "C" __declspec(dllexport) int dk_xr_flat() {
  if (g_flat_world != 1) return 0;
  return InLevel() ? 0 : 1;
}
'''
    open(src, "w", encoding="utf-8").write(s)
    print("patched vr_wide_view.cpp")
