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
    void renderScene();  // draw the animated scene into fbo_

    Fbo fbo_;                 // clean offscreen output (no GUI overlay)
    VideoRecorder recorder_;

    int   recW_ = 640;        // clean recording size
    int   recH_ = 480;
    float fps_  = 30.0f;

    bool recordScreen_ = false;  // false: clean Fbo, true: whole window

    void toggleRecording();
};
