// The Darkness VR: the headset turns the game camera (DK_XR_HEAD=1), 2026-10-25. See vr_xr_head.cpp.
#pragma once

namespace dkxr {

// True when DK_XR=1 (SDK patch 09 sends each pair to the headset) and DK_XR_HEAD=1.
bool HeadOn();

// Turns the camera's rows (forward, right, up; 3 floats each, as sub_823F9B00 writes them) by the
// head's rotation, in the camera's own frame. first_eye = the left eye: the head is sampled there and
// the same sample is used for the right eye, so both eyes of a pair look the same way.
void TurnCamera(float fwd[3], float right[3], float up[3], bool first_eye);

}  // namespace dkxr
