#include <algorithm>
#include <cmath>

#include "spectator_camera.hpp"

namespace app {

SpectatorStep compute_spectator_step(engine::ecs::Transform& transform, SpectatorCameraState& state,
                                     const engine::input::InputState& input, glm::vec2 lookDeltaPixels,
                                     float dtSeconds, float groundHeightWorld) noexcept {
    // Mouse look: cursor right (+x) turns right (negative yaw about +Y), cursor down (+y) looks
    // down (negative pitch). Clamp shy of ±90° so forward never exactly degenerates onto world up.
    state.yaw_radians -= lookDeltaPixels.x * state.look_sensitivity;
    state.pitch_radians -= lookDeltaPixels.y * state.look_sensitivity;
    constexpr float kPitchLimit = glm::radians(89.9f);
    state.pitch_radians = std::clamp(state.pitch_radians, -kPitchLimit, kPitchLimit);

    transform.orientation = glm::angleAxis(state.yaw_radians, glm::vec3(0.0f, 1.0f, 0.0f)) *
                            glm::angleAxis(state.pitch_radians, glm::vec3(1.0f, 0.0f, 0.0f));

    constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};
    SpectatorStep step;

    if (state.mode == CameraMoveMode::Fly) {
        // Move along the view axes (full free-flight: "forward" includes pitch), vertical strafe
        // along world up -- the standard spectator scheme. Normalized so diagonals aren't faster.
        const glm::vec3 forward = transform.orientation * glm::vec3(0.0f, 0.0f, -1.0f);
        const glm::vec3 right = transform.orientation * glm::vec3(1.0f, 0.0f, 0.0f);

        glm::vec3 wish{0.0f};
        if (input.move_forward)
            wish += forward;
        if (input.move_back)
            wish -= forward;
        if (input.move_right)
            wish += right;
        if (input.move_left)
            wish -= right;
        if (input.move_up)
            wish += kWorldUp;
        if (input.move_down)
            wish -= kWorldUp;

        const float wishLength = glm::length(wish);
        if (wishLength > 0.0f) {
            const float speed = state.move_speed * (input.speed_boost ? kSpectatorBoostFactor : 1.0f);
            step.delta = wish / wishLength * speed * dtSeconds;
        }
        return step;
    }

    // Walk mode (TERRAIN_FIXES_BRIEF Group V tasks 24): horizontal movement follows YAW only --
    // looking at the ground must not slow walking -- and the vertical axis belongs to physics:
    // constant gravity acceleration integrated into a velocity the caller's collision (or the
    // legacy ground clamp) resolves. Space/Ctrl are deliberately inert here.
    const glm::vec3 walkForward{-std::sin(state.yaw_radians), 0.0f, -std::cos(state.yaw_radians)};
    const glm::vec3 walkRight{std::cos(state.yaw_radians), 0.0f, -std::sin(state.yaw_radians)};

    glm::vec3 wish{0.0f};
    if (input.move_forward)
        wish += walkForward;
    if (input.move_back)
        wish -= walkForward;
    if (input.move_right)
        wish += walkRight;
    if (input.move_left)
        wish -= walkRight;
    const float wishLength = glm::length(wish);
    if (wishLength > 0.0f) {
        const float speed =
            state.move_speed * kWalkSpeedFactor * (input.speed_boost ? kSpectatorBoostFactor : 1.0f);
        step.delta = wish / wishLength * speed * dtSeconds;
    }

    // Swimming (goal 79): when the feet are below the water surface over a genuinely submerged
    // column, buoyancy opposes gravity -- proportional to submersion up to one voxel, so the
    // equilibrium floats the feet kSwimEquilibriumDepth under the surface (eyes above water) --
    // and drag damps the bob. On land (or once ground rises above sea level) this term is zero
    // and walk physics is exactly what it always was.
    const float feetY = transform.position.y - kEyeHeight;
    const bool inWater = feetY < kSeaLevelWorld && groundHeightWorld < kSeaLevelWorld;
    if (inWater) {
        const float submersion = std::min(kSeaLevelWorld - feetY, 1.0f);
        state.vertical_velocity += (kGravityAcceleration + kBuoyancyAcceleration * submersion) * dtSeconds;
        state.vertical_velocity *= std::exp(-kWaterDrag * dtSeconds);
    } else {
        state.vertical_velocity += kGravityAcceleration * dtSeconds;
    }
    step.delta.y += state.vertical_velocity * dtSeconds;
    return step;
}

void update_spectator_camera(engine::ecs::Transform& transform, SpectatorCameraState& state,
                             const engine::input::InputState& input, glm::vec2 lookDeltaPixels,
                             float dtSeconds, float groundHeightWorld) noexcept {
    const SpectatorStep step =
        compute_spectator_step(transform, state, input, lookDeltaPixels, dtSeconds, groundHeightWorld);
    transform.position += step.delta;
    if (state.mode == CameraMoveMode::Walk) {
        clamp_to_ground(transform, state, groundHeightWorld);
    }
}

void clamp_to_ground(engine::ecs::Transform& transform, SpectatorCameraState& state,
                     float groundHeightWorld) noexcept {
    // The hard floor is the REAL ground (the seabed under water, now that ground_height no longer
    // clamps to sea level) -- swimming floats you above it, but you can still stand in shallows.
    const float standingEyeY = groundHeightWorld + kEyeHeight;
    if (transform.position.y <= standingEyeY) {
        transform.position.y = standingEyeY; // grounded: clamp to the surface...
        state.vertical_velocity = 0.0f;      // ...and kill the fall, never tunnel through
    }
}

} // namespace app
