// Prompt 007 goal 336's incidental find, pinned. `bake_playable_field` exists because the bake used
// to live in an anonymous namespace in `app/src/svo_world.cpp`, which meant every OTHER caller --
// `tools/svo_render`, the harness's own `pose_ground` resolver -- was silently working in the
// pre-Prompt-006 noise world.
//
// The failure had no symptom a test would have caught, because each program was internally
// consistent. It only showed when a CPU frame and a GPU frame of the same coordinates were put side
// by side and turned out to be two unrelated landscapes. What CAN be tested is the property that
// would have made it impossible: one function, deterministic, and a surface that is measurably not
// the bare noise -- so a caller who forgets it is looking at something provably different rather
// than something subtly different.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/heightmap_generator.hpp"

using namespace world::generation;

TEST_CASE("the playable field bake is deterministic", "[field][bake]") {
    const auto a = field::bake_playable_field(1337);
    const auto b = field::bake_playable_field(1337);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a->cells() == b->cells());
    // Geometry included: `recentre_on_land` moves the origin, and an origin that moved differently
    // between two runs would put the same world coordinate on different ground.
    REQUIRE(a->geometry().origin_x == b->geometry().origin_x);
    REQUIRE(a->geometry().origin_z == b->geometry().origin_z);
    REQUIRE(a->geometry().cell_size == b->geometry().cell_size);

    const HeightmapGenerator withA(1337, a);
    const HeightmapGenerator withB(1337, b);
    for (int i = 0; i < 400; ++i) {
        const float x = -200.0f + static_cast<float>(i) * 1.37f;
        const float z = 150.0f - static_cast<float>(i) * 0.91f;
        REQUIRE(withA.height_at(x, z) == withB.height_at(x, z));
    }
}

TEST_CASE("forgetting the field is not a subtle difference", "[field][bake]") {
    // The number that makes the bug worth a test: how far apart the two worlds are. If this were a
    // metre it would be a nuance; it is tens of metres, which is why a pose resolved in one world
    // spawns a body inside a hill in the other.
    const HeightmapGenerator withField(1337, field::bake_playable_field(1337));
    const HeightmapGenerator bare(1337);

    double worst = 0.0;
    double mean = 0.0;
    int n = 0;
    for (int ix = -20; ix <= 20; ++ix) {
        for (int iz = -20; iz <= 20; ++iz) {
            const auto x = static_cast<float>(ix) * 8.0f;
            const auto z = static_cast<float>(iz) * 8.0f;
            const double d = std::abs(static_cast<double>(withField.height_at(x, z)) -
                                      static_cast<double>(bare.height_at(x, z)));
            worst = std::max(worst, d);
            mean += d;
            ++n;
        }
    }
    mean /= n;
    INFO("over a 320 m square: mean |difference| " << mean << " m, worst " << worst << " m");
    CHECK(mean > 2.0);
    CHECK(worst > 5.0);
}

TEST_CASE("the playable region is on land, which is what the recentre is for", "[field][bake]") {
    // Goal 321's property, restated where a tool author can find it: after the bake, the 512 m
    // window at world (0, 0) is not a bay.
    const HeightmapGenerator withField(1337, field::bake_playable_field(1337));
    int land = 0;
    int total = 0;
    for (int ix = -16; ix <= 16; ++ix) {
        for (int iz = -16; iz <= 16; ++iz) {
            ++total;
            if (withField.height_at(static_cast<float>(ix) * 16.0f, static_cast<float>(iz) * 16.0f) > 0.0f) {
                ++land;
            }
        }
    }
    const double fraction = static_cast<double>(land) / total;
    INFO("land fraction over the 512 m playable window: " << 100.0 * fraction << "%");
    CHECK(fraction > 0.9);
}
