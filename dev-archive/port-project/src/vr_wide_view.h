// The headset's full view: see vr_wide_view.cpp.
#pragma once
#include <cstdint>
namespace dkview {
// the half-width tangents the widened world view was built with; false while the view is the game's own
bool Tangents(float* tan_x, float* tan_y);
// a level camera was just built (sub_823F9B00): the view is widened only while these keep coming
void NoteCamera();
// true while level cameras are being built (not in the menus or films)
bool InLevelNow();
// shrink a flat (orthographic) projection just written at this guest address; nothing for perspective ones
void Flat(uint8_t* base, uint32_t projection);
void FlatAtCommit(uint8_t* base, uint32_t render_context);
}  // namespace dkview
