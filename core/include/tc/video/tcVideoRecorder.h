#pragma once

// =============================================================================
// tcVideoRecorder.h - Native video recording (H.264 / HEVC / ProRes -> file)
// =============================================================================
//
// Two layers, each platform-native (no ffmpeg, no extra dependency):
//
//   VideoWriter     - low-level encoder. You feed it frames; it writes a file.
//                       open() -> addFrame()/addFrameAt() ... -> close()
//                     Deterministic, caller-driven (fixed-rate timestamps).
//                     This is what the per-platform backends implement.
//
//   ScreenRecorder  - high-level live capture. Records the window (or an Fbo)
//                       every frame at wall-clock speed until stop().
//                       start(path) / start(fbo, path) -> stop()
//                     Size is taken automatically; built on a VideoWriter.
//
//   startRecording()/stopRecording() - global convenience over a singleton
//                     ScreenRecorder ("just record the whole window").
//
// Per-platform encoders:
//   - macOS:   AVFoundation (AVAssetWriter)        H.264 / HEVC / ProRes
//   - Windows: Media Foundation (IMFSinkWriter)    H.264 (+ HW->SW fallback)
//   - Linux:   GStreamer (appsrc -> enc -> mux)    H.264 / HEVC (HW-probed)
//
// This file is included from TrussC.h AFTER Fbo, Pixels, grabScreen, events()
// and sapp_width(), whose definitions the inline helpers below rely on.
// =============================================================================

#include <string>
#include <vector>
#include <filesystem>

namespace trussc {

class Fbo;
class Pixels;

// Opaque per-platform encoder state (AVAssetWriter / IMFSinkWriter / GstPipeline)
struct VideoWriterPlatformData;

// ---------------------------------------------------------------------------
// Codec / settings
// ---------------------------------------------------------------------------
enum class VideoCodec {
    H264,        // .mp4, universal — the default
    HEVC,        // .mp4, H.265
    ProRes422,   // .mov, near-lossless mastering (macOS only)
    ProRes4444,  // .mov, ProRes with alpha (macOS only)
};

inline const char* videoCodecName(VideoCodec c) {
    switch (c) {
        case VideoCodec::H264:       return "H.264";
        case VideoCodec::HEVC:       return "HEVC";
        case VideoCodec::ProRes422:  return "ProRes 422";
        case VideoCodec::ProRes4444: return "ProRes 4444";
    }
    return "?";
}

struct VideoRecordSettings {
    VideoCodec codec = VideoCodec::H264;
    float fps = 60.0f;          // ScreenRecorder: capture ceiling (real-time PTS,
                                // so a slower app just yields fewer frames at 1x).
                                // VideoWriter: the exact output frame rate.
    int bitrate = 0;            // bits/sec for H.264/HEVC; 0 = auto. Ignored by ProRes.
    int keyframeInterval = 0;   // frames between keyframes; 0 = encoder default.
};

// ---------------------------------------------------------------------------
// VideoWriter - low-level encoder you feed frames to.
// ---------------------------------------------------------------------------
class VideoWriter {
public:
    VideoWriter() = default;
    ~VideoWriter() { close(); }
    VideoWriter(const VideoWriter&) = delete;
    VideoWriter& operator=(const VideoWriter&) = delete;

    // Open the encoder. `path` is resolved via getDataPath() when relative; the
    // parent directory is created if missing. Returns false if the encoder (or
    // the requested codec on this platform) could not be created.
    bool open(const std::string& path, int width, int height,
              const VideoRecordSettings& settings = {}) {
        close();
        if (width <= 0 || height <= 0) {
            logError("VideoWriter") << "invalid size " << width << "x" << height;
            return false;
        }
        if ((width & 1) || (height & 1)) {
            logWarning("VideoWriter")
                << "size " << width << "x" << height
                << " is odd; most codecs need even dimensions";
        }
        path_ = getDataPath(path);
        {   // native encoders won't create intermediate directories
            std::error_code ec;
            std::filesystem::path parent =
                std::filesystem::path(path_).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent, ec);
        }
        width_ = width;
        height_ = height;
        settings_ = settings;
        fps_ = (settings_.fps > 0.0f) ? settings_.fps : 60.0f;
        frameCount_ = 0;
        scratch_.resize((size_t)width_ * height_ * 4);

