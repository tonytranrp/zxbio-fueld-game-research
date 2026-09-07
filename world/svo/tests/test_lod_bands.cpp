// Prompt 004 goal 257: the band function, whose only job is to change RARELY.
//
// The property that matters is not "is the band right" -- any monotone function of distance would
// be defensible -- but "does a small camera move leave almost every cell's band alone". That is
// what makes incremental rebuild possible, and it is what these cases assert.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "world/svo/lod_bands.hpp"

using namespace world::svo;

TEST_CASE("band 0 covers everything inside the radius, including the camera's own cell", "[svo][lod]") {
    CHECK(lod_band(0.0f, 4.0f) == 0);
    CHECK(lod_band(1.0f, 4.0f) == 0);
    CHECK(lod_band(3.999f, 4.0f) == 0);
    CHECK(lod_band(4.0f, 4.0f) == 0); // exactly at the boundary is still fine detail

    // A cell the camera stands INSIDE is band 0 however big the cell is -- which is why the
    // distance is to the cell's nearest point and not to its centre.
    CHECK(cell_band(glm::vec3{40.0f, 40.0f, 40.0f}, glm::ivec3{1, 1, 1}, 32.0f, 4.0f) == 0);
}

TEST_CASE("each band doubles the distance and the voxel edge", "[svo][lod]") {
    CHECK(lod_band(4.1f, 4.0f) == 1);
    CHECK(lod_band(8.1f, 4.0f) == 2);
    CHECK(lod_band(16.1f, 4.0f) == 3);
    CHECK(lod_band(256.1f, 4.0f) == 7);

    const float finest = 1.0f / 128.0f;
    CHECK(band_voxel_edge(0, finest, 32.0f) == Catch::Approx(finest));
    CHECK(band_voxel_edge(1, finest, 32.0f) == Catch::Approx(finest * 2.0f));
    CHECK(band_voxel_edge(4, finest, 32.0f) == Catch::Approx(finest * 16.0f));
    // Never coarser than the cell itself, however far away it is.
    CHECK(band_voxel_edge(40, finest, 32.0f) == Catch::Approx(32.0f));
}

TEST_CASE("a degenerate distance does not become a huge band", "[svo][lod]") {
    // NaN through `!(d > r)` rather than `d <= r`: a NaN distance must be band 0, not a band that
    // asks for a voxel the size of the world.
    CHECK(lod_band(std::nanf(""), 4.0f) == 0);
    CHECK(lod_band(-1.0f, 4.0f) == 0);
    CHECK(lod_band(10.0f, 0.0f) >= 0); // a zero radius must not divide by zero
}

TEST_CASE("a small camera move leaves almost every cell's band alone", "[svo][lod]") {
    // THE PROPERTY THE WHOLE DESIGN RESTS ON. Under the builder's continuous LOD every cell's
    // content changes when the camera moves at all. Under bands, only the shell that crossed a
    // boundary does -- and this measures how thin that shell is on the shipping configuration.
    constexpr float kCellEdge = 32.0f;
    constexpr float kLodRadius = 4.0f;
    constexpr int kPerAxis = 16; // a 512 m region

    const auto bands_for = [&](glm::vec3 camera) {
        std::vector<int> out;
        out.reserve(static_cast<std::size_t>(kPerAxis) * kPerAxis * kPerAxis);
        for (int z = 0; z < kPerAxis; ++z) {
            for (int y = 0; y < kPerAxis; ++y) {
                for (int x = 0; x < kPerAxis; ++x) {
                    out.push_back(cell_band(camera, glm::ivec3{x, y, z}, kCellEdge, kLodRadius));
                }
            }
        }
        return out;
    };

    const glm::vec3 start{250.0f, 250.0f, 250.0f};
    const std::vector<int> before = bands_for(start);

    // Eight metres is the rebuild trigger goal 249 settled on, so this is the real question:
    // how many cells does ONE rebuild's worth of camera motion actually dirty?
    const std::vector<int> after = bands_for(start + glm::vec3{8.0f, 0.0f, 0.0f});
    REQUIRE(before.size() == after.size());
    std::size_t changed = 0;
    for (std::size_t i = 0; i < before.size(); ++i) {
        changed += before[i] != after[i] ? 1u : 0u;
    }
    const double fraction = static_cast<double>(changed) / static_cast<double>(before.size());
    // Under the continuous rule this would be 100%. The assertion is deliberately loose -- the
    // exact number is reported in research section 20 and depends on where in the grid the camera
    // is -- but it must be a minority, or incremental rebuild is not worth having.
    CHECK(fraction < 0.35);
    CHECK(changed > 0); // and it must actually do something, or the test proves nothing
}

TEST_CASE("standing still dirties nothing at all", "[svo][lod]") {
    // The other half: a camera that has not moved must not re-level a single cell, or the grid
    // would rebuild forever at rest.
    constexpr float kCellEdge = 32.0f;
    const glm::vec3 camera{123.25f, 60.5f, -77.75f};
    for (int x = -4; x < 12; ++x) {
        for (int y = -2; y < 6; ++y) {
            const glm::ivec3 coord{x, y, 3};
            CHECK(cell_band(camera, coord, kCellEdge, 4.0f) == cell_band(camera, coord, kCellEdge, 4.0f));
        }
    }
}
