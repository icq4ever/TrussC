#pragma once

// =============================================================================
// tcVideoRecorder.h - Native video recording (H.264 -> .mp4)
// =============================================================================
//
// Records rendered frames to a video file using each platform's native
// encoder (no ffmpeg, no extra dependency):
//   - macOS:   AVFoundation (AVAssetWriter)
//   - Windows: Media Foundation (IMFSinkWriter)  [planned]
//
// Frames can come from:
//   - the live swapchain (whole window)         addFrame()
//   - an offscreen Fbo (clean output, no GUI)   addFrame(fbo)
//   - CPU pixels you already have               addFrame(pixels)
//
// Timestamps are driven by a fixed frame rate (frameIndex / fps), so the
// output plays back at the correct speed even if capture stalls the app.
// This makes it well suited to offline, high-quality demo capture.
//
// This file is included from TrussC.h AFTER Fbo, Pixels and grabScreen, whose
// definitions the inline frame-input helpers below rely on.
// =============================================================================

#include <string>
#include <vector>
#include <filesystem>

namespace trussc {

// Opaque platform state (AVAssetWriter / IMFSinkWriter etc.)
struct VideoRecorderPlatformData;

// ---------------------------------------------------------------------------
// VideoRecorder - encodes RGBA frames to a video file
// ---------------------------------------------------------------------------
class VideoRecorder {
public:
    VideoRecorder() = default;
    ~VideoRecorder() { close(); }

    // Non-copyable
    VideoRecorder(const VideoRecorder&) = delete;
    VideoRecorder& operator=(const VideoRecorder&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    // Open a recording. `path` is resolved via getDataPath() when relative.
    // `fps` sets the playback rate. `bitrate` in bits/sec (0 = auto from size).
    // Returns false if the encoder could not be created.
    bool setup(const std::string& path, int width, int height,
               float fps = 30.0f, int bitrate = 0) {
        close();
        if (width <= 0 || height <= 0) {
            logError("VideoRecorder") << "invalid size " << width << "x" << height;
            return false;
        }
        // Encoders want even dimensions.
        if ((width & 1) || (height & 1)) {
            logWarning("VideoRecorder")
                << "size " << width << "x" << height
                << " is odd; H.264 needs even dimensions";
        }
        path_ = getDataPath(path);
        // Native encoders won't create intermediate directories — ensure the
        // parent exists (e.g. bin/data on first run).
        {
            std::error_code ec;
            std::filesystem::path parent =
                std::filesystem::path(path_).parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent, ec);
            }
        }
        width_ = width;
        height_ = height;
        fps_ = (fps > 0.0f) ? fps : 30.0f;
        bitrate_ = bitrate;
        frameCount_ = 0;
        scratch_.resize((size_t)width_ * height_ * 4);

        if (!openPlatform(path_, width_, height_, fps_, bitrate_)) {
            logError("VideoRecorder") << "failed to open encoder for " << path_;
            return false;
        }
        recording_ = true;
        logNotice("VideoRecorder")
            << "recording " << width_ << "x" << height_
            << " @ " << fps_ << "fps -> " << path_;
        return true;
    }

    // Finalize and flush the file. Safe to call multiple times.
    void close() {
        stop();  // detach the afterFrame listener if auto-capturing
        if (recording_) {
            closePlatform();
            logNotice("VideoRecorder")
                << "saved " << frameCount_ << " frames -> " << path_;
        }
        recording_ = false;
    }

    // =========================================================================
    // Auto-capture - capture every frame on your behalf until close()
    // =========================================================================

    // Capture the live swapchain every frame (the same fully-composited image
    // get_screenshot returns), via the afterFrame hook. setup() must precede it,
    // and its width/height must match the window's framebuffer pixel size.
    bool start() { return startAuto(Source::Swapchain, nullptr); }

    // Capture an offscreen Fbo every frame (clean output, GUI-free).
    bool start(const Fbo& fbo) { return startAuto(Source::Fbo, &fbo); }

    // Stop auto-capture but keep the file open (close() also stops it).
    void stop() {
        afterFrameListener_ = EventListener{};
        started_ = false;
        autoSource_ = Source::None;
        autoFbo_ = nullptr;
    }

    bool isAutoCapturing() const { return started_; }

    bool isRecording() const { return recording_; }
    int  getFrameCount() const { return frameCount_; }
    int  getWidth() const { return width_; }
    int  getHeight() const { return height_; }
    float getFps() const { return fps_; }
    const std::string& getPath() const { return path_; }

    // =========================================================================
    // Frame input - each call appends exactly one frame (1/fps of video time)
    // =========================================================================

    // Grab the live swapchain (whole window, includes any GUI).
    bool addFrame() {
        if (!recording_) return false;
        Pixels px;
        if (!grabScreen(px) || !px.isAllocated()) {
            logError("VideoRecorder") << "grabScreen failed";
            return false;
        }
        return addFrame(px);
    }

    // Read back an offscreen Fbo (clean output, GUI-free). Must match
    // the recorder's width/height.
    bool addFrame(const Fbo& fbo) {
        if (!recording_) return false;
        if (fbo.getWidth() != width_ || fbo.getHeight() != height_) {
            logError("VideoRecorder")
                << "fbo size " << fbo.getWidth() << "x" << fbo.getHeight()
                << " != recorder " << width_ << "x" << height_;
            return false;
        }
        if (!fbo.readPixels(scratch_.data())) {
            logError("VideoRecorder") << "fbo readPixels failed";
            return false;
        }
        return appendRGBA(scratch_.data());
    }

