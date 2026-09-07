#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

#include "world/player/controller.hpp"
#include "world/player/view_polish.hpp"

using namespace world::player;
using world::collision::Aabb;

namespace {

constexpr float kDt = 1.0f / 60.0f;

// A test world made of half-spaces: solid below `ground_y`, plus an optional vertical wall at
// x >= wall_x whose top is `wall_top`. Enough to exercise every branch of step_player without
// pulling in the heightmap generator.
struct FakeWorld {
    float ground_y = 0.0f;
    float wall_x = std::numeric_limits<float>::infinity();
    float wall_top = 0.0f;

    [[nodiscard]] bool overlaps_solid(const Aabb& box) const noexcept {
        if (box.min.y < ground_y) {
            return true;
        }
        return box.max.x > wall_x && box.min.y < wall_top;
    }
};
static_assert(world::collision::SolidQuery<FakeWorld>);

struct Sim {
    FakeWorld world;
    PlayerState state;
    PlayerTuning tuning;
    WorldSense sense;
    glm::vec3 eye{0.0f, 1.7f, 0.0f};

    Sim() {
        state.mode = MoveMode::Walk;
        state.stance = Stance::Grounded;
        sense.water_surface_y = -1000.0f; // dry by default
    }

    StepResult tick(const PlayerIntent& intent) {
        return step_player(world, state, tuning, intent, sense, eye, 0.0f, 0.0f, 40.0f, kDt);
    }
};

// Ticks needed to cover `metres` at walking pace, with slack for the acceleration ramp. Goal 232
// dropped walking from 10 m/s to 1.4, and three cases in this file failed purely because their
// tick counts were sized against the old speed. A count derived from the tuning cannot rot that way.
int ticks_to_walk(float metres, const PlayerTuning& tuning) {
    const float rampSlack = tuning.walk_speed / tuning.ground_accel; // the ramp's own cost, seconds
    return static_cast<int>((metres / tuning.walk_speed + rampSlack) / kDt) + 4;
}

} // namespace

TEST_CASE("Standing on flat ground stays grounded and still", "[player][controller]") {
    Sim sim;
    const PlayerIntent idle;
    for (int i = 0; i < 120; ++i) {
        sim.tick(idle);
    }
    REQUIRE(sim.state.stance == Stance::Grounded);
    REQUIRE_THAT(sim.eye.y, Catch::Matchers::WithinAbs(1.7, 1.0e-4));
}

TEST_CASE("A jump reaches the predicted apex and lands", "[player][controller][jump]") {
    Sim sim;
    PlayerIntent jump;
    jump.jump_pressed = true;
    sim.tick(jump); // the edge fires on exactly this tick

    float highest = sim.eye.y;
    const PlayerIntent idle;
    bool landed = false;
    for (int i = 0; i < 240 && !landed; ++i) {
        highest = std::max(highest, sim.eye.y);
        landed = sim.tick(idle).landed;
    }
    REQUIRE(landed);
    const float apex = highest - 1.7f;
    // Goal 233 moved gravity and jump_speed together to Earth scale, so the closed form is now
    // 3.43^2 / 19.62 = 0.600 m. The discrete integrator undershoots it slightly; the band is the
    // check, and it is the same check it always was.
    // Band, not equality: the jump impulse lands INSIDE the tick that has already paid its gravity
    // decrement, so semi-implicit Euler at 60 Hz overshoots the continuous closed form by ~4.7%
    // (measured: 0.6283 against 0.6000). That was true before goal 233 too -- it is just visible
    // now that the apex is 0.6 m rather than 1.13, where the same 4.7% hid inside a wider band.
    REQUIRE(apex > 0.55f);
    REQUIRE(apex < 0.66f);
    REQUIRE(sim.state.stance == Stance::Grounded);
    REQUIRE_THAT(sim.eye.y, Catch::Matchers::WithinAbs(1.7, 1.0e-3));
}

TEST_CASE("Holding jump does not re-arm the buffer every tick", "[player][controller][jump]") {
    // The edge contract: intent.jump_pressed is set on ONE tick per press. A caller that (wrongly)
    // held it would bunny-hop; this asserts the state machine itself does not.
    Sim sim;
    PlayerIntent jump;
    jump.jump_pressed = true;
    REQUIRE(sim.tick(jump).jumped);
    const PlayerIntent idle;
    int extraJumps = 0;
    for (int i = 0; i < 240; ++i) {
        if (sim.tick(idle).jumped) {
            ++extraJumps;
        }
    }
    REQUIRE(extraJumps == 0);
}

