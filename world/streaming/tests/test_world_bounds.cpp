#include <algorithm>
#include <cstdint>
#include <tuple>

#include <catch2/catch_test_macros.hpp>

#include "world/streaming/world_bounds.hpp"

using namespace world::streaming;
using world::chunk::ChunkCoord;

// Goal 128's own check: every chunk in bounds is generated exactly once, verified by a real count,
// not assumed. chunks_in_bounds() is the pure shape query WorldLoader drives its one-time
// generation pass from -- testing it directly here is cheaper and more precise than only ever
// exercising it through the full GPU-backed loader.
TEST_CASE("chunks_in_bounds covers the full column x layer volume exactly once", "[streaming][bounds]") {
    const WorldBounds bounds{/*radius_chunks=*/2, /*y_min=*/-1, /*y_max=*/1};
    const auto coords = chunks_in_bounds(bounds);

    const std::size_t expectedColumns = 5 * 5; // -2..2 inclusive on both X and Z
    const std::size_t expectedLayers = 3;      // -1..1 inclusive
    REQUIRE(coords.size() == expectedColumns * expectedLayers);

    // No duplicates: every coordinate appears exactly once.
    auto sorted = coords;
    std::sort(sorted.begin(), sorted.end(), [](const ChunkCoord& a, const ChunkCoord& b) {
        return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
    });
    CHECK(std::adjacent_find(sorted.begin(), sorted.end(), [](const ChunkCoord& a, const ChunkCoord& b) {
              return a.x == b.x && a.y == b.y && a.z == b.z;
          }) == sorted.end());

    // Every coordinate is genuinely in bounds, and the extremes are actually reached (not an
    // off-by-one that quietly clips the edge column).
    bool sawMinCorner = false;
    bool sawMaxCorner = false;
    for (const ChunkCoord& c : coords) {
        REQUIRE(c.x >= -bounds.radius_chunks);
        REQUIRE(c.x <= bounds.radius_chunks);
        REQUIRE(c.z >= -bounds.radius_chunks);
        REQUIRE(c.z <= bounds.radius_chunks);
        REQUIRE(c.y >= bounds.y_min);
        REQUIRE(c.y <= bounds.y_max);
        sawMinCorner |= (c.x == -bounds.radius_chunks && c.z == -bounds.radius_chunks && c.y == bounds.y_min);
        sawMaxCorner |= (c.x == bounds.radius_chunks && c.z == bounds.radius_chunks && c.y == bounds.y_max);
    }
    CHECK(sawMinCorner);
    CHECK(sawMaxCorner);
}

TEST_CASE("a single-chunk world (radius 0) is exactly its own y-band", "[streaming][bounds]") {
    const WorldBounds bounds{/*radius_chunks=*/0, /*y_min=*/0, /*y_max=*/2};
    const auto coords = chunks_in_bounds(bounds);
    REQUIRE(coords.size() == 3);
    for (const ChunkCoord& c : coords) {
        CHECK(c.x == 0);
        CHECK(c.z == 0);
    }
}
