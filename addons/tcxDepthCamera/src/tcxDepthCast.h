#pragma once

// =============================================================================
// tcxDepthCast.h - Capability queries for DepthCamera
// =============================================================================
//
// A device advertises optional features by inheriting capability interfaces
// (tcxDepthCapabilities.h). The single source of truth for "does this camera
// have feature X" is therefore whether it inherits interface X - so we query
// it with a dynamic_cast rather than a hand-maintained bool flag (which could
// drift out of sync with the inheritance list).
//
// as<Cap>() does the check AND hands back the usable pointer in one cast:
//
//   if (auto* s = as<IStereoRaw>(*cam)) {
//       auto& left = s->getLeftInfrared();
//   }
//
// is<Cap>() is the bool-only form, and there are readable named wrappers
// (isStereoCam, hasColor, ...) built on top - all free functions, so adding a
// new capability never touches the DepthCamera base class.
//
// =============================================================================

#include "tcxDepthCameraBase.h"
#include <memory>

namespace tcx {

using namespace tc;

// -----------------------------------------------------------------------------
// as<Cap>(cam) -> Cap* (nullptr if the camera lacks the capability)
// -----------------------------------------------------------------------------
template <class Cap>
Cap* as(DepthCamera& cam) { return dynamic_cast<Cap*>(&cam); }

template <class Cap>
const Cap* as(const DepthCamera& cam) { return dynamic_cast<const Cap*>(&cam); }

template <class Cap>
Cap* as(DepthCamera* cam) { return cam ? dynamic_cast<Cap*>(cam) : nullptr; }

template <class Cap>
Cap* as(const std::shared_ptr<DepthCamera>& cam) {
    return cam ? dynamic_cast<Cap*>(cam.get()) : nullptr;
}

// -----------------------------------------------------------------------------
// is<Cap>(cam) -> bool
// -----------------------------------------------------------------------------
template <class Cap>
bool is(const DepthCamera& cam) { return dynamic_cast<const Cap*>(&cam) != nullptr; }

template <class Cap>
bool is(const std::shared_ptr<DepthCamera>& cam) {
    return cam && dynamic_cast<const Cap*>(cam.get()) != nullptr;
}

// -----------------------------------------------------------------------------
// Readable named wrappers (free functions - base class stays untouched)
// -----------------------------------------------------------------------------
inline bool isStereoCam(const DepthCamera& c) { return is<IStereoRaw>(c); }
inline bool hasColor(const DepthCamera& c)    { return is<IColorStream>(c); }
inline bool hasInfrared(const DepthCamera& c) { return is<IInfraredStream>(c); }
inline bool hasConfidence(const DepthCamera& c) { return is<IConfidenceMap>(c); }

// Sensor kind is informational, not a capability - report it from the enum.
inline bool isToF(const DepthCamera& c) { return c.getSensorType() == DepthSensorType::ToF; }

} // namespace tcx
