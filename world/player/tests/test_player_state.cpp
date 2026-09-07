#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "world/player/player_state.hpp"

using namespace world::player;

namespace {
constexpr float kDt = 1.0f / 60.0f;
}

TEST_CASE("Walk-mode wish ignores pitch; fly-mode wish uses it", "[player][state]") {
    PlayerTuning tuning;
    PlayerIntent intent;
    intent.forward = true;

    // Looking 45 degrees down while walking must not shorten the horizontal step -- the classic
    // "walking slows down when you look at your feet" bug this ordering exists to prevent.
    const glm::vec3 walk = wish_velocity(intent, tuning, MoveMode::Walk, 0.0f, -0.785f, 40.0f);
    REQUIRE_THAT(walk.y, Catch::Matchers::WithinAbs(0.0, 1.0e-6));
    REQUIRE_THAT(glm::length(walk), Catch::Matchers::WithinRel(40.0f * tuning.walk_speed_factor, 1.0e-5f));

    const glm::vec3 fly = wish_velocity(intent, tuning, MoveMode::Fly, 0.0f, -0.785f, 40.0f);
    REQUIRE(fly.y < -1.0f); // flying forward while looking down descends
    REQUIRE_THAT(glm::length(fly), Catch::Matchers::WithinRel(40.0f, 1.0e-5f));
}

TEST_CASE("Diagonal movement is not faster than straight", "[player][state]") {
    PlayerTuning tuning;
    PlayerIntent straight;
    straight.forward = true;
    PlayerIntent diagonal;
    diagonal.forward = true;
    diagonal.right = true;
    const float a = glm::length(wish_velocity(straight, tuning, MoveMode::Walk, 0.4f, 0.0f, 40.0f));
    const float b = glm::length(wish_velocity(diagonal, tuning, MoveMode::Walk, 0.4f, 0.0f, 40.0f));
    REQUIRE_THAT(a, Catch::Matchers::WithinRel(b, 1.0e-5f));
}

TEST_CASE("Jump apex lands in the 1.0-1.25 m band", "[player][jump]") {
    // Prompt 001 A2's number, pinned here so a gravity or jump-speed change cannot silently move
    // the apex: h = v0^2 / (2|g|).
    const PlayerTuning tuning;
    const float apex = (tuning.jump_speed * tuning.jump_speed) / (2.0f * std::abs(tuning.gravity));
    REQUIRE(apex >= 1.0f);
    REQUIRE(apex <= 1.25f);
}

TEST_CASE("A grounded press jumps immediately", "[player][jump]") {
    PlayerState state;
    state.stance = Stance::Grounded;
    const PlayerTuning tuning;
    const JumpOutcome outcome = update_jump(state, tuning, true, kDt);
    REQUIRE(outcome.jumped);
    REQUIRE(state.stance == Stance::Airborne);
    REQUIRE_THAT(state.vertical_velocity, Catch::Matchers::WithinRel(tuning.jump_speed, 1.0e-5f));
    REQUIRE(state.jump_buffer_remaining == 0.0f); // consumed, not left to fire again
}

TEST_CASE("Coyote time lets a late press jump after leaving a ledge", "[player][jump]") {
    const PlayerTuning tuning;
    PlayerState state;
    state.stance = Stance::Airborne;
    state.coyote_remaining = tuning.coyote_time; // as step_player leaves it on the last grounded tick

    // 0.05 s of falling -- inside the 0.1 s window.
    for (int i = 0; i < 3; ++i) {
        REQUIRE_FALSE(update_jump(state, tuning, false, kDt).jumped);
    }
    REQUIRE(state.coyote_remaining > 0.0f);
    REQUIRE(update_jump(state, tuning, true, kDt).jumped);
}

TEST_CASE("Coyote credit expires", "[player][jump]") {
    const PlayerTuning tuning;
    PlayerState state;
    state.stance = Stance::Airborne;
    state.coyote_remaining = tuning.coyote_time;
    for (int i = 0; i < 10; ++i) { // 0.166 s > 0.1 s
        update_jump(state, tuning, false, kDt);
    }
    REQUIRE(state.coyote_remaining == 0.0f);
    REQUIRE_FALSE(update_jump(state, tuning, true, kDt).jumped);
}

