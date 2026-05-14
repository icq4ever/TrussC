#pragma once

// This file is included from TrussC.h

// Disable Windows GDI Path macro
#ifdef _WIN32
#undef Path
#endif

#include <vector>
#include <cmath>
#include <deque>

namespace trussc {

// Path - Class that holds vertex array (with curve generation functionality)
class Path {
public:
    Path() : closed_(false) {}

    // Constructor (from vertex list)
    Path(const std::vector<Vec2>& verts) : closed_(false) {
        for (const auto& v : verts) {
            vertices_.push_back(Vec3{v.x, v.y, 0.0f});
        }
    }

    Path(const std::vector<Vec3>& verts) : vertices_(verts), closed_(false) {}

    // Add vertex
    void addVertex(float x, float y) {
        vertices_.push_back(Vec3{x, y, 0.0f});
    }

    void addVertex(float x, float y, float z) {
        vertices_.push_back(Vec3{x, y, z});
    }

    void addVertex(const Vec2& v) {
        addVertex(v.x, v.y);
    }

    void addVertex(const Vec3& v) {
        vertices_.push_back(v);
    }

    // Add multiple vertices at once
    void addVertices(const std::vector<Vec2>& verts) {
        for (const auto& v : verts) {
            addVertex(v);
        }
    }

    void addVertices(const std::vector<Vec3>& verts) {
        for (const auto& v : verts) {
            addVertex(v);
        }
    }

    // Get vertices
    const std::vector<Vec3>& getVertices() const {
        return vertices_;
    }

    std::vector<Vec3>& getVertices() {
        return vertices_;
    }

    // Vertex count
    int size() const {
        return static_cast<int>(vertices_.size());
    }

    bool empty() const {
        return vertices_.empty();
    }

    // Access specific vertex
    Vec3& operator[](int index) {
        return vertices_[index];
    }

    const Vec3& operator[](int index) const {
        return vertices_[index];
    }

    // Clear
    void clear() {
        vertices_.clear();
        curveVertices_.clear();
        closed_ = false;
    }

    // =========================================================================
    // Adding lines and curves
    // =========================================================================

    // lineTo is an alias for addVertex
    void lineTo(float x, float y, float z = 0) {
        addVertex(x, y, z);
    }

    void lineTo(const Vec2& p) {
        addVertex(p);
    }

    void lineTo(const Vec3& p) {
        addVertex(p);
    }

    // Cubic Bezier curve
    // -----------------------------------------------------------------------
    // bezierTo / quadBezierTo / curveTo
    //
    // Default behaviour (no `resolution` arg, or resolution < 0): inherit
    // the current CurveStyle. In Tolerance mode the curve is adaptively
    // subdivided so the polyline stays within the configured pixel
    // tolerance (scale-aware). In Resolution mode it falls back to N
    // uniform t-samples.
    //
    // Pass an explicit positive `resolution` to force N uniform t-samples
    // regardless of the CurveStyle (the original behaviour — kept so
    // existing callers that asked for, say, resolution=64 still get
    // exactly 64 samples).
    //
    // NOTE: this changes the default of `resolution` from 20 to -1 (=
    // sentinel for "use CurveStyle"). Existing `path.bezierTo(c1, c2, t)`
    // calls now produce smoother curves at small radii and may emit a
    // different vertex count than before — visually equivalent or better.
    // -----------------------------------------------------------------------

    // Cubic Bezier — cp1, cp2 control points, `to` endpoint.
    void bezierTo(const Vec3& cp1, const Vec3& cp2, const Vec3& to, int resolution = -1) {
        if (vertices_.empty()) vertices_.push_back(Vec3{0, 0, 0});
        Vec3 p0 = vertices_.back();

        if (resolution < 0) {
            const auto& ctx = getDefaultContext();
            if (ctx.getCurveMode() == CurveStyle::Mode::Tolerance) {
                std::vector<Vec3> tmp;
                tessellateCubicBezier(p0, cp1, cp2, to,
                                      ctx.getCurveTolerance() / ctx.getCurrentScale(),
                                      tmp);
                // tmp[0] == p0 — already at vertices_.back(), skip it.
                for (size_t i = 1; i < tmp.size(); i++) vertices_.push_back(tmp[i]);
                return;
            }
            resolution = std::max(2, ctx.getCurveResolution());
        }
        for (int i = 1; i <= resolution; i++) {
            float t  = (float)i / resolution;
            float t2 = t * t,  t3 = t2 * t;
            float mt = 1 - t,  mt2 = mt * mt, mt3 = mt2 * mt;
            vertices_.push_back(Vec3{
                mt3*p0.x + 3*mt2*t*cp1.x + 3*mt*t2*cp2.x + t3*to.x,
                mt3*p0.y + 3*mt2*t*cp1.y + 3*mt*t2*cp2.y + t3*to.y,
                mt3*p0.z + 3*mt2*t*cp1.z + 3*mt*t2*cp2.z + t3*to.z});
        }
    }

