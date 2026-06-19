// =============================================================================
// videoWriterExample - Offline frame-by-frame video export
// =============================================================================
// The counterpart to videoRecExample (which does live ScreenRecorder capture).
// Here we drive a VideoWriter ourselves: open() -> addFrame() per frame -> close().
//
//   e : export a deterministic clip (exportFrames_ / fps_ seconds)
//
// The point of VideoWriter is determinism: the scene time comes from the frame
// INDEX (exportFrame_ / fps_), not the wall clock. So the output is always
// exactly the same length with perfectly smooth motion, no matter how fast (or
// slow) the export loop actually runs — ideal for rendering animations to file.
// For live "record what's on screen now" use ScreenRecorder / startRecording().
//
// Output lands in bin/data (getDataPath): videoWriter_export.mp4
// =============================================================================

#include "tcApp.h"
#include <cmath>
using namespace std;

void tcApp::setup() {
    fbo_.allocate(recW_, recH_);
    logNotice("tcApp") << "videoWriterExample - press 'e' to export a deterministic clip";
}

void tcApp::update() {
}

void tcApp::renderScene(float t) {
    fbo_.begin(0.05f, 0.06f, 0.10f, 1.0f);

    const float cx = recW_ * 0.5f;
    const float cy = recH_ * 0.5f;
    const int n = 12;
    for (int i = 0; i < n; ++i) {
        float a = TAU * i / n + t * 0.5f;
        float r = recH_ * 0.3f;
        float x = cx + cosf(a) * r;
        float y = cy + sinf(a) * r;
        setColorHSB(fmodf(i / (float)n + t * 0.1f, 1.0f), 0.7f, 1.0f);
        drawCircle(x, y, 22.0f + 10.0f * sinf(t * 2.0f + i));
    }
    setColor(1.0f);
    float bx = fmodf(t * 160.0f, (float)recW_);
    drawRect(bx, 0.0f, 4.0f, (float)recH_);

    fbo_.end();
}

void tcApp::startExport() {
    if (exporting_) return;
    // VideoWriter::open() takes the size + a VideoRecordSettings (fps lives in it,
    // default 60). Same settings as ScreenRecorder — e.g. a ProRes master on macOS:
    //   writer_.open("master.mov", recW_, recH_, { .codec = VideoCodec::ProRes422, .fps = fps_ });
    if (writer_.open("videoWriter_export.mp4", recW_, recH_, { .fps = fps_ })) {
        exporting_ = true;
        exportFrame_ = 0;
        logNotice("tcApp") << "exporting " << exportFrames_ << " frames ("
                           << exportFrames_ / fps_ << "s @ " << fps_ << "fps)...";
    }
}

void tcApp::draw() {
    clear(0.12f);

    if (exporting_) {
        // Deterministic: scene time = frame index / fps (NOT wall clock).
        float t = exportFrame_ / fps_;
        renderScene(t);
        writer_.addFrame(fbo_);   // fixed-rate timestamp (frameIndex / fps)
        if (++exportFrame_ >= exportFrames_) {
            writer_.close();
            exporting_ = false;
            logNotice("tcApp") << "export done -> " << writer_.getPath();
        }
    } else {
        // Idle: live preview driven by the wall clock.
        renderScene(getElapsedTimef());
    }

    // Fit the Fbo into the window (letterbox).
    float s = std::min(getWindowWidth() / (float)recW_,
                       getWindowHeight() / (float)recH_);
    float w = recW_ * s, h = recH_ * s;
    float x = (getWindowWidth() - w) * 0.5f;
    float y = (getWindowHeight() - h) * 0.5f;
    setColor(1.0f);
    fbo_.draw(x, y, w, h);

    setColor(exporting_ ? colors::red : colors::yellow);
    string status = exporting_
        ? format("● EXPORT  {}/{} frames", exportFrame_, exportFrames_)
        : "press 'e' to export a deterministic clip (VideoWriter)";
    drawBitmapString(status, 12, 22);
}

void tcApp::keyPressed(int key) {
    if (key == 'e' || key == 'E') startExport();
}

void tcApp::exit() {
    writer_.close();
}
