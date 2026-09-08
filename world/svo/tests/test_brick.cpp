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
    CHECK(kBrickWords == 70); // goal 258: 144 before palette compression
}

TEST_CASE("exposed face sum points out of the solid and ignores the brick boundary", "[svo][brick]") {
    // Bottom half solid: the only exposed faces inside the brick are the 64 top faces at y = 3.
    Brick floor;
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 8; ++x) {
                floor.set(x, y, z, MaterialID::Dirt);
            }
        }
    }
    CHECK(floor.exposed_face_sum() == glm::ivec3{0, 64, 0});

    // Left half solid (x < 4): 64 +x faces at x = 3.
    Brick wall;
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 4; ++x) {
                wall.set(x, y, z, MaterialID::Stone);
            }
        }
    }
    CHECK(wall.exposed_face_sum() == glm::ivec3{64, 0, 0});

    // A 45-degree ramp rising toward +x (solid where y < x): 8 exposed top faces per z-row are
    // matched by 8 exposed -x faces (the risers), so the sum is the ramp normal (-1, 1, 0) * 56.
    Brick ramp;
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                if (y < x) {
                    ramp.set(x, y, z, MaterialID::Sand);
                }
            }
        }
    }
    const glm::ivec3 rampSum = ramp.exposed_face_sum();
    CHECK(rampSum.x == -rampSum.y);
    CHECK(rampSum.y > 0);
    CHECK(rampSum.z == 0);

    // One isolated voxel in the middle exposes all six faces, which cancel.
    Brick dot;
    dot.set(3, 3, 3, MaterialID::Wood);
    CHECK(dot.exposed_face_sum() == glm::ivec3{0, 0, 0});
    // A full brick exposes nothing inside.
    Brick full;
    for (std::size_t i = 0; i < kBrickVoxels; ++i) {
        full.set(i, MaterialID::Stone);
    }
    CHECK(full.exposed_face_sum() == glm::ivec3{0, 0, 0});
}

TEST_CASE("brick set/get round-trips every voxel and keeps the occupancy mask in sync", "[svo][brick]") {
    Brick brick;
    REQUIRE(brick.empty());
    REQUIRE(brick.is_homogeneous());
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const auto m = static_cast<MaterialID>((x + 3 * y + 5 * z) %
                                                       static_cast<int>(world::chunk::kMaterialCount));
                brick.set(x, y, z, m);
            }
        }
    }
    std::size_t expectedOccupied = 0;
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const auto m = static_cast<MaterialID>((x + 3 * y + 5 * z) %
                                                       static_cast<int>(world::chunk::kMaterialCount));
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

// ---------------------------------------------------------------- goal 258: palette compression

TEST_CASE("brick is palette-compressed to 70 words and the layout constants agree", "[svo][brick]") {
    // The number the goal is about: 576 B per brick became 280 B. Asserted rather than commented
    // so a future layout change has to restate it deliberately.
    CHECK(kBrickWords == 70);
    CHECK(kBrickWords * sizeof(std::uint32_t) == 280);
    CHECK(kBrickIndexWord0 == kBrickMaskWords);
    CHECK(kBrickIndexWords == 52);
    CHECK(kBrickPaletteWord0 == 68);
    // Ten indices per word leaves two bits unused; that waste is the point (one load per fetch).
    CHECK(kBrickIndicesPerWord * kBrickPaletteBits == 30);
    CHECK(kBrickIndexWords * kBrickIndicesPerWord >= kBrickVoxels);
}

TEST_CASE("every voxel index round-trips through the ten-per-word packing", "[svo][brick]") {
    // 512 does not divide by 10, so the last word is partial and voxel 510/511 sit at a boundary
    // the arithmetic has to get right. Walk all 512 rather than sampling.
    for (std::size_t i = 0; i < kBrickVoxels; ++i) {
        Brick brick;
        brick.set(i, MaterialID::Stone);
        CHECK(brick.at(i) == MaterialID::Stone);
        CHECK(brick.occupied_count() == 1);
        // No neighbour was disturbed by the read-modify-write of a 3-bit field.
        for (std::size_t j = 0; j < kBrickVoxels; ++j) {
            if (j != i) {
                REQUIRE(brick.at(j) == MaterialID::Air);
            }
        }
    }
}

TEST_CASE("the palette interns: entry 0 is Air and each material takes one slot", "[svo][brick]") {
    Brick brick;
    // Every non-Air material in the registry, twice over, in a scrambled order.
    const std::size_t materials = world::chunk::kMaterialCount;
    for (std::size_t rep = 0; rep < 2; ++rep) {
        for (std::size_t m = 1; m < materials; ++m) {
            brick.set((m * 37 + rep * 11) % kBrickVoxels, static_cast<MaterialID>(m));
        }
    }
    CHECK(brick_palette_entry(brick.words().data(), 0) == MaterialID::Air);
    // Each material appears exactly once among entries 1..7 -- interning, not appending.
    for (std::size_t m = 1; m < materials; ++m) {
        int seen = 0;
        for (std::size_t e = 1; e < kBrickPaletteSize; ++e) {
            seen += brick_palette_entry(brick.words().data(), e) == static_cast<MaterialID>(m) ? 1 : 0;
        }
        CHECK(seen == 1);
    }
}

TEST_CASE("clearing a voxel to Air clears its bit and its index", "[svo][brick]") {
    Brick brick;
    brick.set(std::size_t{100}, MaterialID::Water);
    brick.set(std::size_t{101}, MaterialID::Stone);
    brick.set(std::size_t{100}, MaterialID::Air);
    CHECK(brick.at(std::size_t{100}) == MaterialID::Air);
    CHECK_FALSE(brick.occupied(0, 4, 1)); // voxel 100 = (4, 4, 1)
    CHECK(brick.at(std::size_t{101}) == MaterialID::Stone);
    CHECK(brick.occupied_count() == 1);
}

TEST_CASE("the same fill produces byte-identical words -- determinism survives the palette",
          "[svo][brick]") {
    const auto fill = [](Brick& b) {
        for (std::size_t i = 0; i < kBrickVoxels; ++i) {
            b.set(i, static_cast<MaterialID>((i * 7 + 3) % world::chunk::kMaterialCount));
        }
    };
    Brick a;
    Brick b;
    fill(a);
    fill(b);
    CHECK(a.words() == b.words());
    // And a cleared brick is a valid empty brick with an empty palette -- the invariant that lets
    // interning work without a separate "entries used" counter.
    a.clear();
    for (std::size_t e = 0; e < kBrickPaletteSize; ++e) {
        CHECK(brick_palette_entry(a.words().data(), e) == MaterialID::Air);
    }
    CHECK(a.empty());
}
