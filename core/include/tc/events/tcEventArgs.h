#pragma once

// =============================================================================
// tcEventArgs - Event argument structures
// =============================================================================

#include <vector>
#include <string>
#include "tcMath.h"   // Vec2

namespace trussc {

// ---------------------------------------------------------------------------
// Mouse button identifier (values match SAPP_MOUSEBUTTON_*)
// ---------------------------------------------------------------------------
enum class MouseButton {
    Left   = 0,      // SAPP_MOUSEBUTTON_LEFT
    Right  = 1,      // SAPP_MOUSEBUTTON_RIGHT
    Middle = 2,      // SAPP_MOUSEBUTTON_MIDDLE
    None   = 0x100,  // SAPP_MOUSEBUTTON_INVALID (no button; e.g. during a plain move)
};

// ---------------------------------------------------------------------------
// Key event arguments
// ---------------------------------------------------------------------------
struct KeyEventArgs {
    int key = 0;              // Key code (SAPP_KEYCODE_*)
    bool isRepeat = false;    // Whether this is a repeat input
    bool shift = false;       // Shift key
    bool ctrl = false;        // Ctrl key
    bool alt = false;         // Alt key
    bool super = false;       // Super/Command key
};

// ---------------------------------------------------------------------------
// Mouse event arguments (pressed / released / moved / dragged)
// ---------------------------------------------------------------------------
// Coordinate convention:
//   - `pos`       : position in the receiving node's LOCAL space.
//   - `globalPos` : position in SCREEN space.
//   When handled at app level (events().mouseXxx or App::mouseXxx) there is no
//   node transform, so `pos == globalPos`. Inside a Node's onMouseXxx the two
//   differ by that node's transform.
//   - `delta`/`globalDelta` follow the same local/screen split (movement since
//   the previous event). `button` is None during a plain move.
struct MouseEventArgs {
    Vec2 pos;                 // Local position (== globalPos at app level)
    Vec2 globalPos;           // Screen position
    Vec2 delta;               // Movement since last event, local space
    Vec2 globalDelta;         // Movement since last event, screen space
    MouseButton button = MouseButton::None;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool super = false;
};

// ---------------------------------------------------------------------------
// Mouse scroll event arguments
// ---------------------------------------------------------------------------
struct ScrollEventArgs {
    Vec2 pos;                 // Local position of the cursor (== globalPos at app level)
    Vec2 globalPos;           // Screen position of the cursor
    Vec2 scroll;              // Scroll amount (x: horizontal, y: vertical)
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool super = false;
};

// ---------------------------------------------------------------------------
// Window resize event arguments
// ---------------------------------------------------------------------------
struct ResizeEventArgs {
    int width = 0;
    int height = 0;
};

// ---------------------------------------------------------------------------
// Drag & drop event arguments (for future use)
// ---------------------------------------------------------------------------
struct DragDropEventArgs {
    std::vector<std::string> files;  // Dropped file paths
    float x = 0.0f;
    float y = 0.0f;
};

// ---------------------------------------------------------------------------
// Clipboard paste event arguments
// ---------------------------------------------------------------------------
// Fired on the platform paste gesture (macOS: Cmd+V, others: Ctrl+V, Web:
// browser 'paste' event). `text` is the pasted clipboard content, already
// read for you — no need to call getClipboardString() yourself. This is the
// only reliable way to read the clipboard on the Web platform, where arbitrary
// getClipboardString() calls are blocked by the browser.
struct ClipboardPastedEventArgs {
    std::string text;  // Pasted clipboard content
};

// ---------------------------------------------------------------------------
// Touch point (single finger)
// ---------------------------------------------------------------------------
struct TouchPoint {
    int id = 0;               // Touch ID (persistent across move)
    float x = 0.0f;
    float y = 0.0f;
    float pressure = 1.0f;    // Touch pressure (0.0-1.0, default 1.0; not yet reported by sokol)
    bool changed = false;     // Whether this touch was part of the current action
};

// ---------------------------------------------------------------------------
// Touch event arguments (multi-touch)
// ---------------------------------------------------------------------------
struct TouchEventArgs {
    static constexpr int MAX_TOUCHES = 8;  // Matches SAPP_MAX_TOUCHPOINTS
    TouchPoint touches[MAX_TOUCHES];
    int numTouches = 0;
    bool cancelled = false;   // true when touchReleased is due to system cancellation
                              // (e.g. incoming call, system gesture). Released is still
                              // fired — check this flag for cancellation-specific handling.

    // Convenience: first touch
    float x() const { return numTouches > 0 ? touches[0].x : 0.0f; }
    float y() const { return numTouches > 0 ? touches[0].y : 0.0f; }
    int id() const { return numTouches > 0 ? touches[0].id : 0; }
};

// ---------------------------------------------------------------------------
// Console input event arguments (commands from stdin)
// ---------------------------------------------------------------------------
struct ConsoleEventArgs {
    std::string raw;               // Raw input line (e.g., "tcdebug screenshot /tmp/a.png")
    std::vector<std::string> args; // Parsed by whitespace (e.g., ["tcdebug", "screenshot", "/tmp/a.png"])
};

// ---------------------------------------------------------------------------
// Exit request event arguments
// ---------------------------------------------------------------------------
struct ExitRequestEventArgs {
    bool cancel = false;           // Set to true to cancel the exit
};

} // namespace trussc
