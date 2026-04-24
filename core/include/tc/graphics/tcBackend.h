#pragma once

// =============================================================================
// Graphics backend detection
// Runtime query for the active sokol_gfx backend.
// Note: values are only meaningful after sg_setup() has run.
// =============================================================================

#include "sokol/sokol_gfx.h"
#include "tcPlatform.h"

namespace trussc {

struct GraphicsBackend {
    static bool isWebGPU() { return sg_query_backend() == SG_BACKEND_WGPU; }
    static bool isWebGL2() { return Platform::isWeb() && sg_query_backend() == SG_BACKEND_GLES3; }
    static bool isMetal() {
        const sg_backend b = sg_query_backend();
        return b == SG_BACKEND_METAL_MACOS
            || b == SG_BACKEND_METAL_IOS
            || b == SG_BACKEND_METAL_SIMULATOR;
    }
    static bool isD3D11()  { return sg_query_backend() == SG_BACKEND_D3D11; }
    static bool isVulkan() { return sg_query_backend() == SG_BACKEND_VULKAN; }
    static bool isOpenGL() {
        const sg_backend b = sg_query_backend();
        return b == SG_BACKEND_GLCORE || b == SG_BACKEND_GLES3;
    }

    // String form — "opengl" / "gles3" / "webgl2" / "d3d11" / "metal" / "webgpu" / "vulkan" / "dummy" / "unknown"
    static const char* name() {
        switch (sg_query_backend()) {
            case SG_BACKEND_GLCORE:          return "opengl";
            case SG_BACKEND_GLES3:           return Platform::isWeb() ? "webgl2" : "gles3";
            case SG_BACKEND_D3D11:           return "d3d11";
            case SG_BACKEND_METAL_IOS:
            case SG_BACKEND_METAL_MACOS:
            case SG_BACKEND_METAL_SIMULATOR: return "metal";
            case SG_BACKEND_WGPU:            return "webgpu";
            case SG_BACKEND_VULKAN:          return "vulkan";
            case SG_BACKEND_DUMMY:           return "dummy";
        }
        return "unknown";
    }
};

} // namespace trussc