TEST_CASE("A press just before landing is buffered and fires on touchdown", "[player][jump]") {
    const PlayerTuning tuning;
    PlayerState state;
    state.stance = Stance::Airborne;
    state.coyote_remaining = 0.0f;

    REQUIRE_FALSE(update_jump(state, tuning, true, kDt).jumped); // airborne, no credit: buffered
    REQUIRE(state.jump_buffer_remaining > 0.0f);
    for (int i = 0; i < 3; ++i) {
        REQUIRE_FALSE(update_jump(state, tuning, false, kDt).jumped);
    }
    state.stance = Stance::Grounded; // the sweep lands us
    REQUIRE(update_jump(state, tuning, false, kDt).jumped);
}

TEST_CASE("A buffered press older than the window is discarded", "[player][jump]") {
    const PlayerTuning tuning;
    PlayerState state;
    state.stance = Stance::Airborne;
    update_jump(state, tuning, true, kDt);
    for (int i = 0; i < 10; ++i) { // 0.166 s > 0.1 s buffer
        update_jump(state, tuning, false, kDt);
    }
    state.stance = Stance::Grounded;
    REQUIRE_FALSE(update_jump(state, tuning, false, kDt).jumped);
}

TEST_CASE("Swimming: released input floats to the buoyancy equilibrium", "[player][swim]") {
    const PlayerTuning tuning;
    PlayerState state;
    state.stance = Stance::Swimming;
    const PlayerIntent idle;
    // Fully submerged, no input: upthrust (2x gravity) wins and the body rises.
    for (int i = 0; i < 10; ++i) {
        integrate_swim(state, tuning, idle, 1.0f, kDt);
    }
    REQUIRE(state.vertical_velocity > 0.0f);
}

TEST_CASE("Swimming: Ctrl dives against full buoyancy", "[player][swim]") {
    const PlayerTuning tuning;
    PlayerState state;
    state.stance = Stance::Swimming;
    PlayerIntent dive;
    dive.down = true;
    // A direct velocity, not an acceleration -- at full submersion the passive net is +32 m/s^2, so
    // any "swim thrust" small enough to feel human would lose to it (A5's reasoning).
    for (int i = 0; i < 10; ++i) {
        integrate_swim(state, tuning, dive, 1.0f, kDt);
    }
    REQUIRE_THAT(state.vertical_velocity, Catch::Matchers::WithinRel(-tuning.swim_speed, 1.0e-5f));
}

TEST_CASE("Eye smoothing lags a step and then catches up", "[player][smoothing]") {
    const PlayerTuning tuning;
    PlayerState state;
    state.stance = Stance::Grounded;

    // A 4 cm step-up (the svo path's whole budget): the view is left behind by it, never ahead.
    update_eye_smoothing(state, tuning, 1.74f, 1.70f, kDt);
    REQUIRE(state.eye_smooth_offset < 0.0f);
    REQUIRE(std::abs(state.eye_smooth_offset) <= kSvoStepHeight + 1.0e-6f);

    // Standing still, it decays to nothing within a few time constants.
    for (int i = 0; i < 60; ++i) { // 1.0 s = 10 tau
        update_eye_smoothing(state, tuning, 1.74f, 1.74f, kDt);
    }
    REQUIRE(std::abs(state.eye_smooth_offset) < 1.0e-3f);
}

TEST_CASE("Eye smoothing is disabled in the air and hard-clamped", "[player][smoothing]") {
    const PlayerTuning tuning;
    PlayerState state;
    state.stance = Stance::Grounded;
    // An absurd single-tick move (a teleport) must not detach the view: the clamp is the backstop.
    update_eye_smoothing(state, tuning, 100.0f, 0.0f, kDt);
    REQUIRE(std::abs(state.eye_smooth_offset) <= tuning.eye_smooth_max_lag + 1.0e-6f);

    // Leaving the ground collapses the offset outright -- a fall must never trail the body.
    state.stance = Stance::Airborne;
    update_eye_smoothing(state, tuning, 5.0f, 4.0f, kDt);
    REQUIRE(state.eye_smooth_offset == 0.0f);
}
