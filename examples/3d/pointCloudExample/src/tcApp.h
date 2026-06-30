#pragma once

#include <TrussC.h>
using namespace std;
using namespace tc;

// Draw a STATIC point cloud of N points every frame, comparing the immediate
// 1px path (PointStyle::Pixel -> sokol_gl, re-streams every vertex per frame)
// with the GPU splat path (PrimitiveMode::Points + Square/Round, a retained
// instance buffer drawn in one instanced call). The cloud is rebuilt only when
// N changes, so this isolates the *draw* cost from any per-frame meshing.
//
//   G       cycle point style: Square -> Round -> Pixel (immediate 1px)
//   1..6    set N (50k / 100k / 250k / 500k / 1M / 2M)
//   [ ]     point size (logical px; ignored for Pixel)
//   drag    orbit
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void rebuild(int n);
    const char* styleName() const;

    EasyCam view;
    Mesh    cloud;                 // static point cloud (rebuilt only on N change)

    int        n_ = 250000;
    PointStyle style_ = PointStyle::Square;
    float      pointSize_ = 3.0f;  // logical px

    float fps_ = 0.0f;
    float lastTime_ = 0.0f;
};
