#include <cmath>

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
}
