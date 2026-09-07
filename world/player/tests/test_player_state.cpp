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
    // Goal 232: the walking path ignores `moveSpeed` entirely -- that 40 is the FLY camera's
    // number, and the product of it that used to define walking is exactly what was wrong.
    REQUIRE_THAT(glm::length(walk), Catch::Matchers::WithinRel(tuning.walk_speed, 1.0e-5f));

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
    // It used to be exactly EQUAL (normalise, then one speed). With the directional penalties of
    // goal 232 a forward-right diagonal is half lateral, so it is legitimately slower -- but it
    // must still never be FASTER, which is the bug this case was written for.
    REQUIRE(b <= a);
    REQUIRE(b > 0.85f * a); // and the penalty is a penalty, not a different gear
}

TEST_CASE("Jump apex is the 0.6 m goal 233 chose", "[player][jump]") {
    // Prompt 001 A2 pinned this at the 1.0-1.25 m band, which was the apex of an 8.5 m/s jump under
    // 3.26x Earth gravity. Goal 233 moved BOTH numbers together to Earth scale, so this band moved
    // with them -- the case survives because its job is unchanged: h = v0^2 / (2|g|), pinned so a
    // change to either constant cannot silently move the apex.
    //
    // A real standing vertical is 0.4-0.5 m at a ~3 m/s takeoff. 0.6 m is a deliberate departure
    // upward -- a 0.45 m jump does not read on screen -- and 3.43 m/s is within 15% of the human
    // takeoff velocity, which is the part that matters for how it looks.
    const PlayerTuning tuning;
    const float apex = (tuning.jump_speed * tuning.jump_speed) / (2.0f * std::abs(tuning.gravity));
    REQUIRE_THAT(apex, Catch::Matchers::WithinAbs(0.60, 0.02));
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

// --- goal 232: the speeds are the research's, and a test says which band ------------------------

TEST_CASE("Ground speeds sit in the bands the research names", "[player][speed]") {
    const PlayerTuning tuning;
    // research/locomotion-biomechanics-physics.md 2.1: self-selected human walking 1.39 m/s,
    // commonly 1.3-1.5. If someone "just bumps it", this fails and they have to move the evidence
    // with it -- which is the entire point of pinning a band rather than a value.
    CHECK(tuning.walk_speed >= 1.3f);
    CHECK(tuning.walk_speed <= 1.5f);
    // Same file: fit-human sprint 6-8 m/s. Bolt's 12.32 is deliberately NOT the target.
    CHECK(tuning.sprint_speed >= 6.0f);
    CHECK(tuning.sprint_speed <= 8.0f);
    // The walk->run transition (Froude ~0.5) is 1.89-2.16 m/s: a sprint must be past it and a walk
    // must not be, or the names are lies.
    CHECK(tuning.walk_speed < 1.89f);
    CHECK(tuning.sprint_speed > 2.16f);
    // research/player-movement-in-games.md 5.2.3 boxes elite human sprint acceleration at ~10 m/s^2.
    CHECK(tuning.ground_accel <= 10.0f);
    CHECK(tuning.ground_decel >= tuning.ground_accel); // you can plant a foot
}

TEST_CASE("Directional penalties follow ARMA's shipped values", "[player][speed]") {
    const PlayerTuning tuning;
    CHECK_THAT(directional_speed_factor(1.0f, 0.0f, tuning), Catch::Matchers::WithinRel(1.0f, 1.0e-5f));
    CHECK_THAT(directional_speed_factor(-1.0f, 0.0f, tuning),
               Catch::Matchers::WithinRel(tuning.back_speed_factor, 1.0e-5f));
    CHECK_THAT(directional_speed_factor(0.0f, 1.0f, tuning),
               Catch::Matchers::WithinRel(tuning.lateral_speed_factor, 1.0e-5f));
    CHECK_THAT(directional_speed_factor(0.0f, -1.0f, tuning),
               Catch::Matchers::WithinRel(tuning.lateral_speed_factor, 1.0e-5f));
    // Both measured penalties sit inside the human 70-80% band the research reports.
    CHECK(tuning.back_speed_factor >= 0.70f);
    CHECK(tuning.back_speed_factor <= 0.80f);
    CHECK(tuning.lateral_speed_factor >= 0.70f);
    CHECK(tuning.lateral_speed_factor <= 0.80f);
    // Smooth, not four discrete states: every diagonal lies between the two extremes it blends.
    const float diagonal = directional_speed_factor(-0.7071f, 0.7071f, tuning);
    CHECK(diagonal > tuning.back_speed_factor);
    CHECK(diagonal < tuning.lateral_speed_factor);
}

// --- goal 234: the sprint ramp, in the time the tuning declares ---------------------------------

namespace {

// Integrate the ramp at a fixed tick and return the time to get within 1% of `target`.
float seconds_to_reach(glm::vec2 velocity, const glm::vec2& target, const PlayerTuning& tuning) {
    constexpr float kRampDt = 1.0f / 120.0f;
    for (int tick = 0; tick < 2400; ++tick) { // 20 s ceiling
        accelerate_ground(velocity, target, tuning, kRampDt);
        if (glm::length(velocity - target) <= 0.01f * std::max(glm::length(target), 1.0f)) {
            return static_cast<float>(tick + 1) * kRampDt;
        }
    }
    return -1.0f;
}

} // namespace

TEST_CASE("Sprint is reached in the time the tuning declares, and released in less", "[player][speed]") {
    const PlayerTuning tuning;
    const glm::vec2 sprint{tuning.sprint_speed, 0.0f};

    // t = v / a, because the ramp is a constant acceleration and not an asymptote. 7.0 / 10.0.
    const float spinUp = seconds_to_reach(glm::vec2{0.0f}, sprint, tuning);
    REQUIRE(spinUp > 0.0f);
    CHECK_THAT(spinUp, Catch::Matchers::WithinAbs(tuning.sprint_speed / tuning.ground_accel, 0.02));

    // Releasing Shift decelerates at the braking rate: 7.0 / 14.0.
    const float spinDown = seconds_to_reach(sprint, glm::vec2{0.0f}, tuning);
    REQUIRE(spinDown > 0.0f);
    CHECK_THAT(spinDown, Catch::Matchers::WithinAbs(tuning.sprint_speed / tuning.ground_decel, 0.02));
    CHECK(spinDown < spinUp);
}

TEST_CASE("The ramp never overshoots its target, at any tick rate", "[player][speed]") {
    const PlayerTuning tuning;
    const glm::vec2 target{tuning.walk_speed, 0.0f};
    // A huge dt is the case that would overshoot if the step were applied unconditionally -- the
    // same class of bug as the sweep's, and worth pinning for the same reason.
    for (const float dt : {1.0f / 240.0f, 1.0f / 60.0f, 0.25f, 4.0f}) {
        glm::vec2 v{0.0f};
        accelerate_ground(v, target, tuning, dt);
        CHECK(glm::length(v) <= glm::length(target) + 1.0e-5f);
    }
}

TEST_CASE("Turning at constant speed is charged the acceleration rate, not the braking rate",
          "[player][speed]") {
    // A direction change at the same magnitude is not braking. If it were charged `ground_decel`,
    // strafing round a corner would feel grabbier than accelerating out of one, for no reason
    // anybody chose.
    const PlayerTuning tuning;
    glm::vec2 v{tuning.walk_speed, 0.0f};
    const glm::vec2 sideways{0.0f, tuning.walk_speed};
    const float before = glm::length(v - sideways);
    accelerate_ground(v, sideways, tuning, 1.0f / 120.0f);
    const float moved = before - glm::length(v - sideways);
    CHECK_THAT(moved, Catch::Matchers::WithinRel(tuning.ground_accel / 120.0f, 1.0e-4f));
}

// --- goal 233: gravity and the jump are ONE decision ---------------------------------------------

TEST_CASE("Falling 5 m takes the time Earth gravity says", "[player][jump]") {
    // The second place the gravity choice is pinned (the apex is the first). t = sqrt(2h/|g|):
    // 5 m at 9.81 is 1.010 s. Under the old -32 it was 0.559 s, so a silent revert fails here.
    const PlayerTuning tuning;
    const float g = std::abs(tuning.gravity);
    const float fallTime = std::sqrt(2.0f * 5.0f / g);
    CHECK_THAT(fallTime, Catch::Matchers::WithinAbs(1.0096, 0.005));
    // And gravity is Earth's, deliberately, not a game-scale multiple of it.
    CHECK_THAT(g, Catch::Matchers::WithinAbs(9.81, 0.01));
}

TEST_CASE("Water's upthrust stays 2x gravity, whatever gravity is", "[player][swim]") {
    // The pair that goal 233 caught drifting. `defs/water.hpp` cannot see PlayerTuning and
    // PlayerTuning cannot see the material registry, so neither header can assert this -- but this
    // test links both, which is the whole reason it exists here rather than in either of them.
    const PlayerTuning tuning;
    CHECK_THAT(kWaterPhysics.buoyancy_acceleration,
               Catch::Matchers::WithinRel(2.0f * std::abs(tuning.gravity), 1.0e-3f));
    // And what that buys: net upthrust when fully submerged is exactly +1 g, so the time to surface
    // scales with gravity instead of being an accident of two constants moving apart.
    const float netUpthrust = tuning.gravity + kWaterPhysics.buoyancy_acceleration;
    CHECK_THAT(netUpthrust, Catch::Matchers::WithinRel(std::abs(tuning.gravity), 1.0e-3f));
}

// --- goal 240: the polish constants, checked against the perception thresholds -------------------

TEST_CASE("Head bob is above the detection threshold and below the acuity threshold", "[player][polish]") {
    // research/human-movement-and-perception-research.md Part 2: vertical translation is detected
    // at ~2.13 cm/s (median, 2AFC), and retinal slip costs acuity past ~4 deg/s, degrading rapidly
    // past 6. A bob has to clear the first and stay under the second, and the interval between them
    // is where the whole effect has to live.
    const PlayerTuning tuning;
    const float hz = tuning.bob_frequency * tuning.walk_speed;
    const float peakVelocity = 2.0f * 3.14159265f * hz * tuning.bob_amplitude;

    // The bob runs at the human step frequency, because that is the thing it imitates.
    CHECK_THAT(hz, Catch::Matchers::WithinAbs(1.9, 0.15));

    // Detected, comfortably: this is not a term nobody can see.
    CHECK(peakVelocity > 10.0f * 0.0213f);

    // But it does not cost acuity. Gaze perturbation while fixating the ground 4 m ahead:
    // arctan(v / 4 m), which the research's own worked example computes the same way.
    const float gazeDegPerSec = std::atan(peakVelocity / 4.0f) * 180.0f / 3.14159265f;
    CHECK(gazeDegPerSec < 4.0f);
}

TEST_CASE("The polish budget is a budget: no single term can exceed it", "[player][polish]") {
    // `landing_dip_max` used to be 0.06 against a 0.05 budget, so the clamp silently truncated a
    // hard landing and the two constants disagreed about what was permitted.
    const PlayerTuning tuning;
    CHECK(tuning.landing_dip_max <= tuning.polish_max_offset);
    CHECK(tuning.bob_amplitude <= tuning.polish_max_offset);
    // And the two largest terms together still fit, so a landing mid-stride is not clipped.
    CHECK(tuning.landing_dip_max + tuning.bob_amplitude <= tuning.polish_max_offset + 0.02f);
}

TEST_CASE("The landing dip is perceptible for a real landing", "[player][polish]") {
    // A fall from the jump apex arrives at sqrt(2 g h). The dip it produces has to be visible or it
    // is a term nobody can see, which goal 240 says to delete rather than keep.
    const PlayerTuning tuning;
    const float apex = (tuning.jump_speed * tuning.jump_speed) / (2.0f * std::abs(tuning.gravity));
    const float impact = std::sqrt(2.0f * std::abs(tuning.gravity) * apex);
    const float dip = std::min(impact * tuning.landing_dip_per_speed, tuning.landing_dip_max);
    const float peakVelocity = dip / tuning.landing_dip_tau;
    CHECK(dip > 0.02f);                   // a couple of centimetres of eye travel
    CHECK(peakVelocity > 5.0f * 0.0213f); // and well past the vertical detection threshold
}
