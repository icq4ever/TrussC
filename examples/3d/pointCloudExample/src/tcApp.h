#pragma once

#include <TrussC.h>
using namespace std;
using namespace tc;

// Draw a point cloud as a GPU-resident Mesh (PrimitiveMode::Points). All three
// PointStyles are GPU-resident — they differ only in SHAPE:
//   Pixel  - a true 1px GPU point primitive (ignores point size)
//   Square - a billboarded quad splat, sized by setPointSize
//   Round  - an anti-aliased disc splat
//
//   G       cycle shape (Square / Round / Pixel)
//   1..6    point count (50k / 100k / 250k / 500k / 1M / 2M)
//   [ ]     point size in logical px (Square/Round only)
//   drag    orbit
class tcApp : public App {
public:
    void setup() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void rebuild(int n);

    EasyCam view;
    Mesh    cloud;                 // static point cloud (rebuilt only on N change)

    int        n_ = 250000;
    PointStyle style_ = PointStyle::Square;
    float      pointSize_ = 3.0f;  // logical px
};