    void bezierTo(float cx1, float cy1, float cx2, float cy2, float x, float y, int resolution = -1) {
        bezierTo(Vec3{cx1, cy1, 0}, Vec3{cx2, cy2, 0}, Vec3{x, y, 0}, resolution);
    }
    void bezierTo(const Vec2& cp1, const Vec2& cp2, const Vec2& to, int resolution = -1) {
        bezierTo(Vec3{cp1.x, cp1.y, 0}, Vec3{cp2.x, cp2.y, 0}, Vec3{to.x, to.y, 0}, resolution);
    }

    // Quadratic Bezier — cp control point, `to` endpoint.
    void quadBezierTo(const Vec3& cp, const Vec3& to, int resolution = -1) {
        if (vertices_.empty()) vertices_.push_back(Vec3{0, 0, 0});
        Vec3 p0 = vertices_.back();

        if (resolution < 0) {
            const auto& ctx = getDefaultContext();
            if (ctx.getCurveMode() == CurveStyle::Mode::Tolerance) {
                std::vector<Vec3> tmp;
                tessellateQuadBezier(p0, cp, to,
                                     ctx.getCurveTolerance() / ctx.getCurrentScale(),
                                     tmp);
                for (size_t i = 1; i < tmp.size(); i++) vertices_.push_back(tmp[i]);
                return;
            }
            resolution = std::max(2, ctx.getCurveResolution());
        }
        for (int i = 1; i <= resolution; i++) {
            float t  = (float)i / resolution;
            float t2 = t * t;
            float mt = 1 - t, mt2 = mt * mt;
            vertices_.push_back(Vec3{
                mt2*p0.x + 2*mt*t*cp.x + t2*to.x,
                mt2*p0.y + 2*mt*t*cp.y + t2*to.y,
                mt2*p0.z + 2*mt*t*cp.z + t2*to.z});
        }
    }

    void quadBezierTo(float cx, float cy, float x, float y, int resolution = -1) {
        quadBezierTo(Vec3{cx, cy, 0}, Vec3{x, y, 0}, resolution);
    }
    void quadBezierTo(const Vec2& cp, const Vec2& to, int resolution = -1) {
        quadBezierTo(Vec3{cp.x, cp.y, 0}, Vec3{to.x, to.y, 0}, resolution);
    }

