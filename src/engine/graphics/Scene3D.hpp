#pragma once

#include <raylib.h>
#include <rlgl.h>

namespace biofuel::engine::graphics {

// RAII wrapper for BeginMode3D/EndMode3D, matching the ScopedShaderMode/
// ScopedTextureMode idiom in Render.hpp.
class ScopedMode3D final {
public:
    explicit ScopedMode3D(const Camera3D camera) noexcept { BeginMode3D(camera); }
    ~ScopedMode3D() noexcept { EndMode3D(); }

    ScopedMode3D(const ScopedMode3D&) = delete;
    ScopedMode3D& operator=(const ScopedMode3D&) = delete;
    ScopedMode3D(ScopedMode3D&&) = delete;
    ScopedMode3D& operator=(ScopedMode3D&&) = delete;
};

// Temporarily overrides rlgl's near/far clip distances, restoring the
// previous values on scope exit. Used to compress the viewmodel pass's depth
// range so it can never clip into world geometry regardless of world scale.
class ScopedClipPlanes final {
public:
    ScopedClipPlanes(const double nearPlane, const double farPlane) noexcept
        : m_previousNear(rlGetCullDistanceNear()), m_previousFar(rlGetCullDistanceFar()) {
        rlSetClipPlanes(nearPlane, farPlane);
    }
    ~ScopedClipPlanes() noexcept { rlSetClipPlanes(m_previousNear, m_previousFar); }

    ScopedClipPlanes(const ScopedClipPlanes&) = delete;
    ScopedClipPlanes& operator=(const ScopedClipPlanes&) = delete;
    ScopedClipPlanes(ScopedClipPlanes&&) = delete;
    ScopedClipPlanes& operator=(ScopedClipPlanes&&) = delete;

private:
    double m_previousNear;
    double m_previousFar;
};

} // namespace biofuel::engine::graphics
