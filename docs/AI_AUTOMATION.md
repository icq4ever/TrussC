# AI Automation & MCP Integration

## What is this?

TrussC applications natively support the **Model Context Protocol (MCP)**.
This allows AI agents (like Claude, Gemini, or IDE assistants) to directly connect to, inspect, and control your running application via standard JSON-RPC messages over HTTP.

By enabling MCP mode, your app becomes a "tool" for AI, enabling:

- **Autonomous Debugging:** AI can read logs, check app state, and fix bugs.
- **Automated Testing:** AI can simulate user input (mouse/keyboard) and verify results via screenshots.
- **Game Agents:** AI can play games against humans by reading the game state directly.

## Enabling MCP Mode

To start your app in MCP mode, set the `TRUSSC_MCP` environment variable to `1`.

```bash
# Auto-assign port (printed to stderr on startup)
TRUSSC_MCP=1 ./myApp

# Or specify a port
TRUSSC_MCP=1 TRUSSC_MCP_PORT=8080 ./myApp
```

When enabled:
1. An **HTTP server** starts on the specified port (or an OS-assigned port).
2. **Inspection tools** (`get_screenshot`, `save_screenshot`) are automatically registered.
3. The server endpoint URL is printed to stderr: `[MCP] HTTP server listening on http://localhost:PORT/mcp`

## Transport

TrussC uses **HTTP transport** for MCP. All JSON-RPC messages are sent as HTTP POST requests to the `/mcp` endpoint.

| Method | Path | Description |
|--------|------|-------------|
| POST | `/mcp` | JSON-RPC request → response |
| OPTIONS | `/mcp` | CORS preflight (204) |
| GET | `/` | Server info (for port discovery) |

## Standard MCP Tools

### Inspection Tools (always available in MCP mode)
| Tool | Arguments | Description |
|------|-----------|-------------|
| `get_screenshot` | (none) | Get current screen as Base64 PNG image |
| `save_screenshot` | `path` | Save screenshot to file |
| `quit` | (none) | Quit the application gracefully |

### Debugger Tools (opt-in via `mcp::enableDebugger()`)
| Tool | Arguments | Description |
|------|-----------|-------------|
| `mouse_move` | `x`, `y`, `button` | Move mouse cursor (and optionally drag) |
| `mouse_click` | `x`, `y`, `button` | Click mouse button (0:left, 1:right) |
| `mouse_scroll` | `dx`, `dy` | Scroll mouse wheel |
| `key_press` | `key` | Press a key (sokol_app keycode) |
| `key_release` | `key` | Release a key |

To enable debugger tools, call `mcp::enableDebugger()` in your `setup()`:

```cpp
void tcApp::setup() {
    imguiSetup();              // Initialize ImGui first
    mcp::enableDebugger();
    mcp::registerDebuggerTools();  // Also registers ImGui tools if ImGui is enabled
}
```

### ImGui Tools (auto-registered when ImGui is enabled)

When `imguiSetup()` is called before `mcp::registerDebuggerTools()`, ImGui-specific tools are automatically registered. These allow AI agents to inspect and interact with ImGui UI without writing custom tools.

| Tool | Arguments | Description |
|------|-----------|-------------|
| `imgui_get_widgets` | `window` (optional) | List all ImGui widgets with labels, types, and positions |
| `imgui_click` | `label`, `window` (optional) | Click a widget by its label text |
| `imgui_input` | `label`, `text`, `window` (optional) | Type text into an input widget |
| `imgui_checkbox` | `label`, `value` (optional), `window` (optional) | Toggle or set a checkbox |

**How it works:**
- Uses ImGui's Test Engine hooks (`IMGUI_ENABLE_TEST_ENGINE`) to collect widget info each frame
- Widget data is double-buffered: current frame writes, MCP reads from last frame
- Input injection uses `ImGuiIO` public API (`AddMousePosEvent`, `AddMouseButtonEvent`, `AddInputCharactersUTF8`)
- Zero cost when debugger is not enabled

**Window resolution:**
- If `window` is omitted, the label must be unique across all windows
- If the label exists in multiple windows, an error is returned with candidate window names

**Example: Interacting with ImGui UI via curl**

