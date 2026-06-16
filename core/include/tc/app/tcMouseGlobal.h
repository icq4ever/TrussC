#pragma once

// =============================================================================
// tcMouseGlobal.h - global (window-space) mouse state + getters
// =============================================================================
// The raw mouse position state and its window-space getters. Extracted from
// TrussC.h so lower-level headers (e.g. tcNode.h, which converts global mouse
// coords to node-local) can depend on them directly, in dependency order,
// instead of pulling the whole umbrella. Apps include <TrussC.h>; the framework
// event loop writes internal::mouseX/Y each frame. This is an internal piece.
// =============================================================================

#include "tcMath.h"   // Vec2

namespace trussc {

namespace internal {
    // Mouse state (window coordinates), written by the framework event loop.
    inline float mouseX = 0.0f;
    inline float mouseY = 0.0f;
    inline float pmouseX = 0.0f;  // Previous frame mouse position
    inline float pmouseY = 0.0f;
}

// ---------------------------------------------------------------------------
// Mouse state (global / window coordinates)
// ---------------------------------------------------------------------------

// Current mouse X coordinate (window coordinates)
inline float getGlobalMouseX() {
    return internal::mouseX;
}

// Current mouse Y coordinate (window coordinates)
inline float getGlobalMouseY() {
    return internal::mouseY;
}

// Previous frame mouse X coordinate (window coordinates)
inline float getGlobalPMouseX() {
    return internal::pmouseX;
}

// Previous frame mouse Y coordinate (window coordinates)
inline float getGlobalPMouseY() {
    return internal::pmouseY;
}

// Alias for getGlobalMouseX/Y (for tcDebugInput)
inline float getMouseX() { return internal::mouseX; }
inline float getMouseY() { return internal::mouseY; }
inline Vec2 getMousePos() { return Vec2(internal::mouseX, internal::mouseY); }
inline Vec2 getGlobalMousePos() { return Vec2(getGlobalMouseX(), getGlobalMouseY()); }

} // namespace trussc
