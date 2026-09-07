#pragma once

#include "engine/core/math.hpp"
#include "engine/ecs/components.hpp"
#include "engine/input/input_state.hpp"
#include "world/player/controller.hpp"
#include "world/player/player_state.hpp"
#include "world/player/tuning.hpp"

namespace app {

// Per-camera CONTROL state for the spectator (task 19). Yaw/pitch are the controller's working
// values -- the ECS Transform still stores a quaternion, per Phase 1 brief §6. Rebuilding
// orientation = yaw-about-world-up * pitch-about-local-right each update is gimbal-safe (lock needs
// three chained axes; this uses two in fixed order), stays well-defined at exactly ±90° pitch,
// keeps the horizon level (no roll drift from composed increments), and gives the clamp below a
// number to clamp -- extracting pitch back out of a quaternion each frame is the fragile
// alternative this avoids.
//
// The PHYSICS moved to world/player (Prompt 001 Group AD): stance, gravity, jump, swimming, the
// smoothed eye and the view polish all live in `physics`, at a fixed timestep, tested without a
// window. What is left here is the camera the player points -- angles, sensitivity, speed -- plus
// the glue that turns an InputState into a PlayerIntent.
using CameraMoveMode = world::player::MoveMode;

struct SpectatorCameraState {
    float yaw_radians = 0.0f;         // 0 looks down world -Z; positive turns left (right-handed about +Y)
    float pitch_radians = 0.0f;       // positive looks up; clamped to just short of ±90°
    float move_speed = 40.0f;         // voxels/second; kBoostFactor while speed_boost is held
    float look_sensitivity = 0.0025f; // radians per cursor pixel
    world::player::PlayerState physics;
    world::player::PlayerTuning tuning;
};

// Body/motion constants, re-exported so the app's own call sites keep reading one name. The values
// themselves live in world::player::PlayerTuning now.
inline constexpr float kEyeHeight = world::player::kDefaultTuning.eye_height;
inline constexpr float kBodyHalfWidth = world::player::kDefaultTuning.body_half_width;
inline constexpr float kBodyHeight = world::player::kDefaultTuning.body_height;
inline constexpr float kGravityAcceleration = world::player::kDefaultTuning.gravity;
inline constexpr float kSpectatorBoostFactor = world::player::kDefaultTuning.boost_factor;
inline constexpr float kWalkSpeedFactor = world::player::kDefaultTuning.walk_speed_factor;
inline constexpr float kSeaLevelWorld = world::player::kSeaLevelWorld;
inline constexpr float kSwimEquilibriumDepth = world::player::kWaterPhysics.swim_equilibrium_depth;

// Mouse look -> yaw/pitch -> the transform's orientation. Split out from the physics because it is
// the one part that must run at RENDER cadence, not the fixed timestep: a 144 Hz mouse should not
// be sampled at 60 Hz.
void apply_look(engine::ecs::Transform& transform, SpectatorCameraState& state,
                glm::vec2 lookDeltaPixels) noexcept;

// The InputState -> PlayerIntent mapping, in one place. `jumpEdge` is the take-once Space press;
// pass it true on exactly one fixed tick per frame (see world/player's README).
[[nodiscard]] world::player::PlayerIntent to_intent(const engine::input::InputState& input,
                                                    bool jumpEdge) noexcept;

// One fixed tick against the world. `collider` may be null for --noclip, in which case nothing is
// solid and only the analytic ground backstop applies.
template <world::collision::SolidQuery Q>
world::player::StepResult step_camera(engine::ecs::Transform& transform, SpectatorCameraState& state,
                                      const Q& query, const world::player::PlayerIntent& intent,
                                      const world::player::WorldSense& sense, float dtSeconds) {
    return world::player::step_player(query, state.physics, state.tuning, intent, sense, transform.position,
                                      state.yaw_radians, state.pitch_radians, state.move_speed, dtSeconds);
}

// A world in which nothing is solid: the --noclip path and the collision-free tests, expressed as
// a query instead of as a second copy of the physics.
struct OpenWorld {
    [[nodiscard]] bool overlaps_solid(const world::collision::Aabb&) const noexcept { return false; }
};
static_assert(world::collision::SolidQuery<OpenWorld>);

// Delta-time-integrated update with no collision (Phase 1 brief §6: speed must not couple to
// framerate). Pure function of plain data -- unit-testable without GLFW or a window. Kept as the
// legacy/--noclip entry point; the app's frame loop drives step_camera at a fixed timestep instead.
void update_spectator_camera(engine::ecs::Transform& transform, SpectatorCameraState& state,
                             const engine::input::InputState& input, glm::vec2 lookDeltaPixels,
                             float dtSeconds, float groundHeightWorld = 0.0f) noexcept;

} // namespace app
