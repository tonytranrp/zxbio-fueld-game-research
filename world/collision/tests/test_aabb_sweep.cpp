#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/collision/aabb_sweep.hpp"

using world::collision::Aabb;
using world::collision::move_and_slide;
using world::collision::SweepParams;
using world::collision::SweepResult;

namespace {

// A floor (everything below y = 0), plus any number of solid boxes.
struct BoxWorld {
    std::vector<Aabb> solids;
    float floor = 0.0f;

    [[nodiscard]] bool overlaps_solid(const Aabb& box) const {
        if (box.min.y < floor) {
            return true;
        }
        for (const Aabb& s : solids) {
            if (s.intersects(box)) {
                return true;
            }
        }
        return false;
    }
};
static_assert(world::collision::SolidQuery<BoxWorld>);

Aabb body_at(float x, float y, float z) {
    return Aabb::upright(glm::vec3{x, y, z}, 0.3f, 1.75f);
}

} // namespace

TEST_CASE("falling onto the floor stops exactly on it and reports grounded", "[collision][sweep]") {
    BoxWorld world;
    const Aabb body = body_at(0.0f, 1.0f, 0.0f);
    const SweepResult r = move_and_slide(world, body, glm::vec3{0.0f, -5.0f, 0.0f}, SweepParams{});
    CHECK(r.blocked_y);
    CHECK(r.grounded);
    CHECK_FALSE(r.started_inside);
    // Within the bisection's resolution of the floor, never through it.
    CHECK(body.min.y + r.delta.y >= 0.0f);
    CHECK(body.min.y + r.delta.y < 0.01f);
}

TEST_CASE("a wall blocks the axis toward it and the body slides along it", "[collision][sweep]") {
    BoxWorld world;
    world.solids.push_back(Aabb{glm::vec3{2.0f, -1.0f, -10.0f}, glm::vec3{3.0f, 5.0f, 10.0f}}); // x in [2,3]
    const Aabb body = body_at(0.0f, 0.0f, 0.0f);
    SweepParams params;
    params.step_height = 0.0f;
    const SweepResult r = move_and_slide(world, body, glm::vec3{4.0f, 0.0f, 1.5f}, params);
    CHECK(r.blocked_x);
    CHECK_FALSE(r.blocked_z);
    CHECK(r.delta.z == 1.5f);              // sliding keeps the free axis in full
    CHECK(body.max.x + r.delta.x <= 2.0f); // stopped at the wall...
    CHECK(body.max.x + r.delta.x > 1.99f); // ...right at it
}

TEST_CASE("a low ledge is stepped up, a tall one is not", "[collision][sweep]") {
    BoxWorld world;
    world.solids.push_back(Aabb{glm::vec3{1.0f, 0.0f, -10.0f}, glm::vec3{20.0f, 0.4f, 10.0f}}); // 0.4 m ledge
    const Aabb body = body_at(0.0f, 0.0f, 0.0f);
    SweepParams params;
    params.step_height = 0.55f;
    const SweepResult r = move_and_slide(world, body, glm::vec3{1.0f, 0.0f, 0.0f}, params);
    CHECK(r.stepped_up);
    CHECK(r.delta.x == 1.0f);
    CHECK(std::abs(r.delta.y - 0.4f) < 0.01f); // landed on top of the ledge
    CHECK_FALSE(world.overlaps_solid(body.translated(r.delta)));

    world.solids[0].max.y = 0.9f; // too tall to step
    const SweepResult tall = move_and_slide(world, body, glm::vec3{1.0f, 0.0f, 0.0f}, params);
    CHECK_FALSE(tall.stepped_up);
    CHECK(tall.blocked_x);
    CHECK(body.max.x + tall.delta.x <= 1.0f);

    // Fly mode (no step height) never climbs.
    world.solids[0].max.y = 0.4f;
    params.step_height = 0.0f;
    const SweepResult fly = move_and_slide(world, body, glm::vec3{1.0f, 0.0f, 0.0f}, params);
    CHECK_FALSE(fly.stepped_up);
    CHECK(fly.blocked_x);
}

TEST_CASE("a body that starts inside solid is not trapped", "[collision][sweep]") {
    BoxWorld world;
    const Aabb buried = body_at(0.0f, -3.0f, 0.0f);
    const SweepResult r = move_and_slide(world, buried, glm::vec3{1.0f, 2.0f, 0.0f}, SweepParams{});
    CHECK(r.started_inside);
    CHECK(r.delta == glm::vec3{1.0f, 2.0f, 0.0f});
    CHECK_FALSE(r.blocked_x);
    CHECK_FALSE(r.blocked_y);
}

