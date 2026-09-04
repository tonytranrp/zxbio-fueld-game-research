#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_coord.hpp"

using namespace world::chunk;

TEST_CASE("Coordinate math handles negative world coordinates correctly", "[chunk]") {
    // The canonical case flagged in M1.2 brief §3: naive truncating '/' and '%' get this
    // wrong (X/32==0, X%32==-1 for X=-1) the moment a coordinate goes negative.
    REQUIRE(world_to_chunk(-1) == -1);
    REQUIRE(world_to_local(-1) == 31);

    REQUIRE(world_to_chunk(0) == 0);
    REQUIRE(world_to_local(0) == 0);

    REQUIRE(world_to_chunk(31) == 0);
    REQUIRE(world_to_local(31) == 31);

    REQUIRE(world_to_chunk(32) == 1);
    REQUIRE(world_to_local(32) == 0);

    REQUIRE(world_to_chunk(-32) == -1);
    REQUIRE(world_to_local(-32) == 0);

    REQUIRE(world_to_chunk(-33) == -2);
    REQUIRE(world_to_local(-33) == 31);
}

TEST_CASE("ChunkCoord equality and hashing work for use as a map key", "[chunk]") {
    const ChunkCoord a{1, 2, 3};
    const ChunkCoord b{1, 2, 3};
    const ChunkCoord c{1, 2, 4};

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);

    const std::hash<ChunkCoord> hasher;
    REQUIRE(hasher(a) == hasher(b));
}

TEST_CASE("local_index is X-innermost, matching FastNoise2's own grid output convention", "[chunk]") {
    REQUIRE(local_index(0, 0, 0) == 0);
    REQUIRE(local_index(1, 0, 0) == 1);
    REQUIRE(local_index(0, 1, 0) == static_cast<std::size_t>(kChunkSize));
    REQUIRE(local_index(0, 0, 1) == static_cast<std::size_t>(kChunkSize) * static_cast<std::size_t>(kChunkSize));
}