TEST_CASE("Walking off a ledge still jumps inside the coyote window", "[player][controller][jump]") {
    Sim sim;
    const PlayerIntent idle;
    sim.tick(idle); // one grounded tick: this is what grants the coyote credit

    sim.world.ground_y = -100.0f; // the ground vanishes: we just walked off
    sim.sense.ground_height = -100.0f;
    sim.tick(idle); // first airborne tick
    REQUIRE(sim.state.stance == Stance::Airborne);
    REQUIRE(sim.state.coyote_remaining > 0.0f);

    PlayerIntent jump;
    jump.jump_pressed = true;
    sim.tick(idle); // ~0.033 s late
    REQUIRE(sim.tick(jump).jumped);
}

TEST_CASE("The step budget climbs a 4 cm lip but not a 40 cm one", "[player][controller][step]") {
    // Yaw 0 looks down -Z, so "right" is the +X direction the wall below sits in.
    PlayerIntent right;
    right.right = true;

    SECTION("4 cm lip is climbed") {
        Sim sim;
        sim.tuning.step_height = kSvoStepHeight;
        sim.world.wall_x = 1.0f;
        sim.world.wall_top = 0.04f;
        const int ticks = ticks_to_walk(2.0f, sim.tuning);
        for (int i = 0; i < ticks; ++i) {
            sim.tick(right);
        }
        REQUIRE(sim.eye.x > 1.5f);         // walked past the lip
        REQUIRE(sim.eye.y > 1.7f + 0.03f); // and is STILL standing on top of it 55 ticks later
    }
    SECTION("40 cm wall is not") {
        Sim sim;
        sim.tuning.step_height = kSvoStepHeight;
        sim.world.wall_x = 1.0f;
        sim.world.wall_top = 0.40f;
        for (int i = 0; i < 60; ++i) {
            sim.tick(right);
        }
        REQUIRE(sim.eye.x < 1.0f); // stopped at the wall (body half-width keeps it short of it)
        REQUIRE_THAT(sim.eye.y, Catch::Matchers::WithinAbs(1.7, 1.0e-3));
    }
}

TEST_CASE("The smoothed eye never diverges from the physical eye past the budget",
          "[player][controller][smoothing]") {
    // Prompt 001 A3's Check, over a real climb: a staircase of 4 cm lips.
    Sim sim;
    sim.tuning.step_height = kSvoStepHeight;
    sim.world.wall_x = 0.5f;
    sim.world.wall_top = 0.04f;
    PlayerIntent right;
    right.right = true;
    float worst = 0.0f;
    for (int i = 0; i < 120; ++i) {
        sim.tick(right);
        worst = std::max(worst, std::abs(sim.state.eye_smooth_offset));
        // The step budget plus one tick of the slope the body is climbing; never the wild
        // excursion an unclamped follower would show.
        REQUIRE(std::abs(sim.state.eye_smooth_offset) <= sim.tuning.eye_smooth_max_lag + 1.0e-6f);
    }
    REQUIRE(worst > 0.0f); // it did smooth something -- the test is not vacuous
    REQUIRE(worst <= kSvoStepHeight + 1.0e-3f);
}

TEST_CASE("Swimming floats at equilibrium, dives, and surfaces", "[player][controller][swim]") {
    Sim sim;
    sim.world.ground_y = -20.0f; // deep seabed
    sim.sense.ground_height = -20.0f;
    sim.sense.water_surface_y = 0.0f;
    sim.eye.y = 0.0f; // feet at -1.7, well under

    const PlayerIntent idle;
    for (int i = 0; i < 600; ++i) {
        sim.tick(idle);
    }
    REQUIRE(sim.state.stance == Stance::Swimming);
    const float feet = sim.eye.y - sim.tuning.eye_height;
    // Floats with the feet about swim_equilibrium_depth under, eyes above water.
    REQUIRE(feet < 0.0f);
    REQUIRE(sim.eye.y > 0.0f);

    PlayerIntent dive;
    dive.down = true;
    for (int i = 0; i < 180; ++i) { // 3 s of diving at 3.5 m/s
        sim.tick(dive);
    }
    REQUIRE(sim.eye.y < -3.0f);

    PlayerIntent rise;
    rise.up = true;
    for (int i = 0; i < 600; ++i) {
        sim.tick(rise);
    }
    REQUIRE(sim.eye.y > 0.0f); // surfaced again
}

