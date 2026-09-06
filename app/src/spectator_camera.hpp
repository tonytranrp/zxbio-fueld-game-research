#pragma once

#include "engine/core/math.hpp"
#include "engine/ecs/components.hpp"
#include "engine/input/input_state.hpp"
#include "world/materials/materials.hpp"

namespace app {

// Per-camera movement-policy state for the free-flying spectator (task 19). Yaw/pitch are the
// *controller's* working values -- the ECS Transform still stores a quaternion, per
// Phase 1 brief §6. Rebuilding orientation = yaw-about-world-up * pitch-about-local-right each
// update is gimbal-safe (lock needs three chained axes; this uses two in fixed order), stays
// well-defined at exactly ±90° pitch, keeps the horizon level (no roll drift from composed
// increments), and gives the clamp below a number to clamp -- extracting pitch back out of a
// quaternion each frame is the fragile alternative this avoids.
// TERRAIN_FIXES_BRIEF Group V task 22, the scope decision restated where the code lives: the
// camera gains an optional WALK mode (gravity + ground clamp) alongside fly mode. Still no
// player model or character mesh -- just physical presence for the existing camera; the analytic
// ground query stands in for a physics engine (Jolt is the named, deferred upgrade path -- see
// research/terrain-fixes-log.md).
enum class CameraMoveMode { Fly, Walk };

struct SpectatorCameraState {
    float yaw_radians = 0.0f;         // 0 looks down world -Z; positive turns left (right-handed about +Y)
    float pitch_radians = 0.0f;       // positive looks up; clamped to just short of ±90°
    float move_speed = 40.0f;         // voxels/second; kBoostFactor while speed_boost is held
    float look_sensitivity = 0.0025f; // radians per cursor pixel
    CameraMoveMode mode = CameraMoveMode::Fly;
    float vertical_velocity = 0.0f; // walk mode only; world units/second, negative = falling
};

inline constexpr float kSpectatorBoostFactor = 4.0f;
inline constexpr float kWalkSpeedFactor = 0.25f; // walking is deliberately slower than flying
// Swimming (goal 79): passive buoyancy replaces the old walk-on-water sea clamp. Fully submerged
// feet get double-gravity upthrust, so the equilibrium (upthrust*depth == gravity) floats the
// feet kSwimEquilibriumDepth under the surface with the eyes above water; drag damps the bob.
// The numbers are the Water material's own (world/materials/defs/water.hpp, Group AC); the sea
// level is a world constant -- the camera knows where the water is, the material says how it swims.
inline constexpr float kSeaLevelWorld = 0.0f;
inline constexpr world::materials::LiquidPhysics kWaterPhysics =
    world::materials::properties_of(world::materials::MaterialID::Water).liquid;
inline constexpr float kBuoyancyAcceleration = kWaterPhysics.buoyancy_acceleration; // at >=1 voxel submersion
inline constexpr float kWaterDrag = kWaterPhysics.drag;                             // exponential damping
inline constexpr float kSwimEquilibriumDepth = kWaterPhysics.swim_equilibrium_depth; // feet rest here
inline constexpr float kGravityAcceleration = -32.0f; // world units/s^2 (voxel-scale gravity)
inline constexpr float kEyeHeight = 1.7f;             // camera above the ground surface when standing

// The body the collision sweep moves (docs/goals.md Group AA): an upright box whose feet sit
// kEyeHeight under the camera. 0.6 m wide, 1.75 m tall -- the eye is 5 cm under the top.
inline constexpr float kBodyHalfWidth = 0.3f;
inline constexpr float kBodyHeight = 1.75f;
inline constexpr float kStepHeight = 0.55f; // walk mode climbs ledges up to this (fly mode: none)

// The motion this frame WANTS, before the world has a say: orientation is applied to the
// transform, the vertical velocity is integrated (gravity/buoyancy in walk mode), and the wanted
// displacement is returned without moving the position. The app feeds it to
// world::collision::move_and_slide; update_spectator_camera below is the collision-free legacy
// path (tests, --noclip).
struct SpectatorStep {
    glm::vec3 delta{0.0f};
};
SpectatorStep compute_spectator_step(engine::ecs::Transform& transform, SpectatorCameraState& state,
                                     const engine::input::InputState& input, glm::vec2 lookDeltaPixels,
                                     float dtSeconds, float groundHeightWorld) noexcept;

// Walk mode's analytic floor: never let the eye end under ground height + kEyeHeight. With
// collision on this is a backstop that should never fire (the voxel top the sweep lands on is at
// or above the analytic surface); without it, it is the whole ground model.
void clamp_to_ground(engine::ecs::Transform& transform, SpectatorCameraState& state,
                     float groundHeightWorld) noexcept;

// Delta-time-integrated update (Phase 1 brief §6: speed must not couple to framerate). Pure
// function of plain data -- unit-testable without GLFW or a window; the app's frame loop is just
// glue around this. groundHeightWorld: the terrain surface height at the camera's current (x,z)
// column (from the same height function that generated the terrain); only read in Walk mode.
// Equivalent to compute_spectator_step + applying the delta + clamp_to_ground.
void update_spectator_camera(engine::ecs::Transform& transform, SpectatorCameraState& state,
                             const engine::input::InputState& input, glm::vec2 lookDeltaPixels,
                             float dtSeconds, float groundHeightWorld = 0.0f) noexcept;

} // namespace app
