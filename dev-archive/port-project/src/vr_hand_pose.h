// The Darkness VR: the hand poser (2026-10-29). See vr_hand_pose.cpp.
#pragma once

#include <cstdint>

namespace dkpose {

// Registers the pose.* bridge variables and loads the saved pose file; safe to call more than once.
void Start();

// Called by the controller pad on every pad read with the controllers' raw bits (SDK patch 11) and axes
// (lt rt lx ly rx ry). True while the poser has the controllers: the game is then given none of their input.
bool FeedPad(uint32_t raw_buttons, const float axes[6]);

// For the solver hook, per hand (0 left, 1 right): h = the animation's hand rows, c = the controller's axes in
// the world (rows X right, Y up, F pointing). Writes the hand rows to use and returns true, or returns false to
// leave the animation's own rows (no pose saved for this hand yet, or the poser is waiting for its line-up).
bool HandRows(int hand, const float h[3][3], const float c[3][3], float out[3][3]);

// Line a hand up at its next solve: bit 0 left, bit 1 right (ik.calib=1 asks for both).
void RequestLineUp(uint32_t hands);

}  // namespace dkpose
