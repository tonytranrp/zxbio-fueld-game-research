#include <array>
#include <cmath>
#include <cstdio>
#include <random>

#include <catch2/catch_test_macros.hpp>

#include "world/collision/aabb_sweep.hpp"
#include "world/collision/terrain_collider.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_placement.hpp"

using world::collision::Aabb;
using world::collision::move_and_slide;
using world::collision::SweepParams;
using world::collision::SweepResult;
using world::collision::TerrainCollider;
using world::collision::TerrainColliderParams;

namespace {

constexpr float kHalfWidth = 0.3f;
constexpr float kHeight = 1.75f;

Aabb body_at(const glm::vec3& feet) {
    return Aabb::upright(feet, kHalfWidth, kHeight);
}

// Drops a body from high above the column onto the ground with the sweep; returns it resting.
Aabb settle(TerrainCollider& collider, float x, float z) {
    glm::vec3 feet{x, 120.0f, z};
    collider.refresh(feet);
    Aabb body = body_at(feet);
    REQUIRE_FALSE(collider.overlaps_solid(body)); // 120 m is above every hilltop
    SweepParams sweep;
    bool grounded = false;
    for (int step = 0; step < 400 && !grounded; ++step) {
        const SweepResult r = move_and_slide(collider, body, glm::vec3{0.0f, -0.5f, 0.0f}, sweep);
        body = body.translated(r.delta);
        grounded = r.grounded;
    }
    REQUIRE(grounded);
    REQUIRE_FALSE(collider.overlaps_solid(body));
    return body;
}

// The highest analytic surface under a footprint (5x5 samples): what a box collider stands on.
float footprint_max_height(const TerrainCollider& collider, const Aabb& body) {
    float hMax = -1.0e9f;
    for (int j = 0; j < 5; ++j) {
        for (int i = 0; i < 5; ++i) {
            const float x = body.min.x + (body.max.x - body.min.x) * static_cast<float>(i) / 4.0f;
            const float z = body.min.z + (body.max.z - body.min.z) * static_cast<float>(j) / 4.0f;
            hMax = std::max(hMax, collider.ground_height(x, z));
        }
    }
    return hMax;
}

} // namespace

TEST_CASE("the voxel top is never below the analytic surface and within one voxel above it",
          "[collision][terrain]") {
    const world::generation::HeightmapGenerator heightmap(1337);
    TerrainColliderParams params;
    params.async = false;
    const TerrainCollider collider(heightmap, params);
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> coord(-200.0f, 200.0f);
    for (int i = 0; i < 2000; ++i) {
        const float x = coord(rng);
        const float z = coord(rng);
        const float top = collider.voxel_top(x, z);
        // The column is sampled at its min corner, so compare against that corner's height.
        const float e = params.voxel_edge;
        const float hCorner = heightmap.height_at(std::floor(x / e) * e, std::floor(z / e) * e);
        CHECK(top >= hCorner);
        CHECK(top - hCorner <= e + 1.0e-4f);
    }
}

TEST_CASE("a body dropped anywhere settles on the ground and never ends below it", "[collision][terrain]") {
    const world::generation::HeightmapGenerator heightmap(1337);
    TerrainColliderParams params;
    params.async = false;
    TerrainCollider collider(heightmap, params);
    std::mt19937 rng(5);
    std::uniform_real_distribution<float> coord(-150.0f, 150.0f);
    double refreshMs = 0.0;
    int refreshes = 0;
    float worstHover = 0.0f;
    for (int i = 0; i < 40; ++i) {
        const float x = coord(rng);
        const float z = coord(rng);
        const Aabb body = settle(collider, x, z);
        if (collider.last_refresh_ms() > 0.0) {
            ++refreshes;
            refreshMs += collider.last_refresh_ms();
        }
        // Feet at or above the surface under the body's center, and resting on the highest
        // column under the footprint (a box stands on its uphill edge) within one cache cell's
        // slope allowance plus a voxel -- never floating.
        const float hCenter = collider.ground_height(x, z);
        const float hMax = footprint_max_height(collider, body);
        CHECK(body.min.y >= hCenter - 1.0e-3f);
        CHECK(body.min.y >= hMax - 0.05f);
        CHECK(body.min.y < hMax + 0.25f);
        worstHover = std::max(worstHover, body.min.y - hMax);
    }
    std::printf("terrain collider: %d cache refreshes, %.2f ms each; worst hover above the footprint's "
                "highest column %.3f m\n",
                refreshes, refreshes > 0 ? refreshMs / refreshes : 0.0, worstHover);
}