    // Catmull-Rom spline. Each consecutive curveTo() emits the segment
    // between the previous-1 and current points using the previous-2 and
    // next as tangent influences (i.e. you must call curveTo at least 4
    // times before any geometry appears). When `resolution < 0`, the
    // segment is converted to its equivalent cubic Bezier and adaptively
    // subdivided per CurveStyle.
    void curveTo(const Vec3& to, int resolution = -1) {
        curveVertices_.push_back(to);
        if (curveVertices_.size() < 4) return;

        size_t n = curveVertices_.size();
        const Vec3& p0 = curveVertices_[n - 4];
        const Vec3& p1 = curveVertices_[n - 3];
        const Vec3& p2 = curveVertices_[n - 2];
        const Vec3& p3 = curveVertices_[n - 1];

        if (vertices_.empty() ||
            vertices_.back().x != p1.x || vertices_.back().y != p1.y || vertices_.back().z != p1.z) {
            vertices_.push_back(p1);
        }

        if (resolution < 0) {
            const auto& ctx = getDefaultContext();
            if (ctx.getCurveMode() == CurveStyle::Mode::Tolerance) {
                // Convert Catmull-Rom (uniform) segment to its cubic-Bezier
                // equivalent (B1 = p1 + (p2-p0)/6, B2 = p2 - (p3-p1)/6) and
                // reuse the same adaptive tessellator the rest of the
                // plumbing depends on.
                Vec3 b1{p1.x + (p2.x - p0.x) / 6.0f,
                        p1.y + (p2.y - p0.y) / 6.0f,
                        p1.z + (p2.z - p0.z) / 6.0f};
                Vec3 b2{p2.x - (p3.x - p1.x) / 6.0f,
                        p2.y - (p3.y - p1.y) / 6.0f,
                        p2.z - (p3.z - p1.z) / 6.0f};
                std::vector<Vec3> tmp;
                tessellateCubicBezier(p1, b1, b2, p2,
                                      ctx.getCurveTolerance() / ctx.getCurrentScale(),
                                      tmp);
                for (size_t i = 1; i < tmp.size(); i++) vertices_.push_back(tmp[i]);
                return;
            }
            resolution = std::max(2, ctx.getCurveResolution());
        }
        for (int i = 1; i <= resolution; i++) {
            float t = (float)i / resolution;
            vertices_.push_back(catmullRom(p0, p1, p2, p3, t));
        }
    }

    void curveTo(float x, float y, float z = 0, int resolution = -1) {
        curveTo(Vec3{x, y, z}, resolution);
    }

    void curveTo(const Vec2& to, int resolution = -1) {
        curveTo(Vec3{to.x, to.y, 0}, resolution);
    }

    // Arc
    void arc(const Vec3& center, float radiusX, float radiusY,
             float angleBegin, float angleEnd, bool clockwise = true, int circleResolution = 20) {

        // Degrees to radians
        float startRad = angleBegin * TAU / 360.0f;
        float endRad = angleEnd * TAU / 360.0f;

        float diff = endRad - startRad;

        // Adjust segment count based on angle size
        int segments = std::max(2, (int)(std::abs(diff) / TAU * circleResolution));

        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments;
            float angle = clockwise ? (startRad + diff * t) : (startRad - diff * t);

            Vec3 p;
            p.x = center.x + std::cos(angle) * radiusX;
            p.y = center.y + std::sin(angle) * radiusY;
            p.z = center.z;

            vertices_.push_back(p);
        }
    }

    void arc(float x, float y, float radiusX, float radiusY,
             float angleBegin, float angleEnd, int circleResolution = 20) {
        arc(Vec3{x, y, 0}, radiusX, radiusY, angleBegin, angleEnd, true, circleResolution);
    }

    void arc(const Vec2& center, float radiusX, float radiusY,
             float angleBegin, float angleEnd, int circleResolution = 20) {
        arc(Vec3{center.x, center.y, 0}, radiusX, radiusY, angleBegin, angleEnd, true, circleResolution);
    }

