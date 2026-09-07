#include <algorithm>
#include <cmath>

#include "spectator_camera.hpp"

namespace app {

void apply_look(engine::ecs::Transform& transform, SpectatorCameraState& state,
                glm::vec2 lookDeltaPixels) noexcept {
    // Cursor right (+x) turns right (negative yaw about +Y), cursor down (+y) looks down (negative
    // pitch). Clamp shy of ±90° so forward never exactly degenerates onto world up.
    state.yaw_radians -= lookDeltaPixels.x * state.look_sensitivity;
    state.pitch_radians -= lookDeltaPixels.y * state.look_sensitivity;
    constexpr float kPitchLimit = glm::radians(89.9f);
    state.pitch_radians = std::clamp(state.pitch_radians, -kPitchLimit, kPitchLimit);

    transform.orientation = glm::angleAxis(state.yaw_radians, glm::vec3(0.0f, 1.0f, 0.0f)) *
                            glm::angleAxis(state.pitch_radians, glm::vec3(1.0f, 0.0f, 0.0f));
}

world::player::PlayerIntent to_intent(const engine::input::InputState& input, bool jumpEdge) noexcept {
    world::player::PlayerIntent intent;
    intent.forward = input.move_forward;
    intent.back = input.move_back;
    intent.left = input.move_left;
    intent.right = input.move_right;
    intent.up = input.move_up;
    intent.down = input.move_down;
    intent.boost = input.speed_boost;
    intent.jump_pressed = jumpEdge;
    return intent;
}

void update_spectator_camera(engine::ecs::Transform& transform, SpectatorCameraState& state,
                             const engine::input::InputState& input, glm::vec2 lookDeltaPixels,
                             float dtSeconds, float groundHeightWorld) noexcept {
    apply_look(transform, state, lookDeltaPixels);
    world::player::WorldSense sense;
    sense.ground_height = groundHeightWorld;
    // Space is a jump here too, so --noclip walk mode behaves like the collided one. The level is
    // used as the edge on this path: it has no frame-to-frame memory to derive one from, and the
    // state machine's own buffer consumption keeps a held key from repeating.
    const world::player::PlayerIntent intent = to_intent(input, input.move_up);
    const OpenWorld open;
    step_camera(transform, state, open, intent, sense, dtSeconds);
}

} // namespace app
