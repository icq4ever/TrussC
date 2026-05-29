#pragma once

// =============================================================================
// tcxDepthTypes.h - Common value types for depth cameras
// =============================================================================
//
// These are plain data structs shared by the DepthCamera interface and all
// device implementations. Keeping per-device differences (resolution, optics,
// sensor kind) as DATA rather than as separate class shapes is what lets a
// single DepthCamera interface span structured-light / stereo / ToF / LiDAR
// without an awkward inheritance tree.
//
// =============================================================================

#include <TrussC.h>

namespace tcx {

using namespace tc;

// What kind of sensor produced the depth. Informational only - never branch on
// this to decide which methods exist; use capability interfaces + as<>() for
// that (see tcxDepthCast.h). This is for UI, logging and debugging.
enum class DepthSensorType {
    Unknown,
    StructuredLight,  // e.g. Kinect v1, Orbbec Astra
    Stereo,           // e.g. Orbbec Gemini, RealSense D4xx
    ToF,              // e.g. Kinect Azure, Orbbec Femto
    LiDAR,            // scanning / solid-state LiDAR
};

// Pinhole intrinsics of the depth stream, in pixel units.
//
// World coordinates are derived from a depth sample d (meters) at pixel (x, y):
//   X = (x - cx) * d / fx
//   Y = (y - cy) * d / fy
//   Z = d
//
// Distortion coefficients follow the Brown-Conrady (plumb-bob) model and are
// optional; leave them at 0 for an ideal pinhole. Most depth SDKs already hand
// back undistorted depth, so they are usually unused here.
struct DepthIntrinsics {
    int   width  = 0;
    int   height = 0;
    float fx = 0.0f;
    float fy = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;
    float k1 = 0.0f, k2 = 0.0f, k3 = 0.0f;  // radial distortion
    float p1 = 0.0f, p2 = 0.0f;             // tangential distortion
};

// Identifies a stream for per-stream freshness queries.
enum class Stream {
    Depth,
    Color,
    Infrared,
    Confidence,
};

// Which streams were refreshed by a single frame capture. Used by
// ThreadedDepthCameraBase to drive isFrameNew() / isColorFrameNew() / etc. so
// implementations never have to track or reset freshness flags themselves.
struct StreamFreshness {
    bool depth      = false;
    bool color      = false;
    bool infrared   = false;
    bool confidence = false;

    void clear() { depth = color = infrared = confidence = false; }

    // Accumulate (OR) - lets a background grabber fold several captures that
    // happened between two update() calls into a single "something new" result.
    StreamFreshness& operator|=(const StreamFreshness& o) {
        depth      |= o.depth;
        color      |= o.color;
        infrared   |= o.infrared;
        confidence |= o.confidence;
        return *this;
    }

    bool any() const { return depth || color || infrared || confidence; }

    bool get(Stream s) const {
        switch (s) {
            case Stream::Depth:      return depth;
            case Stream::Color:      return color;
            case Stream::Infrared:   return infrared;
            case Stream::Confidence: return confidence;
        }
        return false;
    }
};

// Options for DepthCamera::toMesh(). All attributes are built in a single pass
// over the depth image, so invalid points are simply skipped and the optional
// attributes (texCoords / colors) stay aligned with the kept vertices.
//
// texCoords / colors require the camera to also implement IColorStream
// (see tcxDepthCapabilities.h); they are silently ignored otherwise.
struct DepthMeshOptions {
    // Add UVs into the color image. Cheap (the device already knows the
    // depth->color mapping). Render the resulting POINTS mesh with the color
    // texture bound. This does NOT bake color into the vertices.
    bool texCoords = false;

    // Sample and bake an RGB color per vertex. More expensive, but the mesh is
    // self-contained (no texture needed) - handy for obj/gltf export.
    bool colors = false;

    // Decimation: keep every Nth pixel on each axis (1 = full resolution).
    // A cheap way to thin a ~300k-point cloud down for preview / perf.
    int step = 1;
};

} // namespace tcx