    // Append CPU pixels directly (RGBA8, top-down). Must match width/height.
    bool addFrame(const Pixels& pixels) {
        if (!recording_) return false;
        if (pixels.getWidth() != width_ || pixels.getHeight() != height_) {
            logError("VideoRecorder")
                << "pixels size " << pixels.getWidth() << "x" << pixels.getHeight()
                << " != recorder " << width_ << "x" << height_;
            return false;
        }
        if (pixels.getChannels() == 4) {
            return appendRGBA(pixels.getData());
        }
        // Expand to RGBA into the scratch buffer.
        const unsigned char* src = pixels.getData();
        const int ch = pixels.getChannels();
        const size_t n = (size_t)width_ * height_;
        for (size_t i = 0; i < n; ++i) {
            unsigned char r = src[i * ch + 0];
            unsigned char g = (ch > 1) ? src[i * ch + 1] : r;
            unsigned char b = (ch > 2) ? src[i * ch + 2] : r;
            scratch_[i * 4 + 0] = r;
            scratch_[i * 4 + 1] = g;
            scratch_[i * 4 + 2] = b;
            scratch_[i * 4 + 3] = 255;
        }
        return appendRGBA(scratch_.data());
    }

private:
    enum class Source { None, Swapchain, Fbo };

    // Begin auto-capture from a source via the afterFrame hook.
    bool startAuto(Source src, const Fbo* fbo) {
        if (!recording_) {
            logError("VideoRecorder") << "start() called before setup()";
            return false;
        }
        autoSource_ = src;
        autoFbo_ = fbo;
        startElapsed_ = getElapsedTimef();
        lastCaptureElapsed_ = -1.0f;
        started_ = true;
        afterFrameListener_ = events().afterFrame.listen([this]() { onAfterFrame(); });
        return true;
    }

    // Called after present() each frame while auto-capturing.
    void onAfterFrame() {
        if (!recording_ || !started_) return;
        float now = getElapsedTimef();
        // Throttle to the target fps (drop frames that arrive too soon).
        if (lastCaptureElapsed_ >= 0.0f &&
            (now - lastCaptureElapsed_) < (1.0f / fps_) * 0.999f) {
            return;
        }
        lastCaptureElapsed_ = now;
        if (autoSource_ == Source::Fbo && autoFbo_) addFrame(*autoFbo_);
        else                                        addFrame();
    }

    // Common path: feed one RGBA8 (top-down) frame of the recorder's size.
    bool appendRGBA(const unsigned char* rgba) {
        if (!recording_ || !rgba) return false;
        // Manual addFrame() uses a deterministic fixed-rate clock (offline);
        // auto-capture (start()) uses real elapsed time so live plays at 1x.
        double timeSec = started_
            ? (double)(getElapsedTimef() - startElapsed_)
            : (double)frameCount_ / (double)fps_;
        if (!appendPlatform(rgba, timeSec)) {
            logError("VideoRecorder") << "encoder rejected frame " << frameCount_;
            return false;
        }
        ++frameCount_;
        return true;
    }

    // Platform hooks (defined in platform/<os>/tcVideoRecorder_<os>.*)
    bool openPlatform(const std::string& fullPath, int w, int h,
                      float fps, int bitrate);
    bool appendPlatform(const unsigned char* rgba, double timeSec);
    void closePlatform();

    VideoRecorderPlatformData* platform_ = nullptr;
    std::string path_;
    std::vector<unsigned char> scratch_;
    int   width_  = 0;
    int   height_ = 0;
    float fps_    = 30.0f;
    int   bitrate_ = 0;
    int   frameCount_ = 0;
    bool  recording_ = false;

    // Auto-capture (start()) state.
    Source autoSource_ = Source::None;
    const Fbo* autoFbo_ = nullptr;
    bool  started_ = false;
    float startElapsed_ = 0.0f;
    float lastCaptureElapsed_ = -1.0f;
    EventListener afterFrameListener_;
};

// ---------------------------------------------------------------------------
// Convenience: one global recorder for "just record the whole window".
// ---------------------------------------------------------------------------
namespace internal {
    inline VideoRecorder& globalRecorder() {
        static VideoRecorder rec;
        return rec;
    }
}

// Start recording the whole window (swapchain) to an H.264 .mp4.
// Size is taken from the framebuffer automatically. `path` is required;
// relative paths resolve via getDataPath(). Auto-finalizes on app exit.
inline bool startRecording(const std::string& path, float fps = 60.0f) {
    // Finalize cleanly on app exit (registered once).
    static EventListener exitListener =
        events().exit.listen([]() { internal::globalRecorder().close(); });
    (void)exitListener;

    VideoRecorder& rec = internal::globalRecorder();
    if (rec.isRecording()) rec.close();  // restart if already going
    if (!rec.setup(path, sapp_width(), sapp_height(), fps)) return false;
    return rec.start();
}

// Stop the global recording and flush the file.
inline void stopRecording() {
    internal::globalRecorder().close();
}

inline bool isRecording() {
    return internal::globalRecorder().isRecording();
}

// Frames written so far (valid during and right after recording).
inline int recordingFrameCount() {
    return internal::globalRecorder().getFrameCount();
}

// Resolved output path of the global recording.
inline const std::string& recordingPath() {
    return internal::globalRecorder().getPath();
}

} // namespace trussc
