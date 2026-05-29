#pragma once

// =============================================================================
// tcxDepthCapabilities.h - Optional capability interfaces for depth cameras
// =============================================================================
//
// A depth camera's depth + point-cloud behaviour is universal and lives in the
// DepthCamera base (tcxDepthCameraBase.h). Everything else - a color stream,
// raw stereo IR, a confidence/amplitude map - is OPTIONAL and varies per
// device. We model those as small, independent interfaces here.
//
// Crucially, none of these inherit from DepthCamera. A device composes the ones
// it supports via multiple inheritance:
//
//   class Orbbec : public DepthCamera,
//                  public IColorStream,
//                  public IStereoRaw { ... };
//
// Because only DepthCamera carries implementation/state and the capabilities
// are pure interfaces that don't re-derive it, there is no diamond - it behaves
// like Java's "implements" of several interfaces.
//
// To query a capability at runtime, use as<>() / is<>() from tcxDepthCast.h:
//
//   if (auto* s = as<IStereoRaw>(*cam)) { auto& l = s->getLeftInfrared(); }
//
// =============================================================================

#include "tcxDepthTypes.h"

namespace tcx {

using namespace tc;

// -----------------------------------------------------------------------------
// IColorStream - device has an RGB(A) color camera registered to the depth
// -----------------------------------------------------------------------------
// Provides the depth->color mapping used by DepthCamera::toMesh() when
// texCoords / colors are requested.
class IColorStream {
public:
    virtual ~IColorStream() = default;

    // Color arrives asynchronously from depth, so it has its own freshness
    // signal (true when update() brought in a new color image).
    virtual bool isColorFrameNew() const = 0;

    virtual int getColorWidth()  const = 0;
    virtual int getColorHeight() const = 0;

    // Raw color frame (CPU pixels).
    virtual const Pixels& getColorPixels() const = 0;

    // Normalized (0-1) UV into the color image for the depth pixel (dx, dy).
    // This is the registration: structured-light/stereo devices read it from a
    // lookup table, ToF/Kinect from a calibrated formula - same signature.
    virtual Vec2 getColorTexCoordAt(int dx, int dy) const = 0;

    // Sampled RGBA color for the depth pixel (dx, dy). Default implementation
    // samples getColorPixels() at getColorTexCoordAt(); override if the device
    // exposes an already-registered color frame for a faster path.
    virtual Color getColorAt(int dx, int dy) const {
        const Pixels& px = getColorPixels();
        if (!px.isAllocated()) return Color{0, 0, 0, 1};
        Vec2 uv = getColorTexCoordAt(dx, dy);
        int cx = static_cast<int>(uv.x * px.getWidth());
        int cy = static_cast<int>(uv.y * px.getHeight());
        if (cx < 0 || cy < 0 || cx >= px.getWidth() || cy >= px.getHeight()) {
            return Color{0, 0, 0, 1};
        }
        return px.getColor(cx, cy);
    }
};

// -----------------------------------------------------------------------------
// IInfraredStream - device exposes an IR / amplitude image
// -----------------------------------------------------------------------------
// One IR image (the active/structured-light view, or a ToF amplitude image).
// Stereo devices that expose a left/right pair should use IStereoRaw instead.
class IInfraredStream {
public:
    virtual ~IInfraredStream() = default;

    virtual bool isInfraredFrameNew() const = 0;
    virtual int getInfraredWidth()  const = 0;
    virtual int getInfraredHeight() const = 0;
    virtual const Pixels& getInfraredPixels() const = 0;
};

// -----------------------------------------------------------------------------
// IStereoRaw - device is a stereo pair and can hand back both raw views
// -----------------------------------------------------------------------------
class IStereoRaw {
public:
    virtual ~IStereoRaw() = default;

    virtual const Pixels& getLeftInfrared()  const = 0;
    virtual const Pixels& getRightInfrared() const = 0;

    // Stereo baseline in meters (distance between the two sensors).
    virtual float getBaseline() const = 0;
};

// -----------------------------------------------------------------------------
// IConfidenceMap - per-pixel confidence / amplitude (typical of ToF)
// -----------------------------------------------------------------------------
// Useful for masking out flying pixels / low-confidence depth.
class IConfidenceMap {
public:
    virtual ~IConfidenceMap() = default;

    virtual const Pixels& getConfidencePixels() const = 0;

    // Normalized 0-1 confidence at a depth pixel (1 = most reliable).
    virtual float getConfidenceAt(int dx, int dy) const = 0;
};

} // namespace tcx