TEST_CASE("The shore assist pops a swimmer onto a low lip", "[player][controller][swim]") {
    Sim sim;
    sim.world.ground_y = -20.0f;
    sim.sense.ground_height = -20.0f;
    sim.sense.water_surface_y = 0.0f;
    // A 0.5 m shore lip at x = 1: taller than the floating body's feet, shorter than the probe.
    //
    // The step budget must be the SVO path's (4 cm), not the mesh path's 0.55 m relic, or this case
    // does not test what it says. Found by goal 233: under Earth gravity the swimmer floats higher
    // than it did under -32, high enough that a 0.55 m step budget climbs a 0.5 m lip outright --
    // so the assist never fired and the case passed for the wrong reason waiting to happen. It is
    // the shipping configuration either way; it just used not to matter which.
    sim.tuning.step_height = kSvoStepHeight;
    sim.world.wall_x = 1.0f;
    sim.world.wall_top = 0.5f;
    sim.eye.y = 0.0f;

    const PlayerIntent idle;
    for (int i = 0; i < 120; ++i) {
        sim.tick(idle); // settle at the surface first
    }
    PlayerIntent right;
    right.right = true;
    bool popped = false;
    // Ten seconds, NOT a distance: the assist is time-limited, not travel-limited. The swimmer bobs
    // against the lip while buoyancy and the horizontal push argue, and the pop fires on whichever
    // tick finds free air one probe-height up. Sizing this from the walk speed (as the two cases
    // above legitimately are) would be modelling the wrong thing.
    for (int i = 0; i < 600; ++i) {
        popped = sim.tick(right).shore_popped || popped;
    }
    REQUIRE(popped);
    REQUIRE(sim.eye.x > 1.0f); // made it past the lip rather than bobbing against it forever
}

TEST_CASE("Fly mode is unchanged: no gravity, vertical strafe, collision still on",
          "[player][controller][fly]") {
    Sim sim;
    sim.state.mode = MoveMode::Fly;
    sim.eye = glm::vec3{0.0f, 10.0f, 0.0f};
    const PlayerIntent idle;
    for (int i = 0; i < 120; ++i) {
        sim.tick(idle);
    }
    REQUIRE_THAT(sim.eye.y, Catch::Matchers::WithinAbs(10.0, 1.0e-5)); // did not fall

    PlayerIntent down;
    down.down = true;
    for (int i = 0; i < 600; ++i) {
        sim.tick(down);
    }
    REQUIRE(sim.eye.y >= 0.0f); // stopped by the ground, not flown through it
}

TEST_CASE("View polish never moves the eye more than 5 cm", "[player][polish]") {
    // Prompt 001 A6's Check. Driven with deliberately absurd inputs: a 200 m/s impact and full
    // walking speed, on every tick.
    PlayerState state;
    state.mode = MoveMode::Walk;
    state.stance = Stance::Grounded;
    const PlayerTuning tuning;
    ViewPolishSense sense;
    sense.horizontal_speed = 10.0f;
    sense.boosting = true;
    StepResult landing;
    landing.landed = true;
    landing.impact_speed = 200.0f;

    for (int i = 0; i < 600; ++i) {
        const float offset = update_view_polish(state, tuning, sense, i == 0 ? landing : StepResult{},
                                                /*enabled=*/true, kDt);
        REQUIRE(std::abs(offset) <= tuning.polish_max_offset + 1.0e-6f);
    }
    REQUIRE(view_polish_fov_offset(state) > 0.0f); // the boost kick did settle in
}

TEST_CASE("Disabled view polish returns exactly zero and settles", "[player][polish]") {
    PlayerState state;
    state.mode = MoveMode::Walk;
    state.stance = Stance::Grounded;
    const PlayerTuning tuning;
    ViewPolishSense sense;
    sense.horizontal_speed = 10.0f;
    sense.boosting = true;
    for (int i = 0; i < 300; ++i) {
        REQUIRE(update_view_polish(state, tuning, sense, StepResult{}, /*enabled=*/false, kDt) == 0.0f);
    }
    REQUIRE(std::abs(view_polish_fov_offset(state)) < 1.0e-4f); // decayed, not frozen
}

