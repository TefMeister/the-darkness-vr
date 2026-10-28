// MCP Game Bridge³ in The Darkness: see vr_bridge.cpp.
#pragma once

#include <memory>

#include <rex/system/interfaces/input.h>

namespace rex {
class Runtime;
}

// The SDK's default input system, plus the bridge's virtual pad (unless DK_BRIDGE=0).
std::unique_ptr<rex::system::IInputSystem> DkCreateInputSystem(bool tool_mode);
// Mirrors the game's log into the bridge (call once logging is up).
void DkBridgeLogging();
// Opens the pipe; call once the runtime exists.
void DkBridgeStart(rex::Runtime* runtime);
void DkBridgeStop();
