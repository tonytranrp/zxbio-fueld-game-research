#pragma once

#include "engine/core/math.hpp"
#include "engine/ecs/components.hpp"
#include "engine/input/input_state.hpp"

namespace app {

// Per-camera movement-policy state for the free-flying spectator (task 19). Yaw/pitch are the
// *controller's* working values -- the ECS Transform still stores a quaternion, per
// PHASE_1_BRIEF.md §6. Rebuilding orientation = yaw-about-world-up * pitch-about-local-right each
// update is gimbal-safe (lock needs three chained axes; this uses two in fixed order), stays
// well-defined at exactly ±90° pitch, keeps the horizon level (no roll drift from composed
// increments), and gives the clamp below a number to clamp -- extracting pitch back out of a
// quaternion each frame is the fragile alternative this avoids.
struct SpectatorCameraState {
    float yaw_radians = 0.0f;   // 0 looks down world -Z; positive turns left (right-handed about +Y)
    float pitch_radians = 0.0f; // positive looks up; clamped to just short of ±90°
    float move_speed = 40.0f;   // voxels/second; kBoostFactor while speed_boost is held
    float look_sensitivity = 0.0025f; // radians per cursor pixel
};

inline constexpr float kSpectatorBoostFactor = 4.0f;

// Delta-time-integrated update (PHASE_1_BRIEF.md §6: speed must not couple to framerate). Pure
// function of plain data -- unit-testable without GLFW or a window; the app's frame loop is just
// glue around this.
void update_spectator_camera(engine::ecs::Transform& transform, SpectatorCameraState& state,
                             const engine::input::InputState& input, glm::vec2 lookDeltaPixels,
                             float dtSeconds) noexcept;

} // namespace app
