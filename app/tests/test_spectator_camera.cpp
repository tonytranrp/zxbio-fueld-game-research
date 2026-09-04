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
