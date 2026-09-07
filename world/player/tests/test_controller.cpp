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
    // The discrete integrator undershoots the closed-form 1.129 m slightly; the band is the check.
    REQUIRE(apex > 1.0f);
    REQUIRE(apex < 1.25f);
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
        for (int i = 0; i < 60; ++i) {
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
    for (int i = 0; i < 300; ++i) {
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