    // -----------------------------------------------------------------------
    // Circular arc — radians, tolerance-aware (uses current CurveStyle).
    //
    // Disambiguated from the elliptical overloads above by parameter count:
    // these take a single `radius` instead of `radiusX, radiusY`. Angles
    // are in **radians** (matches TrussC's overall convention; the older
    // overloads take degrees for backward-compat reasons).
    // -----------------------------------------------------------------------
    void arc(const Vec3& center, float radius,
             float angleBegin, float angleEnd, bool clockwise = true) {
        if (radius <= 0.0f) return;
        float diff = angleEnd - angleBegin;
        if (!clockwise) diff = -diff;
        if (diff == 0.0f) return;
        int segments = getDefaultContext().decideArcSegments(radius, std::abs(diff));
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / (float)segments;
            float a = angleBegin + diff * t;
            vertices_.push_back(Vec3{
                center.x + std::cos(a) * radius,
                center.y + std::sin(a) * radius,
                center.z
            });
        }
    }
    void arc(float x, float y, float radius,
             float angleBegin, float angleEnd, bool clockwise = true) {
        arc(Vec3{x, y, 0}, radius, angleBegin, angleEnd, clockwise);
    }
    void arc(const Vec2& center, float radius,
             float angleBegin, float angleEnd, bool clockwise = true) {
        arc(Vec3{center.x, center.y, 0}, radius, angleBegin, angleEnd, clockwise);
    }

    // Close/Open path
    void close() {
        closed_ = true;
    }

    void setClosed(bool closed) {
        closed_ = closed;
    }

    bool isClosed() const {
        return closed_;
    }

    // Draw
    void draw() const {
        if (vertices_.empty()) return;

        size_t n = vertices_.size();
        auto& ctx = getDefaultContext();
        Color col = ctx.getColor();

        // Fill mode: triangle fan (only convex shapes render correctly)
        if (ctx.isFillEnabled() && n >= 3) {
            sgl_begin_triangles();
            sgl_c4f(col.r, col.g, col.b, col.a);
            for (size_t i = 1; i < n - 1; i++) {
                sgl_v3f(vertices_[0].x, vertices_[0].y, vertices_[0].z);
                sgl_v3f(vertices_[i].x, vertices_[i].y, vertices_[i].z);
                sgl_v3f(vertices_[i+1].x, vertices_[i+1].y, vertices_[i+1].z);
            }
            sgl_end();
        }

        // Stroke mode: line strip
        if (ctx.isStrokeEnabled() && n >= 2) {
            sgl_c4f(col.r, col.g, col.b, col.a);
            sgl_begin_line_strip();
            for (size_t i = 0; i < n; i++) {
                sgl_v3f(vertices_[i].x, vertices_[i].y, vertices_[i].z);
            }
            if (closed_ && n > 2) {
                sgl_v3f(vertices_[0].x, vertices_[0].y, vertices_[0].z);
            }
            sgl_end();
        }
    }

    // Draw the path as a thick stroke (StrokeMesh, respects strokeWeight /
    // strokeCap / strokeJoin). Use draw() for 1-pixel line rendering.
    void drawStroke() const {
        if (vertices_.empty()) return;
        beginStroke();
        for (const auto& v : vertices_) vertex(v);
        endStroke(closed_);
    }

    // Get bounding box as Rect
    Rect getBounds() const {
        if (vertices_.empty()) {
            return Rect{0, 0, 0, 0};
        }
        float minX = vertices_[0].x, maxX = vertices_[0].x;
        float minY = vertices_[0].y, maxY = vertices_[0].y;
        for (const auto& v : vertices_) {
            if (v.x < minX) minX = v.x;
            if (v.x > maxX) maxX = v.x;
            if (v.y < minY) minY = v.y;
            if (v.y > maxY) maxY = v.y;
        }
        return Rect{minX, minY, maxX - minX, maxY - minY};
    }

    // Calculate length (line length)
    float getPerimeter() const {
        if (vertices_.size() < 2) return 0;
        float len = 0;
        for (size_t i = 1; i < vertices_.size(); i++) {
            float dx = vertices_[i].x - vertices_[i-1].x;
            float dy = vertices_[i].y - vertices_[i-1].y;
            float dz = vertices_[i].z - vertices_[i-1].z;
            len += sqrt(dx*dx + dy*dy + dz*dz);
        }
        if (closed_ && vertices_.size() > 2) {
            float dx = vertices_[0].x - vertices_.back().x;
            float dy = vertices_[0].y - vertices_.back().y;
            float dz = vertices_[0].z - vertices_.back().z;
            len += sqrt(dx*dx + dy*dy + dz*dz);
        }
        return len;
    }

private:
    std::vector<Vec3> vertices_;
    std::deque<Vec3> curveVertices_;  // Buffer for curveTo
    bool closed_;

    // Catmull-Rom spline interpolation
    static Vec3 catmullRom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;

        // Catmull-Rom matrix
        Vec3 result;
        result.x = 0.5f * ((2 * p1.x) +
                          (-p0.x + p2.x) * t +
                          (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 +
                          (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
        result.y = 0.5f * ((2 * p1.y) +
                          (-p0.y + p2.y) * t +
                          (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 +
                          (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
        result.z = 0.5f * ((2 * p1.z) +
                          (-p0.z + p2.z) * t +
                          (2 * p0.z - 5 * p1.z + 4 * p2.z - p3.z) * t2 +
                          (-p0.z + 3 * p1.z - 3 * p2.z + p3.z) * t3);
        return result;
    }
};

} // namespace trussc