        if (!openPlatform(path_, width_, height_, fps_, settings_)) {
            logError("VideoWriter")
                << "failed to open " << videoCodecName(settings_.codec)
                << " encoder for " << path_;
            return false;
        }
        open_ = true;
        logNotice("VideoWriter")
            << "writing " << width_ << "x" << height_ << " @ " << fps_ << "fps "
            << videoCodecName(settings_.codec) << " -> " << path_;
        return true;
    }

    // Finalize and flush the file. Safe to call multiple times.
    void close() {
        if (open_) {
            closePlatform();
            logNotice("VideoWriter")
                << "saved " << frameCount_ << " frames -> " << path_;
        }
        open_ = false;
    }

    bool isOpen() const { return open_; }
    int  getFrameCount() const { return frameCount_; }
    int  getWidth() const { return width_; }
    int  getHeight() const { return height_; }
    float getFps() const { return fps_; }
    const std::string& getPath() const { return path_; }
    const VideoRecordSettings& getSettings() const { return settings_; }

    // Append one frame at the fixed-rate clock (frameIndex / fps) — deterministic
    // offline render. Source size must match the writer's size.
    bool addFrame(const Fbo& fbo)      { return addFrameAt(fbo, frameCount_ / (double)fps_); }
    bool addFrame(const Pixels& pixels){ return addFrameAt(pixels, frameCount_ / (double)fps_); }

    // Append one frame at an explicit presentation time (seconds). Used by
    // ScreenRecorder for wall-clock live capture, or for custom timing.
    bool addFrameAt(const Fbo& fbo, double timeSec) {
        if (!open_) return false;
        if (fbo.getWidth() != width_ || fbo.getHeight() != height_) {
            logError("VideoWriter")
                << "fbo size " << fbo.getWidth() << "x" << fbo.getHeight()
                << " != writer " << width_ << "x" << height_;
            return false;
        }
        if (!fbo.readPixels(scratch_.data())) {
            logError("VideoWriter") << "fbo readPixels failed";
            return false;
        }
        return appendRGBA(scratch_.data(), timeSec);
    }
    bool addFrameAt(const Pixels& pixels, double timeSec) {
        if (!open_) return false;
        if (pixels.getWidth() != width_ || pixels.getHeight() != height_) {
            logError("VideoWriter")
                << "pixels size " << pixels.getWidth() << "x" << pixels.getHeight()
                << " != writer " << width_ << "x" << height_;
            return false;
        }
        if (pixels.getChannels() == 4) return appendRGBA(pixels.getData(), timeSec);
        // Expand to RGBA into the scratch buffer.
        const unsigned char* src = pixels.getData();
        const int ch = pixels.getChannels();
        const size_t n = (size_t)width_ * height_;
        for (size_t i = 0; i < n; ++i) {
            unsigned char r = src[i * ch + 0];
            scratch_[i * 4 + 0] = r;
            scratch_[i * 4 + 1] = (ch > 1) ? src[i * ch + 1] : r;
            scratch_[i * 4 + 2] = (ch > 2) ? src[i * ch + 2] : r;
            scratch_[i * 4 + 3] = 255;
        }
        return appendRGBA(scratch_.data(), timeSec);
    }

private:
    bool appendRGBA(const unsigned char* rgba, double timeSec) {
        if (!open_ || !rgba) return false;
        if (!appendPlatform(rgba, timeSec)) {
            logError("VideoWriter") << "encoder rejected frame " << frameCount_;
            return false;
        }
        ++frameCount_;
        return true;
    }

    // Platform hooks (defined in platform/<os>/tcVideoRecorder_<os>.*)
    bool openPlatform(const std::string& fullPath, int w, int h, float fps,
                      const VideoRecordSettings& settings);
    bool appendPlatform(const unsigned char* rgba, double timeSec);
    void closePlatform();

    VideoWriterPlatformData* platform_ = nullptr;
    std::string path_;
    std::vector<unsigned char> scratch_;
    int   width_  = 0;
    int   height_ = 0;
    float fps_    = 30.0f;
    VideoRecordSettings settings_;
    int   frameCount_ = 0;
    bool  open_ = false;
};

