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
