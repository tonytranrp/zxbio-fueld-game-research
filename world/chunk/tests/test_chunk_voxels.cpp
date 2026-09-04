#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_voxels.hpp"

using namespace world::chunk;

TEST_CASE("A freshly constructed ChunkVoxels is homogeneous Air", "[chunk]") {
    ChunkVoxels voxels;
    REQUIRE(voxels.is_homogeneous());
    REQUIRE(voxels.palette_size() == 1);
    REQUIRE(voxels.bits_per_voxel() == 0);
    REQUIRE(voxels.at(0) == MaterialID::Air);
    REQUIRE(voxels.at(kVoxelsPerChunk - 1) == MaterialID::Air);
}

TEST_CASE("fill_uniform sets every voxel to one material in O(1) state", "[chunk]") {
    ChunkVoxels voxels;
    voxels.fill_uniform(MaterialID::Stone);
    REQUIRE(voxels.is_homogeneous());
    REQUIRE(voxels.palette_size() == 1);
    REQUIRE(voxels.bits_per_voxel() == 0);
    REQUIRE(voxels.at(0) == MaterialID::Stone);
    REQUIRE(voxels.at(kVoxelsPerChunk / 2) == MaterialID::Stone);
    REQUIRE(voxels.at(kVoxelsPerChunk - 1) == MaterialID::Stone);
}

TEST_CASE("Setting a second distinct material promotes to 1 bit/voxel and preserves prior values", "[chunk]") {
    ChunkVoxels voxels; // starts as [Air]
    voxels.set(0, MaterialID::Stone); // introduces the 2nd distinct material -> promotion to 1 bit
    REQUIRE(voxels.palette_size() == 2);
    REQUIRE(voxels.bits_per_voxel() == 1);
    REQUIRE(voxels.at(0) == MaterialID::Stone);
    // Every other voxel was never explicitly set -- must still read back as the original Air.
    REQUIRE(voxels.at(1) == MaterialID::Air);
    REQUIRE(voxels.at(kVoxelsPerChunk - 1) == MaterialID::Air);
}

TEST_CASE("Every palette-promotion boundary preserves previously-set voxel values", "[chunk]") {
    // M1.2 brief §1.3: the correctness-critical part is that promotion re-packs every
    // EXISTING voxel at the new bit width without corrupting it. This walks through all four
    // boundaries the brief names (1->2, 2->3, 4->5, 16->17 distinct materials) by introducing one
    // new distinct material at a time, re-verifying every previously-set voxel after each one --
    // not just at the boundary steps, every step.
    ChunkVoxels voxels; // [Air], bits=0

    std::vector<std::pair<std::size_t, MaterialID>> setVoxels;

    // Distinct materials 2..17 overall (index 0 is the implicit Air the chunk starts with).
    // Synthetic values via static_cast, not the 4 named MaterialID enumerators, specifically to
    // exercise the 16->17 boundary generically -- well-defined for any value representable in
    // the enum's fixed underlying type (std::uint8_t), C++17 onward.
    for (int distinctIndex = 1; distinctIndex <= 16; ++distinctIndex) {
        const auto material = static_cast<MaterialID>(distinctIndex);
        const std::size_t localIndex = static_cast<std::size_t>(distinctIndex) * 37; // spread out, arbitrary

        voxels.set(localIndex, material);
        setVoxels.emplace_back(localIndex, material);

        INFO("after introducing distinct material index " << distinctIndex << " (palette size "
                                                            << voxels.palette_size() << ")");
        for (const auto& [idx, mat] : setVoxels) {
            REQUIRE(voxels.at(idx) == mat);
        }
    }

    REQUIRE(voxels.palette_size() == 17);
    REQUIRE(voxels.bits_per_voxel() == 8);
}

TEST_CASE("Re-setting an already-present material does not grow the palette", "[chunk]") {
    ChunkVoxels voxels;
    voxels.set(0, MaterialID::Stone);
    voxels.set(1, MaterialID::Stone);
    voxels.set(2, MaterialID::Stone);
    REQUIRE(voxels.palette_size() == 2); // Air + Stone, not 4
    REQUIRE(voxels.at(0) == MaterialID::Stone);
    REQUIRE(voxels.at(1) == MaterialID::Stone);
    REQUIRE(voxels.at(2) == MaterialID::Stone);
}
