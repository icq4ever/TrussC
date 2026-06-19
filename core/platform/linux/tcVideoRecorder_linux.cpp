// =============================================================================
// tcVideoRecorder_linux.cpp - Linux GStreamer (appsrc -> H.264 -> mp4) impl
// =============================================================================
//
// Mirrors the macOS AVFoundation / Windows Media Foundation backends: it
// implements the three platform hooks declared in tc/video/tcVideoRecorder.h
//   - openPlatform()   : build an appsrc -> encoder -> mp4mux pipeline
//   - appendPlatform() : push one RGBA8 (top-down) frame
//   - closePlatform()  : end-of-stream, flush and finalize the .mp4
//
// GStreamer is the desktop-native multimedia framework on Linux and is already
// a TrussC dependency (Sound/VideoPlayer), so this adds no new library - the
// same "use the platform's native media stack, no ffmpeg" spirit as the other
// backends. The H.264 encoder is chosen at runtime from whatever is installed
// (x264enc / openh264enc / vaapih264enc / v4l2h264enc), so we degrade rather
// than hard-fail on minimal systems.
//
// The recorder feeds RGBA top-down, which is exactly GStreamer's default raw
// video layout, so no vertical flip or R<->B swizzle is needed here (unlike the
// mac/win backends) - videoconvert handles RGBA -> I420 for the encoder.
// =============================================================================

#if defined(__linux__)

#include "TrussC.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <cmath>
#include <string>

