// The Darkness VR: the link to the Hand Poser window (DRAFT 2026-11-15, plan step 2). See vr_hand_poser_link.cpp.
#pragma once

#include <cstdint>

struct PPCContext;

namespace dkposer {

// Registers the poser.* bridge variables and starts the file thread (state.json out, command.json in); safe to call
// more than once. Does nothing further while poser.on = 0 (DK_POSER=1 switches it on at launch).
void Start();

// Call from the sub_821BF988 hook AFTER the original has run (so after the explicit-target IK solve), with the
// skeleton (r4) and skeleton instance (r5) saved at the hook's ENTRY, and only for the player's skeleton instance.
// Writes every held finger / hand joint's local turn into *(SI+32) and re-composes those bones (and everything below
// them) with the game's own composer sub_82420678. ctx's volatile registers (r3, the return value, included) are put
// back before it returns. No-op when nothing is held and nothing is asked for.
void AfterIk(PPCContext& ctx, uint8_t* base, uint32_t skel, uint32_t si);

// The held-gun number (vr_hand_pose.cpp's pose.held): reported to the window as "gun <n>" ("" for 0 = empty hands),
// so Hand Poser keeps one pose per gun by itself.
void NoteGun(uint32_t value);

}  // namespace dkposer
