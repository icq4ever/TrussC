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
    void renderScene(float t);  // deterministic scene at time t (seconds)
    void startExport();

    Fbo fbo_;
    VideoWriter writer_;        // low-level encoder: we feed it frames

    int   recW_ = 640;          // export size
    int   recH_ = 480;
    float fps_  = 30.0f;
    int   exportFrames_ = 150;  // 150 / 30 = exactly 5 seconds

    bool  exporting_ = false;
    int   exportFrame_ = 0;
};