namespace trussc {

// ---------------------------------------------------------------------------
// Platform state (the pipeline owns the encoder + mp4 muxer + file sink)
// ---------------------------------------------------------------------------
struct VideoRecorderPlatformData {
    GstElement* pipeline = nullptr;
    GstElement* appsrc   = nullptr;  // borrowed (owned by pipeline)
    int    width  = 0;
    int    height = 0;
    double fps    = 30.0;
    bool   failed = false;
};

namespace {

// Pick the first installed H.264 encoder, preferring quality/ubiquity over
// hardware paths (HW encoders vary wildly in availability/quality on Linux).
// Returns the GStreamer element name, or nullptr if none is available.
const char* pickH264Encoder() {
    static const char* candidates[] = {
        "x264enc",       // software, gst-plugins-ugly - best default
        "openh264enc",   // software, gst-plugins-bad
        "vaapih264enc",  // VA-API hardware (Intel/AMD)
        "nvh264enc",     // NVIDIA NVENC
        "v4l2h264enc",   // V4L2 stateful (Raspberry Pi etc.)
    };
    for (const char* name : candidates) {
        GstElementFactory* f = gst_element_factory_find(name);
        if (f) {
            gst_object_unref(f);
            return name;
        }
    }
    return nullptr;
}

// Split a float fps into an exact rational (num/den) for caps framerate.
void fpsToRatio(double fps, int& num, int& den) {
    if (fps <= 0.0) fps = 30.0;
    double rounded = std::floor(fps + 0.5);
    if (std::fabs(fps - rounded) < 1e-4) {
        num = (int)rounded;
        den = 1;
    } else {
        num = (int)std::floor(fps * 1000.0 + 0.5);
        den = 1000;
    }
}

// Configure the encoder's bitrate. The property and its unit differ per
// encoder, so branch on the element name. `bitsPerSec` is the target rate.
void configureEncoderBitrate(GstElement* enc, const char* name, int bitsPerSec) {
    if (!enc || bitsPerSec <= 0) return;
    if (g_strcmp0(name, "openh264enc") == 0) {
        // openh264enc: "bitrate" in bits/sec.
        g_object_set(enc, "bitrate", (guint)bitsPerSec, nullptr);
    } else {
        // x264enc / vaapih264enc / nvh264enc / v4l2h264enc: "bitrate" in kbit/s.
        guint kbps = (guint)((bitsPerSec + 999) / 1000);
        g_object_set(enc, "bitrate", kbps, nullptr);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// openPlatform - build and start the encoding pipeline
// ---------------------------------------------------------------------------
bool VideoRecorder::openPlatform(const std::string& fullPath, int w, int h,
                                 float fps, int bitrate) {
    // Safe to call multiple times; matches the Sound backend's guard.
    static bool gstInitialized = false;
    if (!gstInitialized) {
        gst_init(nullptr, nullptr);
        gstInitialized = true;
    }

    const char* encName = pickH264Encoder();
    if (!encName) {
        logError("VideoRecorder")
            << "no H.264 GStreamer encoder found (install gst-plugins-ugly "
               "for x264enc, or gst-plugins-bad for openh264enc)";
        return false;
    }

    // h264parse makes the encoder->mp4mux hand-off robust (avc stream-format,
    // AU alignment). Include it only when the plugin is present.
    bool haveParse = false;
    if (GstElementFactory* pf = gst_element_factory_find("h264parse")) {
        gst_object_unref(pf);
        haveParse = true;
    }

    // appsrc -> queue -> videoconvert -> I420 -> <enc> -> [h264parse] -> mp4mux -> filesink
    // Elements are named so we can configure caps/bitrate/location after parse,
    // which avoids fragile shell-style escaping of the output path.
    //
    // The I420 capsfilter forces 4:2:0 chroma. Without it x264enc negotiates
    // 4:4:4 straight from RGBA (profile high-4:4:4), which many players, browser
    // <video> tags and hardware decoders can't play. 4:2:0 is the portable
    // default the mac/win backends also produce.
    std::string desc =
        "appsrc name=src ! queue ! videoconvert ! video/x-raw,format=I420 ! ";
    desc += encName;
    desc += " name=enc ! ";
    if (haveParse) desc += "h264parse ! ";
    desc += "mp4mux name=mux ! filesink name=sink";

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(desc.c_str(), &err);
    if (!pipeline || err) {
        logError("VideoRecorder")
            << "failed to build pipeline: "
            << (err ? err->message : "unknown") << " [" << desc << "]";
        if (err) g_error_free(err);
        if (pipeline) gst_object_unref(pipeline);
        return false;
    }

    GstElement* appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    GstElement* enc    = gst_bin_get_by_name(GST_BIN(pipeline), "enc");
    GstElement* sink   = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (!appsrc || !sink) {
        logError("VideoRecorder") << "pipeline missing src/sink";
        if (appsrc) gst_object_unref(appsrc);
        if (enc) gst_object_unref(enc);
        if (sink) gst_object_unref(sink);
        gst_object_unref(pipeline);
        return false;
    }

    // Output file.
    g_object_set(sink, "location", fullPath.c_str(), nullptr);

    // Encoder bitrate: ~0.1 bits per pixel per second is a good clean-demo
    // default, matching the mac/win backends.
    int br = (bitrate > 0)
                 ? bitrate
                 : (int)((double)w * h * (fps > 0 ? fps : 30.0) * 0.1);
    configureEncoderBitrate(enc, encName, br);
    if (enc) gst_object_unref(enc);

    // appsrc: time-stamped (we set PTS ourselves), pushed synchronously with
    // backpressure so a slow encoder throttles capture instead of ballooning
    // memory. RGBA top-down matches the recorder's frame layout exactly.
    int fnum = 0, fden = 1;
    fpsToRatio((fps > 0) ? fps : 30.0, fnum, fden);
    GstCaps* caps = gst_caps_new_simple(
        "video/x-raw",
        "format",    G_TYPE_STRING, "RGBA",
        "width",     G_TYPE_INT, w,
        "height",    G_TYPE_INT, h,
        "framerate", GST_TYPE_FRACTION, fnum, fden,
        nullptr);
    gst_app_src_set_caps(GST_APP_SRC(appsrc), caps);
    gst_caps_unref(caps);

    g_object_set(appsrc,
                 "format",       GST_FORMAT_TIME,
                 "is-live",      FALSE,
                 "do-timestamp", FALSE,
                 "block",        TRUE,
                 nullptr);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        logError("VideoRecorder")
            << "could not start pipeline (encoder: " << encName << ")";
        gst_object_unref(appsrc);
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        return false;
    }
    gst_object_unref(sink);

    auto* pd = new VideoRecorderPlatformData();
    pd->pipeline = pipeline;
    pd->appsrc   = appsrc;  // keep our ref; released in closePlatform()
    pd->width    = w;
    pd->height   = h;
    pd->fps      = (fps > 0) ? fps : 30.0;
    platform_ = pd;

    logNotice("VideoRecorder") << "GStreamer encoder: " << encName;
    return true;
}

// ---------------------------------------------------------------------------
// appendPlatform - push one RGBA8 (top-down) frame into the pipeline
// ---------------------------------------------------------------------------
bool VideoRecorder::appendPlatform(const unsigned char* rgba, double timeSec) {
    VideoRecorderPlatformData* pd = platform_;
    if (!pd || !pd->appsrc || pd->failed) return false;

    const gsize size = (gsize)pd->width * pd->height * 4;
    GstBuffer* buf = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (!buf) {
        logError("VideoRecorder") << "could not allocate frame buffer";
        pd->failed = true;
        return false;
    }

    // RGBA top-down -> GStreamer RGBA top-down: a straight copy.
    gst_buffer_fill(buf, 0, rgba, size);

    // PTS is decided by the caller (fixed-rate for manual addFrame(), real
    // elapsed time for auto-capture). Duration is one frame at the target fps.
    GST_BUFFER_PTS(buf) = (GstClockTime)(timeSec * GST_SECOND);
    GST_BUFFER_DURATION(buf) =
        (GstClockTime)(GST_SECOND / (pd->fps > 0 ? pd->fps : 30.0));

    GstFlowReturn ret =
        gst_app_src_push_buffer(GST_APP_SRC(pd->appsrc), buf);  // takes ownership
    if (ret != GST_FLOW_OK) {
        logError("VideoRecorder")
            << "appsrc rejected frame: " << gst_flow_get_name(ret);
        pd->failed = true;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// closePlatform - end the stream, wait for the muxer to finalize, tear down
// ---------------------------------------------------------------------------
void VideoRecorder::closePlatform() {
    VideoRecorderPlatformData* pd = platform_;
    if (!pd) return;

    if (pd->pipeline && pd->appsrc && !pd->failed) {
        // Signal EOS so mp4mux writes its moov atom, then wait for the EOS (or
        // ERROR) to travel the pipeline - otherwise the file is left truncated.
        if (gst_app_src_end_of_stream(GST_APP_SRC(pd->appsrc)) == GST_FLOW_OK) {
            GstBus* bus = gst_element_get_bus(pd->pipeline);
            if (bus) {
                // Generous timeout: software H.264 flush of a long clip.
                GstMessage* msg = gst_bus_timed_pop_filtered(
                    bus, 30 * GST_SECOND,
                    (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
                if (!msg) {
                    logWarning("VideoRecorder")
                        << "timed out waiting for EOS; file may be incomplete";
                } else {
                    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                        GError* e = nullptr;
                        gst_message_parse_error(msg, &e, nullptr);
                        logError("VideoRecorder")
                            << "pipeline error on finalize: "
                            << (e ? e->message : "unknown");
                        if (e) g_error_free(e);
                    }
                    gst_message_unref(msg);
                }
                gst_object_unref(bus);
            }
        }
    }

    if (pd->pipeline) {
        gst_element_set_state(pd->pipeline, GST_STATE_NULL);
    }
    if (pd->appsrc)   gst_object_unref(pd->appsrc);
    if (pd->pipeline) gst_object_unref(pd->pipeline);

    delete pd;
    platform_ = nullptr;
}

} // namespace trussc

#endif // __linux__
