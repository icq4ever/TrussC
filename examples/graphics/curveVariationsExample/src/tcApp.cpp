// =============================================================================
// curveVariationsExample - tcApp.cpp
//
// Six panels showing different ways to compose curves out of the new
// drawCurve / drawBezier / Path primitives.
// =============================================================================

#include "tcApp.h"
#include <cmath>

void tcApp::setup() {
    setIndependentFps(VSYNC, VSYNC);
    setCurveTolerance(0.1f);
}

void tcApp::update() {}

void tcApp::draw() {
    clear(0.10f, 0.11f, 0.13f);

    // 3 columns x 2 rows layout
    const float colW = 400, rowH = 360;
    drawSection_basic       (0,        0);
    drawSection_chain       (colW,     0);
    drawSection_loop        (colW * 2, 0);
    drawSection_pathCurveTo (0,        rowH);
    drawSection_bezierLoop  (colW,     rowH);
    drawSection_animated    (colW * 2, rowH);
}

// ---------------------------------------------------------------------------
// 1. Basic 4-point Catmull-Rom (reference)
// ---------------------------------------------------------------------------
void tcApp::drawSection_basic(float x, float y) {
    label("1. drawCurve(p0, p1, p2, p3)", x + 20, y + 30);
    label("  - curve interpolates p1 -> p2 only", x + 20, y + 50);

    vector<Vec3> pts = {
        Vec3{x + 50,  y + 230, 0},   // p0 (tangent)
        Vec3{x + 130, y + 130, 0},   // p1 (start of visible curve)
        Vec3{x + 260, y + 270, 0},   // p2 (end of visible curve)
        Vec3{x + 350, y + 150, 0},   // p3 (tangent)
    };
    setColor(0.85f, 0.95f, 0.4f);
    drawCurve(pts[0], pts[1], pts[2], pts[3]);

    // Mark control points; p1/p2 are filled (the curve passes through them),
    // p0/p3 are ring-only (tangent influence only).
    setColor(1.0f, 1.0f, 1.0f, 0.85f);
    drawCircle(pts[1].x, pts[1].y, 4);
    drawCircle(pts[2].x, pts[2].y, 4);
    noFill();
    drawCircle(pts[0].x, pts[0].y, 5);
    drawCircle(pts[3].x, pts[3].y, 5);
    fill();
}

// ---------------------------------------------------------------------------
// 2. drawCurve(vector) - chain through N points
// ---------------------------------------------------------------------------
void tcApp::drawSection_chain(float x, float y) {
    label("2. drawCurve(vector<Vec3>)", x + 20, y + 30);
    label("  - passes through all interior points", x + 20, y + 50);

    vector<Vec3> pts = {
        Vec3{x + 30,  y + 200, 0},
        Vec3{x + 80,  y + 270, 0},
        Vec3{x + 150, y + 150, 0},
        Vec3{x + 220, y + 230, 0},
        Vec3{x + 290, y + 130, 0},
        Vec3{x + 360, y + 200, 0},
    };

    setColor(0.4f, 0.85f, 0.95f);
    drawCurve(pts);

    setColor(1.0f, 1.0f, 1.0f, 0.85f);
    drawDots(pts);
}

// ---------------------------------------------------------------------------
// 3. drawCurve(vector, closed) - loop
// ---------------------------------------------------------------------------
void tcApp::drawSection_loop(float x, float y) {
    label("3. drawCurve(vector, closed=true)", x + 20, y + 30);
    label("  - smooth wraparound, organic blob", x + 20, y + 50);

    // Petal-like closed loop
    const float cx = x + 200, cy = y + 200;
    vector<Vec3> pts;
    const int n = 7;
    for (int i = 0; i < n; i++) {
        float a = (float)i / n * TAU - QUARTER_TAU;
        float r = (i & 1) ? 50.0f : 110.0f;  // alternating radius
        pts.push_back(Vec3{cx + cos(a) * r, cy + sin(a) * r, 0});
    }

    setColor(0.95f, 0.45f, 0.7f);
    drawCurve(pts, /*closed=*/true);

    setColor(1.0f, 1.0f, 1.0f, 0.6f);
    drawDots(pts, 3);
}

// ---------------------------------------------------------------------------
// 4. Path::curveTo() accumulator
// ---------------------------------------------------------------------------
void tcApp::drawSection_pathCurveTo(float x, float y) {
    label("4. Path::curveTo() accumulator", x + 20, y + 30);
    label("  - feeds Path one point at a time", x + 20, y + 50);

    vector<Vec3> pts = {
        Vec3{x + 40,  y + 150, 0},
        Vec3{x + 100, y + 250, 0},
        Vec3{x + 170, y + 130, 0},
        Vec3{x + 240, y + 260, 0},
        Vec3{x + 310, y + 130, 0},
        Vec3{x + 370, y + 240, 0},
    };

    Path path;
    for (auto& p : pts) path.curveTo(p);
    setColor(0.95f, 0.7f, 0.3f);
    noFill();
    path.draw();
    fill();

    setColor(1.0f, 1.0f, 1.0f, 0.5f);
    drawDots(pts, 3);
}

// ---------------------------------------------------------------------------
// 5. Cubic Bezier loop - self-intersecting
// ---------------------------------------------------------------------------
void tcApp::drawSection_bezierLoop(float x, float y) {
    label("5. cubic drawBezier(loop)", x + 20, y + 30);
    label("  - control polygon crosses, curve loops", x + 20, y + 50);

    Vec3 p0{x + 80,  y + 250, 0};
    Vec3 p1{x + 350, y + 100, 0};   // far up-right
    Vec3 p2{x + 50,  y + 100, 0};   // back-far-left (creates loop)
    Vec3 p3{x + 320, y + 250, 0};

    // Faint control polygon
    setColor(1.0f, 1.0f, 1.0f, 0.25f);
    drawLine(p0.x, p0.y, p1.x, p1.y);
    drawLine(p1.x, p1.y, p2.x, p2.y);
    drawLine(p2.x, p2.y, p3.x, p3.y);

    setColor(0.7f, 0.5f, 0.9f);
    drawBezier(p0, p1, p2, p3);

    setColor(1.0f, 1.0f, 1.0f, 0.85f);
    drawCircle(p0.x, p0.y, 4);
    drawCircle(p3.x, p3.y, 4);
    noFill();
    drawCircle(p1.x, p1.y, 5);
    drawCircle(p2.x, p2.y, 5);
    fill();
}

// ---------------------------------------------------------------------------
// 6. Animated morph - closed Catmull-Rom with time-varying radii
// ---------------------------------------------------------------------------
void tcApp::drawSection_animated(float x, float y) {
    label("6. animated closed curve", x + 20, y + 30);
    label("  - radii oscillate over time", x + 20, y + 50);

    const float cx = x + 200, cy = y + 200;
    const float t = getElapsedTimef();
    vector<Vec3> pts;
    const int n = 8;
    for (int i = 0; i < n; i++) {
        float a = (float)i / n * TAU;
        // Per-vertex radius oscillation with a phase offset
        float r = 80.0f + sin(t * 1.5f + i * 0.6f) * 35.0f;
        pts.push_back(Vec3{cx + cos(a) * r, cy + sin(a) * r, 0});
    }

    setColor(0.4f, 0.9f, 0.6f);
    drawCurve(pts, /*closed=*/true);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void tcApp::drawDots(const vector<Vec3>& pts, float r) {
    for (auto& p : pts) drawCircle(p.x, p.y, r);
}

void tcApp::label(const string& s, float x, float y) {
    setColor(1.0f, 1.0f, 1.0f, 0.85f);
    drawBitmapString(s, x, y);
}
