#pragma once

#include "engine/core/Types.hpp"
#include <functional>
#include <raylib.h>

namespace biofuel::game::gameplay::world3d {

// =============================================================================
// FirstPersonController — reusable WASD + mouse-look + jump character.
//
// Pure state + math: it reads Raylib input each frame and integrates a simple
// kinematic body against a ground-height callback (so it works against any
// terrain source). Owns no resources. Produces a Camera3D for rendering.
// =============================================================================
class FirstPersonController {
public:
    struct Config {
        f32 eyeHeight = 1.75f;        // camera height above the feet
        f32 moveSpeed = 8.0f;         // metres / second (walk)
        f32 sprintMultiplier = 1.9f;  // LEFT-SHIFT speed boost
        f32 jumpSpeed = 8.5f;         // initial upward velocity on jump
        f32 gravity = 24.0f;          // metres / second^2
        f32 mouseSensitivity = 0.0025f;
        f32 pitchLimit = 1.50f;       // radians (~86 deg) up/down clamp
    };

    // Ground height (world Y of the surface) at a world (x, z).
    using HeightFn = std::function<f32(f32 worldX, f32 worldZ)>;

    void setConfig(const Config& config) noexcept { m_config = config; }
    [[nodiscard]] const Config& config() const noexcept { return m_config; }

    // Place the player with their feet at the given world position and reset
    // look/velocity state.
    void reset(Vector3 feetPosition) noexcept;

    // Advance one frame: mouse-look, horizontal movement, gravity, jump, and
    // ground snapping via the supplied height function.
    void update(f32 dt, const HeightFn& groundHeightAt) noexcept;

    [[nodiscard]] Camera3D camera() const noexcept;
    [[nodiscard]] Vector3 feetPosition() const noexcept { return m_position; }
    [[nodiscard]] Vector3 eyePosition() const noexcept;
    [[nodiscard]] bool grounded() const noexcept { return m_grounded; }
    [[nodiscard]] f32 yaw() const noexcept { return m_yaw; }
    [[nodiscard]] f32 speed() const noexcept { return m_horizontalSpeed; }

private:
    Config m_config{};
    Vector3 m_position{0.0f, 0.0f, 0.0f};   // feet position
    f32 m_yaw = 0.0f;                        // radians
    f32 m_pitch = 0.0f;                      // radians
    f32 m_verticalVelocity = 0.0f;
    f32 m_horizontalSpeed = 0.0f;            // last frame's ground speed (for HUD)
    bool m_grounded = false;
};

} // namespace biofuel::game::gameplay::world3d
