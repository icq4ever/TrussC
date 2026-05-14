#pragma once

#include <TrussC.h>
#include <vector>
using namespace std;
using namespace tc;

// =============================================================================
// curveVariationsExample
//
// Catmull-Rom (drawCurve) and Bezier variations beyond the basic 4-point form.
//
//   1. drawCurve(p0,p1,p2,p3)        — single segment (reference)
//   2. drawCurve(vector<Vec3>)       — chain through N points
//   3. drawCurve(vector<Vec3>, true) — closed loop, wraparound tangents
//   4. Path::curveTo() polyline      — accumulator-style spline
//   5. Cubic Bezier loop             — self-intersection demo
//   6. Animated morph                — time-varying control points
// =============================================================================

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

private:
    void drawSection_basic(float x, float y);
    void drawSection_chain(float x, float y);
    void drawSection_loop(float x, float y);
    void drawSection_pathCurveTo(float x, float y);
    void drawSection_bezierLoop(float x, float y);
    void drawSection_animated(float x, float y);

    void drawDots(const vector<Vec3>& pts, float r = 3.5f);
    void label(const string& s, float x, float y);
};
