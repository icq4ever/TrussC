// =============================================================================
// Android Platform-specific Functions
// =============================================================================

#include "TrussC.h"

#ifdef __ANDROID__

#include <android/log.h>
#include <android/native_activity.h>
#include "sokol_app.h"

// GLES3 for screen capture
#include <GLES3/gl3.h>

namespace trussc {
namespace platform {

float getDisplayScaleFactor() {
    return sapp_dpi_scale();
}

void setWindowSize(int width, int height) {
    // Android apps are fullscreen — window size is determined by the device
    (void)width;
    (void)height;
}

std::string getExecutablePath() {
    // Android doesn't have a traditional executable path.
    // Return empty — use getExecutableDir() for asset access.
    return "";
}

std::string getExecutableDir() {
    // On Android, assets are accessed via AAssetManager, not filesystem paths.
    // For files that need a real path, use internal storage.
    auto* activity = (ANativeActivity*)sapp_android_get_native_activity();
    if (activity && activity->internalDataPath) {
        return std::string(activity->internalDataPath) + "/";
    }
    return "/data/local/tmp/";
}

// ---------------------------------------------------------------------------
// Screenshot Functions (GLES3)
// ---------------------------------------------------------------------------

bool captureWindow(Pixels& outPixels) {
    int width = sapp_width();
    int height = sapp_height();

    if (width <= 0 || height <= 0) {
        logError("Screenshot") << "Invalid window dimensions";
        return false;
    }

    outPixels.allocate(width, height, 4);

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outPixels.getData());

    // OpenGL reads from bottom-left, flip vertically
    unsigned char* data = outPixels.getData();
    std::vector<unsigned char> rowBuffer(width * 4);
    for (int y = 0; y < height / 2; y++) {
        int topRow = y * width * 4;
        int bottomRow = (height - 1 - y) * width * 4;
        memcpy(rowBuffer.data(), &data[topRow], width * 4);
        memcpy(&data[topRow], &data[bottomRow], width * 4);
        memcpy(&data[bottomRow], rowBuffer.data(), width * 4);
    }

    return true;
}

bool saveScreenshot(const std::filesystem::path& path) {
    Pixels pixels;
    if (!captureWindow(pixels)) {
        return false;
    }

    std::string ext = path.extension().string();
    std::string pathStr = path.string();

    int width = pixels.getWidth();
    int height = pixels.getHeight();
    unsigned char* data = pixels.getData();

    int result = 0;
    if (ext == ".png") {
        result = stbi_write_png(pathStr.c_str(), width, height, 4, data, width * 4);
    } else if (ext == ".jpg" || ext == ".jpeg") {
        result = stbi_write_jpg(pathStr.c_str(), width, height, 4, data, 90);
    } else if (ext == ".bmp") {
        result = stbi_write_bmp(pathStr.c_str(), width, height, 4, data);
    } else {
        result = stbi_write_png(pathStr.c_str(), width, height, 4, data, width * 4);
    }

    if (result) {
        logVerbose("Screenshot") << "Saved: " << path;
        return true;
    } else {
        logError("Screenshot") << "Failed to save: " << path;
        return false;
    }
}

} // namespace platform
} // namespace trussc

#endif // __ANDROID__
