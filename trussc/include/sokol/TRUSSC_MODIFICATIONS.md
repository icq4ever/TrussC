# TrussC Modifications to Sokol Headers

This file documents all modifications made to sokol headers for TrussC.
When updating from upstream, these changes need to be re-applied.

Search for `tettou771` or `Modified by` or `[TrussC` to find all modified sections.

**Upstream base:** https://github.com/floooh/sokol commit `4838409` (2026-02-23)

---

## Directory Structure

```
sokol/
├── sokol_app.h          # Modified (3 patches)
├── sokol_gfx.h          # Untouched (direct copy from upstream)
├── sokol_glue.h         # Modified (1 patch)
├── sokol_log.h          # Untouched
├── sokol_time.h         # Untouched
├── sokol_fetch.h        # Untouched
├── sokol_audio.h        # Untouched
├── sokol_debugtext.h    # Untouched
├── TRUSSC_MODIFICATIONS.md
└── util/
    ├── sokol_gl_tc.h    # Forked from upstream util/sokol_gl.h + TrussC modifications
    └── sokol_imgui.h    # Untouched (direct copy from upstream util/)
```

Matches upstream directory layout: core headers at root, utility headers in `util/`.

---

## sokol_app.h

### 1. Skip Present (D3D11 flickering fix)

**Purpose:** Add `sapp_skip_present()` function to skip the next present call, fixing D3D11 flickering in event-driven rendering.

**Changes:**
- Added `skip_present` flag to `_sapp_t` struct
- Added `sapp_skip_present()` function declaration and implementation
- Modified D3D11 present logic to check the flag

### 2. Keyboard Events on Canvas (Emscripten)

**Purpose:** Register keyboard events on canvas element instead of window, allowing other page elements (like Monaco editor in TrussSketch) to handle keyboard events independently.

**Changes:**
- Changed `EMSCRIPTEN_EVENT_TARGET_WINDOW` to `_sapp.html5_canvas_selector` for keydown/keyup/keypress callbacks
- Canvas must have `tabindex` attribute to receive focus

### 3. 10-bit Color Output (Metal / D3D11)

**Purpose:** Use RGB10A2 (10-bit per channel, 2-bit alpha) instead of BGRA8 for the default framebuffer on Metal and D3D11. Reduces color banding on 10-bit displays. Same 32-bit bandwidth as BGRA8, no performance impact. WebGL unchanged.

**Changes:**
- Added `SAPP_PIXELFORMAT_RGB10A2` to `sapp_pixel_format` enum
- macOS Metal: `CAMetalLayer.pixelFormat` = `MTLPixelFormatRGB10A2Unorm` (layer + MSAA texture)
- iOS Metal: same (CAMetalLayer + MSAA texture)
- D3D11: `DXGI_FORMAT_R10G10B10A2_UNORM` (swap chain, MSAA, resize)
- `sapp_color_format()` returns `SAPP_PIXELFORMAT_RGB10A2` for Metal/D3D11

---

## sokol_glue.h

### 4. RGB10A2 Format Mapping

**Purpose:** Map the new `SAPP_PIXELFORMAT_RGB10A2` to `SG_PIXELFORMAT_RGB10A2` in the bridge between sokol_app and sokol_gfx.

---

## util/sokol_gl_tc.h (forked from upstream util/sokol_gl.h)

**Forked from upstream `sokol_gl.h`** and renamed to `sokol_gl_tc.h` to clearly separate from upstream. No other sokol header depends on sokol_gl, so this is a safe rename.

TrussC-specific public functions use the `sgl_tc_` prefix to distinguish from upstream `sgl_` API.

### 5. sg_append_buffer (multi-draw per frame)

**Purpose:** Replace `sg_update_buffer` with `sg_append_buffer` so that `_sgl_draw` can be called multiple times per frame. This is required for the FBO suspend/resume pattern — drawing to an FBO mid-frame without losing the main pass commands.

**Changes:**
- `_sgl_draw()`: uses `sg_append_buffer` instead of `sg_update_buffer`
- Vertex buffer offsets tracked per draw call via `base_vertex`
- GPU buffer recreated on overflow if released

### 6. Auto-grow Buffers

**Purpose:** CPU buffers (vertices, uniforms, commands) automatically grow to 2x capacity on overflow. GPU vertex buffer is also recreated when it overflows. This eliminates rendering loss from buffer exhaustion.

**Changes:**
- `_sgl_next_vertex()`: auto-realloc to 2x on overflow
- `_sgl_next_uniform()`: same
- `_sgl_next_command()`: same
- `_sgl_grow_gpu_buffer()`: destroy and recreate GPU buffer with larger capacity
- Default initial capacities kept small (128 verts, 64 cmds) for low-memory targets

### 7. TrussC-specific Public API (`sgl_tc_` prefix)

These functions do NOT exist in upstream sokol_gl. They are TrussC additions.

| Function | Purpose |
|---|---|
| `sgl_tc_draw_rewind()` | Flush all layers then mark matrix dirty for reuse in same frame (FBO suspend/resume) |
| `sgl_tc_context_draw_rewind(ctx)` | Context-specific version of draw_rewind |
| `sgl_tc_context_reset(ctx)` | Reset command/vertex/uniform counters to zero (fast path between FBO draws on shared context) |
| `sgl_tc_context_release_buffers(ctx)` | Release CPU + GPU buffers to free idle memory (context shell and pipelines preserved) |
| `sgl_tc_context_ensure_buffers(ctx)` | Ensure buffers are allocated (no-op if already allocated, call before drawing after release) |

### 8. Float Vertex Colors (UBYTE4N → FLOAT4)

**Purpose:** Replace 8-bit packed vertex colors with full float precision. The original UBYTE4N format caused visible artifacts in FBO accumulation techniques (trail/afterimage effects) because colors were quantized to 1/255 precision, preventing smooth fade-to-zero.

**Changes:**
- `_sgl_vertex_t.rgba`: `uint32_t` → `float[4]`
- `_sgl_context_t.rgba`: `uint32_t` → `float[4]`
- Vertex attribute format: `SG_VERTEXFORMAT_UBYTE4N` → `SG_VERTEXFORMAT_FLOAT4`
- Removed `_sgl_pack_rgbab()` and `_sgl_pack_rgbaf()` (no longer needed)
- All `sgl_c*f/c*b/c1i` functions store floats directly
- Vertex size increased from 28 to 40 bytes (~1.4x)

---

## sokol_gfx.h

**Untouched.** Pool sizes are configured at runtime in TrussC's `tcGlobal.cpp`:
- `pipeline_pool_size = 256` (default 64)
- `image_pool_size = 10000`
- `view_pool_size = 10000`
- `sampler_pool_size = 10000`

---

## How to Update Sokol

1. Download new headers from https://github.com/floooh/sokol
2. **sokol_gfx.h** — overwrite directly (no modifications)
3. **sokol_app.h** — overwrite, then re-apply patches #1–#3 (search `tettou771`)
4. **sokol_glue.h** — overwrite, then re-apply patch #4
5. **util/sokol_imgui.h** — overwrite directly from upstream `util/sokol_imgui.h`
6. **util/sokol_gl_tc.h** — copy upstream `util/sokol_gl.h`, rename, then re-apply patches #5–#8 (search `[TrussC`)
7. **Other headers** (sokol_log.h, sokol_time.h, etc.) — overwrite directly
8. Test on all platforms (macOS, Windows D3D11, Emscripten Web)

## Author

All modifications by tettou771
