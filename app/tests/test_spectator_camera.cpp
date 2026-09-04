#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "spectator_camera.hpp"

using app::SpectatorCameraState;
using app::update_spectator_camera;
using engine::ecs::Transform;
using engine::input::InputState;

namespace {

constexpr float kEps = 1e-4f;

bool approx(const glm::vec3& a, const glm::vec3& b) {
    return glm::length(a - b) < kEps;
}

} // namespace

TEST_CASE("Movement integrates against delta time, so speed does not couple to framerate", "[camera]") {
    InputState input;
    input.move_forward = true;

    // Same wall-clock second as 1x1.0s vs 60x(1/60)s must land within float noise of each other.
    Transform oneStep;
    SpectatorCameraState stateOne;
    update_spectator_camera(oneStep, stateOne, input, {0.0f, 0.0f}, 1.0f);

    Transform manySteps;
    SpectatorCameraState stateMany;
    for (int i = 0; i < 60; ++i) {
        update_spectator_camera(manySteps, stateMany, input, {0.0f, 0.0f}, 1.0f / 60.0f);
    }

    CHECK(glm::length(oneStep.position - manySteps.position) < 1e-3f);
    // Identity orientation looks down -Z: forward motion is exactly -Z at move_speed voxels/s.
    CHECK(approx(oneStep.position, {0.0f, 0.0f, -stateOne.move_speed}));
}

TEST_CASE("Yaw turns the movement basis; 180 degrees of cursor-right reverses forward", "[camera]") {
    InputState input;
    input.move_forward = true;

    Transform transform;
    SpectatorCameraState state;
    state.look_sensitivity = glm::radians(1.0f); // 1 degree per pixel: pixel counts become degrees
    update_spectator_camera(transform, state, input, {180.0f, 0.0f}, 0.0f); // look only, no time
    update_spectator_camera(transform, state, input, {0.0f, 0.0f}, 1.0f);  // then move

    CHECK(approx(transform.position, {0.0f, 0.0f, +state.move_speed})); // now flying +Z
}

TEST_CASE("Looking straight up cannot pass the pitch clamp and forward stays well-defined", "[camera]") {
    InputState input;
    Transform transform;
    SpectatorCameraState state;
    state.look_sensitivity = glm::radians(1.0f);

    // Drag the cursor up (negative y) by far more than 90 degrees' worth.
    update_spectator_camera(transform, state, input, {0.0f, -100000.0f}, 0.0f);
    CHECK(state.pitch_radians < glm::radians(90.0f));
    CHECK(state.pitch_radians > glm::radians(89.0f));

    // Forward is nearly straight up but never degenerate against world up -- flying forward while
    // pitched at the clamp must still move (the ±90° failure a spectator camera hits constantly).
    input.move_forward = true;
    update_spectator_camera(transform, state, input, {0.0f, 0.0f}, 1.0f);
    CHECK(transform.position.y > state.move_speed * 0.99f);
    CHECK(std::abs(transform.position.z) > 0.0f); // still has a horizontal component: not collapsed onto +Y
}

TEST_CASE("Diagonal movement is normalized and boost multiplies speed", "[camera]") {
    InputState input;
    input.move_forward = true;
    input.move_right = true;

    Transform diagonal;
    SpectatorCameraState state;
    update_spectator_camera(diagonal, state, input, {0.0f, 0.0f}, 1.0f);
    CHECK(std::abs(glm::length(diagonal.position) - state.move_speed) < kEps); // not sqrt(2) * speed

    input.speed_boost = true;
    Transform boosted;
    SpectatorCameraState boostedState;
    update_spectator_camera(boosted, boostedState, input, {0.0f, 0.0f}, 1.0f);
    CHECK(std::abs(glm::length(boosted.position) - state.move_speed * app::kSpectatorBoostFactor) < 1e-3f);
}

TEST_CASE("Opposed inputs cancel to no movement without dividing by zero", "[camera]") {
    InputState input;
    input.move_forward = true;
    input.move_back = true;

    Transform transform;
    SpectatorCameraState state;
    update_spectator_camera(transform, state, input, {0.0f, 0.0f}, 1.0f);
    CHECK(approx(transform.position, {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("Walk mode falls under gravity and rests exactly at ground plus eye height", "[camera][walk]") {
    // TERRAIN_FIXES_BRIEF Group V task 24's check: dropped from height, comes to rest at the
    // queried ground height -- not through it, not hovering above it.
    InputState input;
    Transform transform;
    transform.position = {0.0f, 50.0f, 0.0f};
    SpectatorCameraState state;
    state.mode = app::CameraMoveMode::Walk;
    constexpr float kGround = 12.5f;

    for (int i = 0; i < 600; ++i) { // 10 simulated seconds at 60Hz -- far beyond the fall time
        update_spectator_camera(transform, state, input, {0.0f, 0.0f}, 1.0f / 60.0f, kGround);
        CHECK(transform.position.y >= kGround + app::kEyeHeight - 1e-3f); // never tunnels through
    }
    CHECK(std::abs(transform.position.y - (kGround + app::kEyeHeight)) < 1e-3f);
    CHECK(state.vertical_velocity == 0.0f);
}

TEST_CASE("Walk mode survives one huge dt step without tunneling", "[camera][walk]") {
    // A stutter frame (exactly what Group T is about) must not let physics integrate through the
    // ground: one 0.5s step from just above the surface.
    InputState input;
    Transform transform;
    transform.position = {0.0f, 5.0f, 0.0f};
    SpectatorCameraState state;
    state.mode = app::CameraMoveMode::Walk;

    update_spectator_camera(transform, state, input, {0.0f, 0.0f}, 0.5f, 0.0f);
    CHECK(transform.position.y >= app::kEyeHeight - 1e-3f);
    CHECK(state.vertical_velocity == 0.0f);
}

TEST_CASE("Walk mode moves along yaw only and ignores vertical inputs", "[camera][walk]") {
    InputState input;
    input.move_forward = true;
    input.move_up = true;   // Space must be inert on the ground...
    input.move_down = true; // ...and so must Ctrl

    Transform transform;
    transform.position = {0.0f, app::kEyeHeight, 0.0f}; // standing on flat ground at y=0
    SpectatorCameraState state;
    state.mode = app::CameraMoveMode::Walk;
    state.pitch_radians = glm::radians(-80.0f); // staring at the ground must not slow walking

    update_spectator_camera(transform, state, input, {0.0f, 0.0f}, 1.0f, 0.0f);
    // Full walk speed along -Z (yaw 0), zero sideways drift, still standing at eye height.
    CHECK(std::abs(transform.position.z - (-state.move_speed * app::kWalkSpeedFactor)) < 1e-3f);
    CHECK(std::abs(transform.position.x) < 1e-4f);
    CHECK(std::abs(transform.position.y - app::kEyeHeight) < 1e-3f);
}

TEST_CASE("Fly mode is untouched by the walk fields", "[camera][walk]") {
    // Regression guard for the mode split: default-constructed state (Fly) with the new ground
    // parameter supplied must behave exactly as before -- ground is ignored in Fly.
    InputState input;
    input.move_down = true;
    Transform transform;
    transform.position = {0.0f, 1.0f, 0.0f};
    SpectatorCameraState state;

    update_spectator_camera(transform, state, input, {0.0f, 0.0f}, 1.0f, 1000.0f);
    CHECK(std::abs(transform.position.y - (1.0f - state.move_speed)) < 1e-3f); // flew straight down
}