```bash
# List all widgets
curl -s -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":1,
       "params":{"name":"imgui_get_widgets","arguments":{}}}'

# Type into an input field
curl -s -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":2,
       "params":{"name":"imgui_input","arguments":{"label":"##input","text":"Hello"}}}'

# Click a button
curl -s -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":3,
       "params":{"name":"imgui_click","arguments":{"label":"Add"}}}'
```

## Creating Custom Tools

You can easily expose your own application logic to AI using the `mcp::tool` builder in `setup()`.

```cpp
#include <TrussC.h>

void tcApp::setup() {
    // Expose a function as an MCP tool
    mcp::tool("place_stone", "Place a stone on the board")
        .arg<int>("x", "X coordinate (0-7)")
        .arg<int>("y", "Y coordinate (0-7)")
        .bind([this](int x, int y) {
            bool success = board.place(x, y);
            return json{
                {"status", success ? "ok" : "error"},
                {"turn", (int)board.currentTurn}
            };
        });

    // Expose state as an MCP resource
    mcp::resource("app://board", "Current Board State")
        .mime("application/json")
        .bind([this]() {
            return board.toJSON();
        });
}
```

## Protocol Details

TrussC implements a subset of the **MCP (Model Context Protocol)** specification over HTTP.

### Request (AI -> App)
```bash
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/call",
    "params": {
        "name": "place_stone",
        "arguments": { "x": 3, "y": 4 }
    }
  }'
```

### Response (App -> AI)
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "content": [{ "type": "text", "text": "{\"status\":\"ok\"}" }]
  }
}
```

## Testing with curl

```bash
# Start app in MCP mode
TRUSSC_MCP=1 TRUSSC_MCP_PORT=8080 ./bin/MyApp.app/Contents/MacOS/MyApp &

# Initialize
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"initialize","id":1,"params":{}}'

# Take screenshot
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":2,"params":{"name":"save_screenshot","arguments":{"path":"/tmp/test.png"}}}'

# Mouse click (requires enableDebugger())
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":3,"params":{"name":"mouse_click","arguments":{"x":100,"y":200}}}'
```

### Taking Screenshots from Shell

**IMPORTANT for AI agents**: Do NOT use macOS `screencapture` or similar OS commands. TrussC apps may render with Metal/OpenGL and the OS cannot capture the screen correctly. Always use the MCP `save_screenshot` tool.

```bash
# Start app, wait, take screenshot, then kill
TRUSSC_MCP=1 TRUSSC_MCP_PORT=8080 ./bin/myApp.app/Contents/MacOS/myApp &
sleep 2
curl -s -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"initialize","id":1,"params":{}}'
curl -s -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":2,"params":{"name":"save_screenshot","arguments":{"path":"/tmp/screenshot.png"}}}'
kill %1
# Screenshot is now at /tmp/screenshot.png
```

## Connecting with AI Agents

### Via MCP Clients (Claude Desktop, etc.)

Configure your MCP client with the HTTP URL:

```json
{
  "mcpServers": {
    "trussc-app": {
      "url": "http://localhost:8080/mcp"
    }
  }
}
```

### Port Discovery

- If `TRUSSC_MCP_PORT` is set, the app uses that port.
- If not set (or set to `0`), the OS assigns an available port.
- The actual port is printed to stderr on startup: `[MCP] HTTP server listening on http://localhost:PORT/mcp`
- From code: `mcp::getHttpPort()` returns the actual port number.

## Security Model

| Category | Tools | Enabled by |
|----------|-------|------------|
| Inspection (read-only) | `get_screenshot`, `save_screenshot` | Automatic when MCP is enabled |
| Debugger (input injection) | `mouse_click`, `key_press`, `mouse_move`, `mouse_scroll`, `key_release` | `mcp::enableDebugger()` + `mcp::registerDebuggerTools()` |
| ImGui (widget interaction) | `imgui_get_widgets`, `imgui_click`, `imgui_input`, `imgui_checkbox` | Auto-registered when ImGui + debugger are both enabled |
| Custom | `mcp::tool(...)` | Your code |

MCP communicates over localhost only. For remote access, use SSH tunneling or similar.
