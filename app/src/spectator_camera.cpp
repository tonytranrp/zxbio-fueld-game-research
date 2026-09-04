#include <algorithm>

#include "spectator_camera.hpp"

namespace app {

void update_spectator_camera(engine::ecs::Transform& transform, SpectatorCameraState& state,
                             const engine::input::InputState& input, glm::vec2 lookDeltaPixels,
                             float dtSeconds) noexcept {
    // Mouse look: cursor right (+x) turns right (negative yaw about +Y), cursor down (+y) looks
    // down (negative pitch). Clamp shy of ±90° so forward never exactly degenerates onto world up.
    state.yaw_radians -= lookDeltaPixels.x * state.look_sensitivity;
    state.pitch_radians -= lookDeltaPixels.y * state.look_sensitivity;
    constexpr float kPitchLimit = glm::radians(89.9f);
    state.pitch_radians = std::clamp(state.pitch_radians, -kPitchLimit, kPitchLimit);

    transform.orientation = glm::angleAxis(state.yaw_radians, glm::vec3(0.0f, 1.0f, 0.0f)) *
                            glm::angleAxis(state.pitch_radians, glm::vec3(1.0f, 0.0f, 0.0f));

    // Move along the view axes (full free-flight: "forward" includes pitch), vertical strafe along
    // world up -- the standard spectator scheme. Normalized so diagonals aren't faster.
    const glm::vec3 forward = transform.orientation * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 right = transform.orientation * glm::vec3(1.0f, 0.0f, 0.0f);
    constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};

    glm::vec3 wish{0.0f};
    if (input.move_forward) wish += forward;
    if (input.move_back) wish -= forward;
    if (input.move_right) wish += right;
    if (input.move_left) wish -= right;
    if (input.move_up) wish += kWorldUp;
    if (input.move_down) wish -= kWorldUp;

    const float wishLength = glm::length(wish);
    if (wishLength > 0.0f) {
        const float speed = state.move_speed * (input.speed_boost ? kSpectatorBoostFactor : 1.0f);
        transform.position += wish / wishLength * speed * dtSeconds;
    }
}

} // namespace app