TEST_CASE("a body resting a float-ulp inside its own floor is depenetrated, not released",
          "[collision][sweep]") {
    // The bug the skin exists for, found by world/player's step-up test. The caller stores the
    // camera EYE and rebuilds the feet as (eye - eye_height) every tick; that round trip is worth
    // ~1e-7, so a body the sweep placed exactly on a surface reads back as marginally INSIDE it.
    // Before the skin, that took the "started inside -> move unblocked" escape and handed back the
    // full wanted motion -- gravity walking the body straight through the floor it was standing on,
    // one tick after landing. A voxel world puts surfaces at exact coordinates constantly, so this
    // is reachable, not theoretical.
    BoxWorld world;
    world.floor = -100.0f; // no half-space floor; the ledge below is the only solid
    world.solids.push_back(Aabb{glm::vec3{-10.0f, -10.0f, -10.0f}, glm::vec3{10.0f, 0.04f, 10.0f}});

    const Aabb sunk = body_at(0.0f, std::nextafter(0.04f, 0.0f), 0.0f); // one ulp inside the top
    REQUIRE(world.overlaps_solid(sunk));

    const SweepResult r = move_and_slide(world, sunk, glm::vec3{0.0f, -0.5f, 0.0f}, SweepParams{});
    CHECK_FALSE(r.started_inside);                               // not released from collision...
    CHECK(r.delta.y > -0.001f);                                  // ...and it did not fall through the surface
    CHECK_FALSE(world.overlaps_solid(sunk.translated(r.delta))); // it ends outside, so this settles
}

TEST_CASE("a deeply embedded body still escapes", "[collision][sweep]") {
    // The skin must not turn the anti-trap policy off: a body genuinely inside rock (spawned there,
    // or swallowed by a world rebuild) still moves unblocked until it is free.
    BoxWorld world;
    world.solids.push_back(Aabb{glm::vec3{-10.0f, -10.0f, -10.0f}, glm::vec3{10.0f, 5.0f, 10.0f}});
    const Aabb buried = body_at(0.0f, 1.0f, 0.0f);
    const SweepResult r = move_and_slide(world, buried, glm::vec3{1.0f, 2.0f, 0.0f}, SweepParams{});
    CHECK(r.started_inside);
    CHECK(r.delta == glm::vec3{1.0f, 2.0f, 0.0f});
}

TEST_CASE("many small steps never tunnel through a thin wall", "[collision][sweep]") {
    BoxWorld world;
    world.solids.push_back(Aabb{glm::vec3{5.0f, -1.0f, -10.0f}, glm::vec3{5.05f, 5.0f, 10.0f}}); // 5 cm wall
    Aabb body = body_at(0.0f, 0.0f, 0.0f);
    SweepParams params;
    params.step_height = 0.0f;
    for (int i = 0; i < 200; ++i) {
        const SweepResult r = move_and_slide(world, body, glm::vec3{0.3f, -0.1f, 0.0f}, params);
        body = body.translated(r.delta);
        REQUIRE_FALSE(world.overlaps_solid(body));
    }
    CHECK(body.max.x <= 5.0f);
    CHECK(body.max.x > 4.99f);
}

// --- goal 229: the sub-step rule ----------------------------------------------------------------

TEST_CASE("the sub-step is derived from the body, not from a constant", "[collision][sweep]") {
    // Half the smallest extent, for whatever body is handed in -- the point of deriving it is that
    // shrinking the body shrinks the sub-step without anyone remembering to.
    CHECK(world::collision::substep_for(body_at(0.0f, 0.0f, 0.0f)) == 0.3f); // 0.6 m the narrow way
    const Aabb tiny = Aabb::upright(glm::vec3{0.0f}, 0.02f, 1.0f);
    CHECK(world::collision::substep_for(tiny) == 0.02f); // 2 * 0.02 half-width = 0.04 extent
}

TEST_CASE("a wall one voxel thick stops a body moving many body-lengths in one call", "[collision][sweep]") {
    // The claim the rule makes is that tunnelling is impossible for an obstacle of ANY thickness,
    // because consecutive sub-stepped boxes intersect and so their union has no gaps. A 7.8 mm wall
    // -- the finest voxel this world has -- against a 40 m motion is the strongest form of that.
    constexpr float kVoxel = 1.0f / 128.0f;
    for (const float distance : {1.0f, 5.0f, 40.0f, 400.0f}) {
        BoxWorld world;
        world.solids.push_back(Aabb{glm::vec3{5.0f, 0.0f, -10.0f}, glm::vec3{5.0f + kVoxel, 4.0f, 10.0f}});
        const Aabb body = body_at(0.0f, 0.0f, 0.0f);
        SweepParams params;
        params.step_height = 0.0f; // no climbing: this is about the horizontal sweep alone
        const SweepResult r = move_and_slide(world, body, glm::vec3{distance, 0.0f, 0.0f}, params);
        if (distance <= 4.7f) {
            CHECK_FALSE(r.blocked_x); // the wall is out of reach; nothing to hit
        } else {
            CHECK(r.blocked_x);
            CHECK(body.max.x + r.delta.x <= 5.0f);
        }
        CHECK_FALSE(world.overlaps_solid(body.translated(r.delta)));
    }
}