// ---------------------------------------------------------------------------
// ScreenRecorder - live capture of the window (or an Fbo) at wall-clock speed.
// ---------------------------------------------------------------------------
class ScreenRecorder {
public:
    ScreenRecorder() = default;
    ~ScreenRecorder() { stop(); }
    ScreenRecorder(const ScreenRecorder&) = delete;
    ScreenRecorder& operator=(const ScreenRecorder&) = delete;

    // Record the whole window (swapchain) — the same fully-composited image
    // get_screenshot returns. Size is taken from the framebuffer.
    bool start(const std::string& path, const VideoRecordSettings& settings = {}) {
        return startSource(Source::Swapchain, nullptr,
                           sapp_width(), sapp_height(), path, settings);
    }

    // Record an offscreen Fbo every frame (clean output, GUI-free).
    bool start(const Fbo& fbo, const std::string& path,
               const VideoRecordSettings& settings = {}) {
        return startSource(Source::Fbo, &fbo,
                           fbo.getWidth(), fbo.getHeight(), path, settings);
    }

    // Stop capturing and finalize the file. Safe to call multiple times.
    void stop() {
        afterFrameListener_ = EventListener{};
        exitListener_ = EventListener{};
        source_ = Source::None;
        fbo_ = nullptr;
        writer_.close();
    }

    bool isRecording() const { return writer_.isOpen(); }
    int  getFrameCount() const { return writer_.getFrameCount(); }
    const std::string& getPath() const { return writer_.getPath(); }
    VideoWriter& writer() { return writer_; }   // for advanced introspection

private:
    enum class Source { None, Swapchain, Fbo };

    bool startSource(Source src, const Fbo* fbo, int w, int h,
                     const std::string& path,
                     const VideoRecordSettings& settings) {
        stop();
        if (!writer_.open(path, w, h, settings)) return false;
        source_ = src;
        fbo_ = fbo;
        startElapsed_ = getElapsedTimef();
        lastCaptureElapsed_ = -1.0f;
        afterFrameListener_ = events().afterFrame.listen([this]() { onAfterFrame(); });
        exitListener_ = events().exit.listen([this]() { stop(); });
        return true;
    }

    void onAfterFrame() {
        if (!writer_.isOpen()) return;
        float now = getElapsedTimef();
        float fps = writer_.getFps();
        // Throttle to the target fps (drop frames that arrive too soon).
        if (lastCaptureElapsed_ >= 0.0f &&
            (now - lastCaptureElapsed_) < (1.0f / fps) * 0.999f) {
            return;
        }
        lastCaptureElapsed_ = now;
        double t = (double)(now - startElapsed_);   // wall-clock PTS
        if (source_ == Source::Fbo && fbo_) {
            writer_.addFrameAt(*fbo_, t);
        } else {
            Pixels px;
            if (grabScreen(px) && px.isAllocated()) writer_.addFrameAt(px, t);
        }
    }

    VideoWriter writer_;
    Source source_ = Source::None;
    const Fbo* fbo_ = nullptr;
    float startElapsed_ = 0.0f;
    float lastCaptureElapsed_ = -1.0f;
    EventListener afterFrameListener_;
    EventListener exitListener_;
};

// ---------------------------------------------------------------------------
// Convenience: one global ScreenRecorder for "just record the whole window".
// ---------------------------------------------------------------------------
namespace internal {
    inline ScreenRecorder& globalScreenRecorder() {
        static ScreenRecorder rec;
        return rec;
    }
}

// Start recording the whole window to a video file. `path` is required;
// relative paths resolve via getDataPath(). Auto-finalizes on app exit.
inline bool startRecording(const std::string& path,
                           const VideoRecordSettings& settings = {}) {
    return internal::globalScreenRecorder().start(path, settings);
}

inline void stopRecording() { internal::globalScreenRecorder().stop(); }
inline bool isRecording()   { return internal::globalScreenRecorder().isRecording(); }
inline int  recordingFrameCount() { return internal::globalScreenRecorder().getFrameCount(); }
inline const std::string& recordingPath() { return internal::globalScreenRecorder().getPath(); }

} // namespace trussc
