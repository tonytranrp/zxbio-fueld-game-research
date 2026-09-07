#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "../src/aim_query.hpp"

using world::chunk::MaterialID;

// Goal 84's check: the crosshair query against known columns of the real generator.
TEST_CASE("aim query classifies known columns like the terrain fill", "[aim]") {
    const world::generation::HeightmapGenerator generator(1337);

    SECTION("straight down over land hits the surface with a land material") {
        // Find a genuinely above-sea column by scanning a line (deterministic for seed 1337).
        float lx = 0.0f;
        float lz = 0.0f;
        float surface = -1000.0f;
        for (float x = 0.0f; x < 200.0f && surface < 5.0f; x += 7.0f) {
            for (float z = 0.0f; z < 200.0f && surface < 5.0f; z += 7.0f) {
                const float h = generator.height_at(x, z);
                if (h > 5.0f) {
                    lx = x;
                    lz = z;
                    surface = h;
                }
            }
        }
        REQUIRE(surface > 5.0f);

        const app::AimHit hit = app::query_aim(generator, {lx, surface + 40.0f, lz}, {0.0f, -1.0f, 0.0f});
        REQUIRE(hit.hit);
        CHECK(std::abs(hit.position.y - surface) < 0.6f); // refined onto the surface
        // Above the beach band, the material must be the grass/stone slope rule -- never water/sand.
        CHECK((hit.material == MaterialID::Grass || hit.material == MaterialID::Stone));
    }

    SECTION("straight down over deep water hits the water plane") {
        float lx = 0.0f;
        float lz = 0.0f;
        float surface = 1000.0f;
        for (float x = 0.0f; x < 300.0f && surface > -6.0f; x += 7.0f) {
            for (float z = 0.0f; z < 300.0f && surface > -6.0f; z += 7.0f) {
                const float h = generator.height_at(x, z);
                if (h < -6.0f) {
                    lx = x;
                    lz = z;
                    surface = h;
                }
            }
        }
        REQUIRE(surface < -6.0f);

        const app::AimHit hit = app::query_aim(generator, {lx, 30.0f, lz}, {0.0f, -1.0f, 0.0f});
        REQUIRE(hit.hit);
        CHECK(hit.material == MaterialID::Water);
        CHECK(std::abs(hit.position.y) < 0.6f); // the sea-level plane, not the seabed
    }

    SECTION("aiming at open sky misses") {
        const app::AimHit hit = app::query_aim(generator, {0.0f, 120.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        CHECK(!hit.hit);
    }

    SECTION("the reported distance is the real distance to the hit") {
        // Prompt 001 A4: the overlay line gained a range, so it has to be one.
        const app::AimHit hit = app::query_aim(generator, {0.0f, 200.0f, 0.0f}, {0.0f, -1.0f, 0.0f});
        REQUIRE(hit.hit);
        CHECK(std::abs(hit.distance - glm::length(hit.position - glm::vec3{0.0f, 200.0f, 0.0f})) < 0.01f);
    }
}

// Prompt 001 A4: before this the query saw only the height field, so a trunk or a canopy filling
// the screen reported as whatever hillside stood BEHIND it.
TEST_CASE("aim query sees trees", "[aim][trees]") {
    constexpr int kSeed = 1337;
    const world::generation::HeightmapGenerator generator(kSeed);
    const app::TreeLookup trees(generator, kSeed);

    // Find a real tree the generator actually placed, rather than inventing one: the point of the
    // test is that the query agrees with the world the voxelizer builds from the same function.
    // The tree also has to be one you can actually SEE from 6 m away -- the first draft of this
    // test picked the first trunk it found and fired a ray from inside a hillside, which correctly
    // reported Stone at 2 mm. Requiring a clear line of sight (checked with the terrain-only query)
    // makes the test measure what it claims to.
    constexpr float kStandoff = 6.0f;
    std::optional<world::generation::TreePlacement> found;
    glm::vec3 origin{0.0f};
    glm::vec3 trunkMid{0.0f};
    for (std::int32_t cx = 0; cx < 8 && !found; ++cx) {
        for (std::int32_t cz = 0; cz < 8 && !found; ++cz) {
            for (const auto& tree : world::generation::compute_tree_placements(cx, cz, kSeed, generator)) {
                world::generation::TrunkBox trunk{};
                if (!world::generation::tree_trunk(tree, trunk)) {
                    continue; // Shrub: no trunk to aim at
                }
                const glm::vec3 mid{tree.world_x, (trunk.y0 + trunk.y1) * 0.5f, tree.world_z};
                const glm::vec3 eye = mid + glm::vec3{kStandoff, 0.0f, 0.0f};
                if (generator.height_at(eye.x, eye.z) >= eye.y) {
                    continue; // the viewpoint is underground
                }
                const app::AimHit terrainOnly = app::query_aim(generator, eye, {-1.0f, 0.0f, 0.0f});
                if (terrainOnly.hit && terrainOnly.distance < kStandoff + 1.0f) {
                    continue; // a hillside stands between the eye and the trunk
                }
                found = tree;
                origin = eye;
                trunkMid = mid;
                break;
            }
        }
    }
    REQUIRE(found.has_value());

    SECTION("a horizontal ray into a trunk reads Wood, at the trunk's range") {
        const app::AimHit hit = app::query_aim(generator, origin, {-1.0f, 0.0f, 0.0f}, 300.0f, &trees);
        REQUIRE(hit.hit);
        CHECK(hit.material == MaterialID::Wood);
        CHECK(hit.distance > kStandoff - 1.0f);
        CHECK(hit.distance < kStandoff + 1.0f);
    }

    SECTION("without the tree source the same ray does not report Wood") {
        // The regression this closes, stated as a test: the pre-A4 query cannot see the trunk.
        const app::AimHit hit = app::query_aim(generator, origin, {-1.0f, 0.0f, 0.0f});
        CHECK(hit.material != MaterialID::Wood);
    }

    SECTION("straight down through the canopy reads Leaves before the ground") {
        std::array<world::generation::CanopyLobe, world::generation::kMaxCanopyLobes> lobes{};
        const std::size_t count = world::generation::tree_canopy_lobes(*found, lobes);
        REQUIRE(count > 0);
        const app::AimHit hit = app::query_aim(generator, lobes[0].center + glm::vec3{0.0f, 30.0f, 0.0f},
                                               {0.0f, -1.0f, 0.0f}, 300.0f, &trees);
        REQUIRE(hit.hit);
        CHECK(hit.material == MaterialID::Leaves);
        CHECK(hit.position.y > found->base_height); // above the ground it would otherwise have hit
    }
}