TEST_CASE("an unblocked axis applies exactly the wanted motion", "[collision][sweep]") {
    // Sub-stepping is a search strategy, not a change to the answer. Summing wanted/n n times does
    // not give back `wanted` in binary floating point; the snap in move_and_slide does.
    BoxWorld world;
    const Aabb body = body_at(0.0f, 1.0f, 0.0f);
    for (const float d : {1.5f, 0.7f, 13.0f, 0.1f}) {
        const SweepResult r = move_and_slide(world, body, glm::vec3{d, 0.0f, d}, SweepParams{});
        CHECK(r.delta.x == d);
        CHECK(r.delta.z == d);
    }
}

// --- goal 229a: the depenetration policy --------------------------------------------------------

TEST_CASE("a body embedded deeper than the skin climbs out instead of moving unblocked",
          "[collision][sweep]") {
    // The bug this replaces: `started_inside` returned the wanted motion UNBLOCKED, which never
    // frees the body, so one bad tick became hundreds. Measured on walk_hillside: 761 inside-solid
    // ticks, all 761 of them this branch.
    BoxWorld world;
    world.solids.push_back(Aabb{glm::vec3{-5.0f, 0.0f, -5.0f}, glm::vec3{5.0f, 1.0f, 5.0f}});
    const Aabb body = body_at(0.0f, 0.95f, 0.0f); // feet 0.05 m INSIDE the block, 50x the skin
    REQUIRE(world.overlaps_solid(body));

    const SweepResult r = move_and_slide(world, body, glm::vec3{0.0f, 0.0f, 0.0f}, SweepParams{});
    CHECK_FALSE(r.started_inside);                               // it recovered
    CHECK(r.delta.y > 0.0f);                                     // upward
    CHECK(r.delta.y < 0.1f);                                     // and barely -- the least that works
    CHECK_FALSE(world.overlaps_solid(body.translated(r.delta))); // genuinely out
}

TEST_CASE("a body buried deeper than it is tall still gets the unblocked escape", "[collision][sweep]") {
    // The bound matters: lifting an arbitrarily deep body would teleport it to the sky. Past the
    // body's own height the old policy is still the right one -- never trap the player -- and it
    // still SAYS so, which is what the app's counter reports.
    BoxWorld world;
    world.solids.push_back(Aabb{glm::vec3{-5.0f, 0.0f, -5.0f}, glm::vec3{5.0f, 20.0f, 5.0f}});
    const Aabb body = body_at(0.0f, 10.0f, 0.0f);
    REQUIRE(world.overlaps_solid(body));

    const SweepResult r = move_and_slide(world, body, glm::vec3{1.0f, 0.0f, 0.0f}, SweepParams{});
    CHECK(r.started_inside);
    CHECK(r.delta == glm::vec3{1.0f, 0.0f, 0.0f});
}

TEST_CASE("the climb-out finds a gap the doubling ladder would step over", "[collision][sweep]") {
    // The ladder probes skin*8, *2, *2 ... and stops below the body height, so before the cap probe
    // was added a body buried between the last rung and the cap was declared unrecoverable by an
    // arithmetic accident. Measured for real: macro_ground buried the body 1.40 m, the ladder
    // reached 1.024 m, the cap was 1.75 m.
    BoxWorld world;
    world.solids.push_back(Aabb{glm::vec3{-5.0f, 0.0f, -5.0f}, glm::vec3{5.0f, 3.0f, 5.0f}});
    const Aabb body = body_at(0.0f, 1.6f, 0.0f); // feet 1.4 m inside a 3 m block
    REQUIRE(world.overlaps_solid(body));

    const SweepResult r = move_and_slide(world, body, glm::vec3{0.0f, 0.0f, 0.0f}, SweepParams{});
    CHECK_FALSE(r.started_inside);
    CHECK(r.delta.y > 1.39f);
    CHECK_FALSE(world.overlaps_solid(body.translated(r.delta)));
}
