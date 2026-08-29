#include "FirstPersonCamera.hpp"

#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace biofuel::engine::character {

void FirstPersonCamera::addLookDelta(const Vector2 pixelDelta, const f32 sensitivity) noexcept {
    // Raw mouse delta is already a per-frame distance, not a rate -- do not
    // scale by dt here (that would double-integrate time and make turn rate
    // framerate-dependent). Yaw/pitch are owned scalars, not derived from the
    // camera's position/target vectors, which sidesteps gimbal lock entirely.
    m_yaw -= pixelDelta.x * sensitivity;
    m_pitch -= pixelDelta.y * sensitivity;
    m_pitch = std::clamp(m_pitch, -kMaxPitchRadians, kMaxPitchRadians);
}

void FirstPersonCamera::updateBob(const f32 horizontalSpeed, const f32 dt) noexcept {
    // Phase advances with distance traveled, not wall-clock time, so bob
    // frequency naturally scales with movement speed instead of needing a
    // separate speed->frequency curve.
    constexpr f32 kBobCyclesPerMeter = 1.6f;
    m_bobPhase += horizontalSpeed * dt * kBobCyclesPerMeter * (2.0f * PI);
    if (m_bobPhase > 2.0f * PI) {
        m_bobPhase = std::fmod(m_bobPhase, 2.0f * PI);
    }
}

Vector3 FirstPersonCamera::forward() const noexcept {
    return Vector3{
        std::cos(m_pitch) * std::sin(m_yaw),
        std::sin(m_pitch),
        std::cos(m_pitch) * std::cos(m_yaw),
    };
}

Vector3 FirstPersonCamera::flatForward() const noexcept {
    const Vector3 flat{std::sin(m_yaw), 0.0f, std::cos(m_yaw)};
    return flat; // already unit length: sin^2+cos^2 == 1
}

Vector3 FirstPersonCamera::right() const noexcept {
    // Verified against actual gameplay (A/D felt swapped with the other
    // sign): cos/-sin pointed opposite to this project's rendered "right"
    // relative to flatForward(), even though it matched the abstract
    // right-handed-compass derivation this was originally written from.
    return Vector3{-std::cos(m_yaw), 0.0f, std::sin(m_yaw)};
}

Vector3 FirstPersonCamera::bobOffset() const noexcept {
    // Amplitudes are small fractions of eye height by design (research
    // baseline: GoldSrc's own view-bob clamps to a similarly small band) --
    // this is meant to read as "alive," not as a visible wobble.
    constexpr f32 kVerticalAmplitude = 0.015f;
    constexpr f32 kLateralAmplitude = 0.008f;
    return Vector3{
        std::cos(m_bobPhase * 0.5f) * kLateralAmplitude,
        std::abs(std::sin(m_bobPhase)) * kVerticalAmplitude,
        0.0f,
    };
}

Camera3D FirstPersonCamera::toCamera3D(const Vector3 eye, const f32 fovYDegrees) const noexcept {
    const Vector3 bob = bobOffset();
    const Vector3 bobbedEye{eye.x + bob.x, eye.y + bob.y, eye.z + bob.z};
    const Vector3 fwd = forward();
    return Camera3D{
        .position = bobbedEye,
        .target = Vector3{bobbedEye.x + fwd.x, bobbedEye.y + fwd.y, bobbedEye.z + fwd.z},
        .up = Vector3{0.0f, 1.0f, 0.0f},
        .fovy = fovYDegrees,
        .projection = CAMERA_PERSPECTIVE,
    };
}

void FirstPersonCamera::setYawPitch(const f32 yawRadians, const f32 pitchRadians) noexcept {
    m_yaw = yawRadians;
    m_pitch = std::clamp(pitchRadians, -kMaxPitchRadians, kMaxPitchRadians);
}

} // namespace biofuel::engine::character
