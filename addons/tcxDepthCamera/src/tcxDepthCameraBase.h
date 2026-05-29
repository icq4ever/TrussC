#pragma once

// =============================================================================
// tcxDepthCameraBase.h - Unified interface for depth / point-cloud cameras
// =============================================================================
//
// DepthCamera is the common contract shared by every depth sensor, regardless
// of how it produces depth (structured light, stereo, ToF, LiDAR). Drive any
// device through this one interface:
//
//   shared_ptr<DepthCamera> cam = make_shared<Orbbec>(serial);
//   cam->setup();
//   ...
//   cam->update();
//   if (cam->isFrameNew()) {
//       Mesh cloud = cam->toMesh();                       // points only
//       Mesh tex   = cam->toMesh({.texCoords = true});    // + UVs for color tex
//   }
//
// Design notes:
//   - The depth array natively holds DISTANCE only; turning it into 3D points
//     or color is a separate, on-demand calculation. So the per-pixel getters
//     (getDistanceAt / getWorldCoordinateAt) are the primitives, and toMesh()
//     is the convenience layer that runs the full conversion in one pass.
//   - Units are float meters everywhere in this abstract API (1.0 == 1m, like
//     glTF / Unity). The raw uint16-mm buffer, when a device exposes it, is a
//     device-specific fast path, deliberately NOT part of this interface - that
//     keeps the API future-proof for LiDAR ranges that overflow uint16 mm.
//   - Optional streams (color, IR, stereo raw, confidence) are capability
//     interfaces (tcxDepthCapabilities.h), composed via multiple inheritance
//     and discovered with as<>() / is<>() (tcxDepthCast.h).
//
// =============================================================================

#include "tcxDepthCapabilities.h"

namespace tcx {

using namespace tc;

class DepthCamera {
public:
    virtual ~DepthCamera() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    // Open the device and start streaming. Returns false on failure.
    virtual bool setup() = 0;

    // Pull the latest frame from the device. Call once per app frame.
    virtual void update() = 0;

    // Stop streaming and release the device.
    virtual void close() = 0;

    // True only on frames where update() brought in a new DEPTH image. This is
    // the primary, every-camera freshness signal. Other streams arrive
    // asynchronously and report their own freshness on their capability
    // interface (IColorStream::isColorFrameNew(), etc.).
    virtual bool isFrameNew() const = 0;

    // -------------------------------------------------------------------------
    // Threading (optional behaviour switch)
    // -------------------------------------------------------------------------
    //
    // When threaded, the implementation grabs frames on a background thread and
    // update() just publishes the most recent one - non-blocking, double
    // buffered, at the cost of up to one frame of latency. When not threaded,
    // update() acquires the frame inline (lowest latency, but can stall the
    // app loop on slow USB / device I/O).
    //
    // Call before setup(). Not every SDK supports toggling this (some are
    // inherently callback-driven); isThreaded() reports the effective mode.
    // The base only stores the requested flag - honoring it is up to the
    // implementation. Default: not threaded.
    virtual void setThreaded(bool threaded) { threaded_ = threaded; }
    virtual bool isThreaded() const { return threaded_; }

    // -------------------------------------------------------------------------
    // Depth stream
    // -------------------------------------------------------------------------

    virtual int getWidth()  const = 0;
    virtual int getHeight() const = 0;

    // Distance along the optical axis at depth pixel (x, y), in meters.
    // Returns <= 0 for invalid / missing depth (the universal "no data" value).
    virtual float getDistanceAt(int x, int y) const = 0;

    // Pinhole intrinsics of the depth stream (drives the default deprojection).
    virtual DepthIntrinsics getDepthIntrinsics() const = 0;

    // Informational sensor kind. Defaults to Unknown; override to report it.
    virtual DepthSensorType getSensorType() const { return DepthSensorType::Unknown; }

    // -------------------------------------------------------------------------
    // Deprojection (point cloud building blocks)
    // -------------------------------------------------------------------------

    // 3D position of depth pixel (x, y) in camera space, meters. Returns the
    // origin for invalid depth. Default implementation is the textbook pinhole
    // deprojection using getDepthIntrinsics(); override with the SDK's own
    // (often bulk / SIMD) routine when one exists.
    //
    // Convention: +X right, +Y down (image axes), +Z away from the camera.
    virtual Vec3 getWorldCoordinateAt(int x, int y) const {
        float d = getDistanceAt(x, y);
        if (d <= 0.0f) return Vec3{0, 0, 0};
        DepthIntrinsics in = getDepthIntrinsics();
        if (in.fx == 0.0f || in.fy == 0.0f) return Vec3{0, 0, d};
        return Vec3{
            (static_cast<float>(x) - in.cx) * d / in.fx,
            (static_cast<float>(y) - in.cy) * d / in.fy,
            d
        };
    }

    // -------------------------------------------------------------------------
    // Mesh / point cloud (convenience layer)
    // -------------------------------------------------------------------------

    // Build a point-cloud Mesh (PrimitiveMode::Points) from the current frame.
    // Invalid points are skipped, so optional attributes stay aligned with the
    // kept vertices - that's why we build everything in one pass here rather
    // than decorating a mesh after the fact (post-decoration loses the depth
    // pixel mapping once invalid points are dropped).
    //
    // Override this when the device offers a faster bulk conversion; the
    // default is a correct, portable reference implementation.
    virtual Mesh toMesh(DepthMeshOptions opt = {}) const {
        Mesh m;
        m.setMode(PrimitiveMode::Points);

        const int step = (opt.step < 1) ? 1 : opt.step;
        const int w = getWidth();
        const int h = getHeight();

        // Color attributes need a registered color stream; degrade gracefully
        // (the mesh is still built, just without the requested color/UVs).
        const IColorStream* color = (opt.texCoords || opt.colors)
            ? dynamic_cast<const IColorStream*>(this)
            : nullptr;
        if ((opt.texCoords || opt.colors) && !color) {
            static bool warned = false;
            if (!warned) {
                warned = true;
                logWarning("tcxDepthCamera")
                    << "toMesh() requested color/texCoords but this camera has "
                       "no IColorStream; building geometry without them.";
            }
        }

        for (int y = 0; y < h; y += step) {
            for (int x = 0; x < w; x += step) {
                if (getDistanceAt(x, y) <= 0.0f) continue;  // skip invalid
                m.addVertex(getWorldCoordinateAt(x, y));
                if (color && opt.texCoords) m.addTexCoord(color->getColorTexCoordAt(x, y));
                if (color && opt.colors)    m.addColor(color->getColorAt(x, y));
            }
        }
        return m;
    }

protected:
    // Requested threading mode (see setThreaded). Implementations read this in
    // setup() to decide whether to spin up a background grabber.
    bool threaded_ = false;
};

} // namespace tcx