TEST_CASE("The simulation is a pure function of the tick count", "[player][controller][determinism]") {
    // A1's determinism claim, end to end: the same intents over the same NUMBER of fixed ticks give
    // bit-identical state, regardless of how a caller batched them per frame.
    PlayerIntent walk;
    walk.forward = true;
    walk.jump_pressed = false;

    Sim a;
    Sim b;
    for (int i = 0; i < 300; ++i) {
        a.tick(walk);
    }
    // Same 300 ticks, "delivered" in irregular batches of 7, 1, 13, ...
    const int batches[] = {7, 1, 13, 29, 3, 47, 100, 100};
    int done = 0;
    for (const int n : batches) {
        for (int i = 0; i < n && done < 300; ++i, ++done) {
            b.tick(walk);
        }
    }
    REQUIRE(done == 300);
    REQUIRE(a.eye.x == b.eye.x);
    REQUIRE(a.eye.y == b.eye.y);
    REQUIRE(a.eye.z == b.eye.z);
    REQUIRE(a.state.vertical_velocity == b.state.vertical_velocity);
    REQUIRE(a.state.eye_smooth_offset == b.state.eye_smooth_offset);
}

// --- goal 235: the walkable slope limit -----------------------------------------------------------

namespace {

// A world that is a single inclined plane through the origin, rising along +X at `slope`. The body
// collides against the real plane; `WorldSense` reports the same slope analytically, which is the
// split the shipping code has (voxel surface for collision, analytic field for steepness).
struct RampWorld {
    float slope_radians = 0.0f;

    [[nodiscard]] float height_at(float x) const noexcept { return x * std::tan(slope_radians); }

    [[nodiscard]] bool overlaps_solid(const Aabb& box) const noexcept {
        // Solid below the plane. Sampled at the box's own uphill edge so a body standing on the
        // ramp rests on the highest ground it covers, exactly as the voxel world would place it.
        return box.min.y < height_at(box.max.x);
    }
};
static_assert(world::collision::SolidQuery<RampWorld>);

struct RampSim {
    RampWorld world;
    PlayerState state;
    PlayerTuning tuning;
    WorldSense sense;
    glm::vec3 eye{0.0f, 0.0f, 0.0f};

    explicit RampSim(float slopeDegrees) {
        world.slope_radians = glm::radians(slopeDegrees);
        state.mode = MoveMode::Walk;
        state.stance = Stance::Grounded;
        sense.water_surface_y = -1000.0f;
        sense.ground_slope_radians = world.slope_radians;
        sense.ground_uphill = glm::vec2{1.0f, 0.0f}; // the ramp rises along +X
        tuning.step_height = kSvoStepHeight;
        eye.y = world.height_at(0.0f) + tuning.eye_height + 0.01f;
    }

    StepResult tick(const PlayerIntent& intent) {
        sense.ground_height = world.height_at(eye.x);
        // Yaw -90 looks along +X, which is straight up the ramp.
        return step_player(world, state, tuning, intent, sense, eye, glm::radians(-90.0f), 0.0f, 40.0f, kDt);
    }
};

} // namespace

TEST_CASE("A slope at or under the limit is climbed; one above it is not", "[player][slope]") {
    PlayerIntent uphill;
    uphill.forward = true;
    const PlayerTuning defaults;
    const float limitDegrees = glm::degrees(defaults.max_walk_slope_radians);

    // The prompt's ladder. Each rung is compared against the DECLARED limit, not a literal, so
    // retuning the limit retunes the expectation with it.
    for (const float degrees : {20.0f, 30.0f, 40.0f, 50.0f, 60.0f}) {
        RampSim sim(degrees);
        const float startX = sim.eye.x;
        for (int i = 0; i < 240; ++i) { // 4 s
            sim.tick(uphill);
        }
        const float travelled = sim.eye.x - startX;
        const bool walkable = degrees <= limitDegrees + 1.0e-3f;
        UNSCOPED_INFO("slope " << degrees << " deg, travelled " << travelled << " m, limit " << limitDegrees);
        if (walkable) {
            CHECK(travelled > 0.5f); // made real progress up it
            CHECK_FALSE(sim.state.slide_speed > 0.0f);
        } else {
            // Refused: the body may not gain ground up the face, and it slides back down it.
            CHECK(travelled <= 0.0f);
            CHECK(sim.state.slide_speed > 0.0f);
        }
    }
}

