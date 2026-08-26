#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>

namespace biofuel::engine::character {

// -----------------------------------------------------------------------------
// FirstPersonCamera - owns yaw/pitch/head-bob state for a first-person view and
// converts it to a raylib Camera3D on demand.
//
// Deliberately polls no input itself (unlike raylib's own CAMERA_FIRST_PERSON
// mode, which reads GetMouseDelta()/keys internally and can't be driven from a
// decoupled fixed-timestep loop -- see engine/character/README.md). The owner
// (typically a screen) reads raw mouse delta once per render frame and calls
// addLookDelta(); movement/physics stay entirely separate.
// -----------------------------------------------------------------------------
class FirstPersonCamera {
public:
    // Radians; kept just short of +/-90 degrees to avoid the view flipping
    // through the pole (verified convention: Daggerfall Unity and the official
    // bevy_rapier3d character-controller example both clamp in this range).
    static constexpr f32 kMaxPitchRadians = 1.55334f; // ~89.0 degrees

    void addLookDelta(Vector2 pixelDelta, f32 sensitivity = 0.0025f) noexcept;

    void updateBob(f32 horizontalSpeed, f32 dt) noexcept;

    [[nodiscard]] Vector3 forward() const noexcept;
    // Forward with pitch removed (y = 0, renormalized) -- movement direction
    // should not change speed just because the player is looking up/down.
    [[nodiscard]] Vector3 flatForward() const noexcept;
    [[nodiscard]] Vector3 right() const noexcept;
    // Small camera-local offset from head-bob; add to the eye position before
    // building the render camera. Zero when stationary.
    [[nodiscard]] Vector3 bobOffset() const noexcept;

    [[nodiscard]] Camera3D toCamera3D(Vector3 eye, f32 fovYDegrees = 74.0f) const noexcept;

    [[nodiscard]] f32 yawRadians() const noexcept { return m_yaw; }
    [[nodiscard]] f32 pitchRadians() const noexcept { return m_pitch; }
    void setYawPitch(f32 yawRadians, f32 pitchRadians) noexcept;

private:
    f32 m_yaw = 0.0f;
    f32 m_pitch = 0.0f;
    f32 m_bobPhase = 0.0f;
};

} // namespace biofuel::engine::character
