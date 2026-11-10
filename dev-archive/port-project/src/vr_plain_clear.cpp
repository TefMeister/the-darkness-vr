// vr_plain_clear.cpp -- the console graphics library's fast clear, done plainly (2026-11-10, b025).
//
// sub_828621D0 clears a rectangle of the render surface. For speed on the console it clears the 80 x 16
// aligned middle of the rectangle as a 4-sample surface of half the pitch (the same video memory seen
// another way) and only the edge strips with the surface as it is. On the PC port that second way of
// seeing the memory is a new surface every time: under the port's "rtv" drawing method each such clear
// makes it redraw the whole depth surface about nine times in and nine times out (GPU trace 2026-11-10,
// modding-notes/2026-11-10h), and under "rov" it is three extra layout changes an eye.
//
// The routine already has a plain branch (loc_828623D8: the aligned middle is empty -> one clear of the
// whole rectangle with the surface untouched, through sub_82861EE0). This hook always does what that
// branch does. The same video memory is cleared.
//
// DK_VR_PLAIN_CLEAR=0 keeps the game's own routine. Read once.
//
// Arguments of sub_828621D0 (read from the generated code): r3 device, r4 x1, r5 y1, r6 x2, r7 y2,
// r8 colour pointer, r10 surface-info word, f1 depth, and three words on the caller's stack at
// sp+84, sp+92, sp+108.

#include "generated/default/darknessrecomp_pch.h"

#include <rex/hook.h>

#include <cstdlib>

namespace {
constexpr uint32_t kFrame = 288;        // the routine's own stack frame
constexpr uint32_t kRect = 96;          // where it builds the rectangle in that frame
constexpr uint32_t kCallerA = 84;       // the three caller-stack words it passes on
constexpr uint32_t kCallerB = 92;
constexpr uint32_t kCallerC = 108;
constexpr uint32_t kPassC = 84;         // where the third goes in the new frame

bool PlainClearOn() {
  static const bool on = [] {
    const char* e = std::getenv("DK_VR_PLAIN_CLEAR");
    return !(e && e[0] == '0');
  }();
  return on;
}
}  // namespace

DECLARE_REX_FUNC(sub_828621D0);
DECLARE_REX_FUNC(sub_82861EE0);

REX_HOOK_RAW(sub_828621D0) {
  if (!PlainClearOn()) {
    __imp__sub_828621D0(ctx, base);
    return;
  }
  const uint32_t sp = ctx.r1.u32;
  const uint32_t frame = sp - kFrame;
  REX_STORE_U32(frame, sp);                         // back chain, as stwu would
  REX_STORE_U32(frame + kRect + 0, ctx.r4.u32);     // x1
  REX_STORE_U32(frame + kRect + 4, ctx.r5.u32);     // y1
  REX_STORE_U32(frame + kRect + 8, ctx.r6.u32);     // x2
  REX_STORE_U32(frame + kRect + 12, ctx.r7.u32);    // y2
  REX_STORE_U32(frame + kPassC, REX_LOAD_U32(sp + kCallerC));
  const uint64_t colour = ctx.r8.u64, surface = ctx.r10.u64;
  ctx.r9.u64 = REX_LOAD_U32(sp + kCallerA);
  ctx.r10.u64 = REX_LOAD_U32(sp + kCallerB);
  ctx.r8.u64 = surface;
  ctx.r6.u64 = colour;
  ctx.r5.u64 = frame + kRect;
  ctx.r4.u64 = 1;                                   // one rectangle
  ctx.r1.u64 = frame;                               // r3 (device) and f1 (depth) pass through
  sub_82861EE0(ctx, base);
  ctx.r1.u64 = sp;
}
