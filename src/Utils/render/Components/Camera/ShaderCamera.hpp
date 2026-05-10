#pragma once

#include "Core/Types.hpp"
#include "AnimationController/animation/Easing.hpp"

namespace biofuel::utils::render::component {

// ------------------------------------------------------------------------------
// ShaderCameraState — raw camera parameters passed to shaders
//
// These values are applied as offsets/rotations in the shader's raymarcher:
//   offsetX/Y → lateral camera position shift
//   yaw       → horizontal look rotation (radians, positive = look right)
//   pitch     → vertical look rotation (radians, positive = look up)
// ------------------------------------------------------------------------------
struct ShaderCameraState {
    f32 offsetX = 0.0f;
    f32 offsetY = 0.0f;
    f32 yaw     = 0.0f;
    f32 pitch   = 0.0f;

    // Lerp between two states
    [[nodiscard]] static ShaderCameraState lerp(
        const ShaderCameraState& a,
        const ShaderCameraState& b,
        f32 t) noexcept
    {
        return ShaderCameraState{
            .offsetX = a.offsetX + (b.offsetX - a.offsetX) * t,
            .offsetY = a.offsetY + (b.offsetY - a.offsetY) * t,
            .yaw     = a.yaw     + (b.yaw     - a.yaw)     * t,
            .pitch   = a.pitch   + (b.pitch   - a.pitch)    * t,
        };
    }
};

// ------------------------------------------------------------------------------
// ShaderCameraController — smoothly animates camera state over time
//
// Usage:
//   1. Create an instance as a member of your screen class
//   2. Call setTarget() to start an animation towards a new state
//   3. Call update(dt) each frame
//   4. Read current() to get interpolated values for shader uniforms
//
// Example:
//   m_camera.setTarget(
//       ShaderCameraState{.offsetX = 5.0f, .yaw = -0.3f},
//       2.0f,
//       animation::Easing::easeInOutCubic
//   );
//   // ... in update:
//   m_camera.update(dt);
//   // ... in render:
//   backdrop.setFloat("uCameraYaw", m_camera.current().yaw);
// ------------------------------------------------------------------------------
class ShaderCameraController {
public:
    // Reset to identity (no offset, no rotation, no animation)
    void reset() noexcept;

    // Start animating from the current state towards `target` over `duration` seconds.
    // Uses the provided easing function for interpolation.
    void setTarget(
        const ShaderCameraState& target,
        f32 duration,
        animation::Easing::Fn easing = animation::Easing::easeInOutCubic) noexcept;

    // Advance animation by dt seconds.
    void update(f32 dt) noexcept;

    // Current interpolated camera state (use this to set shader uniforms).
    [[nodiscard]] const ShaderCameraState& current() const noexcept;

    // True while an animation is in progress.
    [[nodiscard]] bool isAnimating() const noexcept;

    // True when the last setTarget() animation has fully completed.
    [[nodiscard]] bool isComplete() const noexcept;

private:
    ShaderCameraState m_current{};
    ShaderCameraState m_from{};
    ShaderCameraState m_to{};
    f32 m_elapsed  = 0.0f;
    f32 m_duration = 0.0f;
    bool m_animating = false;
    animation::Easing::Fn m_easing = animation::Easing::easeInOutCubic;
};

} // namespace biofuel::utils::render::component
