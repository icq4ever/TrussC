#pragma once

#include <TrussC.h>
using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void exit() override;

private:
    void drawSpinBox(float cx, float cy, float size);  // rotating wireframe box
    void startExport();                                // VideoWriter offline
    string statusLine();

    Fbo fbo_;              // offscreen "clean" layer (green is drawn in here)
    ScreenRecorder rec_;   // live recorder instance (window OR fbo)
    VideoWriter    writer_; // offline encoder (you feed it frames)

    float fps_ = 30.0f;

    bool exporting_   = false;
    int  exportFrame_ = 0;
    int  exportFrames_ = 150;  // 150 / 30 = 5s deterministic export
};
