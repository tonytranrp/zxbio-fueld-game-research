#include <catch2/catch_test_macros.hpp>

#include "world/svo/brick.hpp"

using namespace world::svo;
using world::chunk::MaterialID;

TEST_CASE("brick voxel index is X-innermost with 8^3 extent", "[svo][brick]") {
    CHECK(brick_voxel_index(0, 0, 0) == 0);
    CHECK(brick_voxel_index(1, 0, 0) == 1);
    CHECK(brick_voxel_index(0, 1, 0) == 8);
    CHECK(brick_voxel_index(0, 0, 1) == 64);
    CHECK(brick_voxel_index(7, 7, 7) == kBrickVoxels - 1);
    CHECK(kBrickWords == 144);
}

TEST_CASE("brick set/get round-trips every voxel and keeps the occupancy mask in sync", "[svo][brick]") {
    Brick brick;
    REQUIRE(brick.empty());
    REQUIRE(brick.is_homogeneous());
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const auto m = static_cast<MaterialID>((x + 3 * y + 5 * z) % 8);
                brick.set(x, y, z, m);
            }
        }
    }
    std::size_t expectedOccupied = 0;
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const auto m = static_cast<MaterialID>((x + 3 * y + 5 * z) % 8);
                CHECK(brick.at(x, y, z) == m);
                CHECK(brick.occupied(x, y, z) == (m != MaterialID::Air));
                // Raw-word accessors (what the traversal uses) agree with the object accessors.
                CHECK(brick_word_material(brick.words().data(), brick_voxel_index(x, y, z)) == m);
                CHECK(brick_word_occupied(brick.words().data(), brick_voxel_index(x, y, z)) ==
                      (m != MaterialID::Air));
                expectedOccupied += (m != MaterialID::Air) ? 1u : 0u;
            }
        }
    }
    CHECK(brick.occupied_count() == expectedOccupied);
    CHECK_FALSE(brick.is_homogeneous());

    // Overwriting with Air clears the mask bit again.
    brick.set(3, 3, 3, MaterialID::Air);
    CHECK_FALSE(brick.occupied(3, 3, 3));
    CHECK(brick.at(3, 3, 3) == MaterialID::Air);
}

TEST_CASE("brick homogeneity and representative follow the topmost-occupied-per-column rule",
          "[svo][brick]") {
    Brick brick;
    // Stone slab with a grass skin on top, one column bare rock.
    for (int z = 0; z < 8; ++z) {
        for (int x = 0; x < 8; ++x) {
            for (int y = 0; y < 3; ++y) {
                brick.set(x, y, z, MaterialID::Stone);
            }
            if (!(x == 0 && z == 0)) {
                brick.set(x, 3, z, MaterialID::Grass);
            }
        }
    }
    CHECK(brick.representative() == MaterialID::Grass); // 63 grass tops vs 1 stone top
    CHECK_FALSE(brick.is_homogeneous());

    Brick uniform;
    for (std::size_t i = 0; i < kBrickVoxels; ++i) {
        uniform.set(i, MaterialID::Dirt);
    }
    CHECK(uniform.is_homogeneous());
    CHECK(uniform.representative() == MaterialID::Dirt);

    CHECK(Brick{}.representative() == MaterialID::Air);
}