TEST_CASE("The climb/slide boundary does not oscillate", "[player][slope]") {
    // The prompt's stability check: 2 degrees either side of the limit, held for 60 ticks, the
    // answer must not change once it has settled. It cannot, by construction -- "too steep" is a
    // pure function of a smooth analytic field, with no hysteresis to ring -- and this is the case
    // that says so rather than assuming it.
    PlayerIntent uphill;
    uphill.forward = true;
    const float limitDegrees = glm::degrees(PlayerTuning{}.max_walk_slope_radians);

    for (const float offset : {-2.0f, 2.0f}) {
        RampSim sim(limitDegrees + offset);
        for (int i = 0; i < 30; ++i) {
            sim.tick(uphill); // settle
        }
        const bool slidingAfterSettle = sim.tick(uphill).sliding;
        int changes = 0;
        for (int i = 0; i < 60; ++i) {
            if (sim.tick(uphill).sliding != slidingAfterSettle) {
                ++changes;
            }
        }
        UNSCOPED_INFO("slope " << (limitDegrees + offset) << " deg, sliding " << slidingAfterSettle
                               << ", changes " << changes);
        CHECK(changes == 0);
        CHECK(slidingAfterSettle == (offset > 0.0f));
    }
}

TEST_CASE("A slide is a friction cone: zero at the limit, growing above it, capped", "[player][slope]") {
    // The limit and the slide strength are ONE number, not two that can disagree: net downslope
    // acceleration is g(sin t - tan(limit) cos t), which is exactly zero at the limit.
    const PlayerTuning tuning;
    const float limit = tuning.max_walk_slope_radians;
    PlayerState state;
    WorldSense sense;
    sense.ground_uphill = glm::vec2{1.0f, 0.0f};

    sense.ground_slope_radians = limit;
    static_cast<void>(update_slide(state, tuning, sense, true, 1.0f));
    CHECK_THAT(state.slide_speed, Catch::Matchers::WithinAbs(0.0, 1.0e-5));

    // Steeper accelerates, and steeper still accelerates harder.
    state.slide_speed = 0.0f;
    sense.ground_slope_radians = limit + glm::radians(10.0f);
    static_cast<void>(update_slide(state, tuning, sense, true, 0.1f));
    const float gentle = state.slide_speed;
    CHECK(gentle > 0.0f);

    state.slide_speed = 0.0f;
    sense.ground_slope_radians = glm::radians(80.0f);
    static_cast<void>(update_slide(state, tuning, sense, true, 0.1f));
    CHECK(state.slide_speed > gentle);

    // Capped: a slide is not faster than a run, however long the face is.
    for (int i = 0; i < 1000; ++i) {
        static_cast<void>(update_slide(state, tuning, sense, true, 1.0f / 60.0f));
    }
    CHECK_THAT(state.slide_speed, Catch::Matchers::WithinRel(tuning.max_slide_speed, 1.0e-4f));

    // And it bleeds off on walkable ground rather than stopping dead, so stepping from a 41 degree
    // face onto a 39 degree one is not a wall.
    sense.ground_slope_radians = 0.0f;
    static_cast<void>(update_slide(state, tuning, sense, true, 1.0f / 60.0f));
    CHECK(state.slide_speed < tuning.max_slide_speed);
    CHECK(state.slide_speed > 0.0f);
}

TEST_CASE("Airborne is not sliding, however steep the ground below is", "[player][slope]") {
    // `grounded` gates the whole thing: a body falling past a cliff face is falling, not sliding
    // down it, and the two must not both be adding downhill velocity.
    const PlayerTuning tuning;
    PlayerState state;
    WorldSense sense;
    sense.ground_uphill = glm::vec2{1.0f, 0.0f};
    sense.ground_slope_radians = glm::radians(80.0f);
    const glm::vec2 slide = update_slide(state, tuning, sense, false, 0.5f);
    CHECK(state.slide_speed == 0.0f);
    CHECK(glm::length(slide) == 0.0f);
}
