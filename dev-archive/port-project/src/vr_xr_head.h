// The Darkness VR: the headset turns the game camera (DK_XR_HEAD=1), 2026-10-25. See vr_xr_head.cpp.
#pragma once

#include <cstdint>

namespace dkxr {

// True when DK_XR=1 (SDK patch 09 sends each pair to the headset) and DK_XR_HEAD=1.
bool HeadOn();

// No pair number known for this camera build (a filler packet, or eye by packet off).
constexpr uint32_t kNoPair = 0xFFFFFFFFu;

// Turns the camera's rows (forward, right, up; 3 floats each) by the head's rotation, in the camera's
// own frame. With a pair number (the SDK's: game sequence / (2 * DK_STEREO_PERIOD)) the head is
// sampled once per pair and both eyes use that sample; dk_xr_pair_sample then tells the SDK which
// sample the pair was drawn with (2026-10-28). Without one: sampled on first_eye, as before.
void TurnCamera(float fwd[3], float right[3], float up[3], bool first_eye, uint32_t pair);

}  // namespace dkxr