TEST_CASE("walking into a steep slope is blocked or climbed, never tunneled", "[collision][terrain]") {
    const world::generation::HeightmapGenerator heightmap(1337);
    TerrainColliderParams params;
    params.async = false;
    TerrainCollider collider(heightmap, params);
    // Find a column whose neighbor 1 m away is > 1.5 m higher: a real cliff face.
    std::mt19937 rng(9);
    std::uniform_real_distribution<float> coord(-200.0f, 200.0f);
    bool found = false;
    float x0 = 0.0f;
    float z0 = 0.0f;
    for (int i = 0; i < 20000 && !found; ++i) {
        const float x = coord(rng);
        const float z = coord(rng);
        const float h0 = heightmap.height_at(x, z);
        const float h1 = heightmap.height_at(x + 1.0f, z);
        if (h0 > 2.0f && h1 - h0 > 1.5f) {
            x0 = x;
            z0 = z;
            found = true;
        }
    }
    REQUIRE(found);
    Aabb body = settle(collider, x0, z0);
    SweepParams sweep;
    float traveled = 0.0f;
    for (int step = 0; step < 100; ++step) {
        const SweepResult r = move_and_slide(collider, body, glm::vec3{0.05f, -0.2f, 0.0f}, sweep);
        body = body.translated(r.delta);
        traveled += r.delta.x;
        REQUIRE_FALSE(collider.overlaps_solid(body));
    }
    // Either the cliff stopped us short of 5 m, or stepping climbed it -- both keep the body out
    // of the ground; what must not happen is the body ending inside the hill.
    const float hMax = footprint_max_height(collider, body);
    CHECK(body.min.y >= hMax - 0.05f);
    std::printf("cliff walk: traveled %.2f m of 5, feet %.2f m above the footprint's highest column\n",
                traveled, body.min.y - hMax);
}

TEST_CASE("tree trunks are solid and canopies are not", "[collision][terrain]") {
    const world::generation::HeightmapGenerator heightmap(1337);
    TerrainColliderParams params;
    params.async = false;
    TerrainCollider collider(heightmap, params);
    // Find a Round or Conifer tree standing on ground that is roughly level 2 m to its -x side
    // (so a body can stand there and walk into it).
    bool found = false;
    world::generation::TreePlacement tree;
    for (std::int32_t cz = -4; cz <= 4 && !found; ++cz) {
        for (std::int32_t cx = -4; cx <= 4 && !found; ++cx) {
            for (const auto& t : world::generation::compute_tree_placements(cx, cz, params.seed, heightmap)) {
                world::generation::TrunkBox trunk;
                if (!world::generation::tree_trunk(t, trunk) || trunk.y1 - trunk.y0 < 2.5f) {
                    continue;
                }
                const float hStart = heightmap.height_at(t.world_x - 2.0f, t.world_z);
                if (std::abs(hStart - t.base_height) < 0.5f) {
                    tree = t;
                    found = true;
                    break;
                }
            }
        }
    }
    REQUIRE(found);
    world::generation::TrunkBox trunk;
    REQUIRE(world::generation::tree_trunk(tree, trunk));
    const glm::vec3 trunkCenter{tree.world_x, 0.5f * (trunk.y0 + trunk.y1), tree.world_z};
    collider.refresh(trunkCenter);
    CHECK(collider.tree_count() > 0);
    // A box around the trunk's middle overlaps; the same box moved high into the canopy does not.
    const Aabb atTrunk{trunkCenter - glm::vec3{0.1f}, trunkCenter + glm::vec3{0.1f}};
    CHECK(collider.overlaps_solid(atTrunk));
    std::array<world::generation::CanopyLobe, world::generation::kMaxCanopyLobes> lobes{};
    const std::size_t lobeCount = world::generation::tree_canopy_lobes(tree, lobes);
    REQUIRE(lobeCount > 0);
    const glm::vec3 canopyTop =
        lobes[lobeCount - 1].center + glm::vec3{0.0f, lobes[lobeCount - 1].rv * 0.5f, 0.0f};
    const Aabb atCanopy{canopyTop - glm::vec3{0.1f}, canopyTop + glm::vec3{0.1f}};
    // Only the canopy is there (no terrain reaches it), so the whole query must say free.
    CHECK_FALSE(collider.overlaps_solid(atCanopy));

    // Walking into the trunk from 2 m away stops at its face (at boost speed too: 1.1 m steps).
    for (const float stepX : {0.05f, 1.1f}) {
        Aabb body = settle(collider, tree.world_x - 2.0f, tree.world_z);
        SweepParams sweep;
        sweep.step_height = 0.0f;
        for (int i = 0; i < 100; ++i) {
            const SweepResult r = move_and_slide(collider, body, glm::vec3{stepX, -0.1f, 0.0f}, sweep);
            body = body.translated(r.delta);
            REQUIRE_FALSE(collider.overlaps_solid(body));
        }
        CHECK(body.max.x <= tree.world_x - trunk.half_width + 1.0e-3f);
        CHECK(body.max.x > tree.world_x - trunk.half_width - 0.05f);
    }
}
