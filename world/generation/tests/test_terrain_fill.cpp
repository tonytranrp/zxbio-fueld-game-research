#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_voxels.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"

using namespace world::chunk;
using namespace world::generation;

namespace {
// A fixed seed used throughout -- these tests assert SHAPE (which materials appear, and
// homogeneity), not exact per-voxel values, since the noise function's specific output isn't the
// thing under test here (that's test_determinism.cpp's job). The noise is remapped to a
// world-Y surface height in roughly [-64, 64] (heightmap_generator.cpp's kBaseHeight/kAmplitude).
constexpr int kSeed = 1337;
} // namespace

TEST_CASE("A chunk far above the generated surface is a homogeneous air chunk", "[generation]") {
    HeightmapGenerator heightmap(kSeed);
    // Chunk y=10 -> world Y 320..351, far above the ~[-64,64] surface range.
    Chunk chunk(ChunkCoord{0, 10, 0});
    fill_terrain(chunk, heightmap);

    REQUIRE(chunk.voxels().is_homogeneous());
    REQUIRE(chunk.voxels().at(0) == MaterialID::Air);
}

TEST_CASE("A chunk far below the generated surface is a homogeneous stone chunk", "[generation]") {
    HeightmapGenerator heightmap(kSeed);
    // Chunk y=-10 -> world Y -320..-289, far below the ~[-64,64] surface range.
    Chunk chunk(ChunkCoord{0, -10, 0});
    fill_terrain(chunk, heightmap);

    REQUIRE(chunk.voxels().is_homogeneous());
    REQUIRE(chunk.voxels().at(0) == MaterialID::Stone);
}

TEST_CASE("A chunk straddling the generated surface contains both air and stone", "[generation]") {
    HeightmapGenerator heightmap(kSeed);
    // Chunk y=0 -> world Y 0..31, squarely inside the ~[-64,64] surface range for this seed.
    Chunk chunk(ChunkCoord{0, 0, 0});
    fill_terrain(chunk, heightmap);

    REQUIRE_FALSE(chunk.voxels().is_homogeneous());
    bool sawAir = false;
    bool sawStone = false;
    for (std::size_t i = 0; i < kVoxelsPerChunk; ++i) {
        const MaterialID m = chunk.voxels().at(i);
        sawAir = sawAir || (m == MaterialID::Air);
        sawStone = sawStone || (m == MaterialID::Stone);
    }
    REQUIRE(sawAir);
    REQUIRE(sawStone);
}

TEST_CASE("A chunk straddling sea level over open air contains water but no stone", "[generation]") {
    HeightmapGenerator heightmap(kSeed);
    // Same air-only chunk as the first test (world Y 320..351), but with sea level set inside
    // that range -- water should appear (overriding air) with no stone (there's no ground here).
    TerrainFillParams params;
    params.seaLevel = 335;
    Chunk chunk(ChunkCoord{0, 10, 0});
    fill_terrain(chunk, heightmap, params);

    bool sawWater = false;
    bool sawStone = false;
    for (std::size_t i = 0; i < kVoxelsPerChunk; ++i) {
        const MaterialID m = chunk.voxels().at(i);
        sawWater = sawWater || (m == MaterialID::Water);
        sawStone = sawStone || (m == MaterialID::Stone);
    }
    REQUIRE(sawWater);
    REQUIRE_FALSE(sawStone); // water never overrides solid ground -- there's no ground to override here
}
