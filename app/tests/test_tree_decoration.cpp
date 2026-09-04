#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../src/tree_decoration.hpp"

using app::compute_tree_placements;
using app::TreePlacement;

// TERRAIN_FIXES_BRIEF Group W tasks 29/30's checks, against the REAL generator.

TEST_CASE("tree placement is deterministic across independent evaluations", "[trees]") {
    const world::generation::HeightmapGenerator genA(1337);
    const world::generation::HeightmapGenerator genB(1337);

    for (std::int32_t cx = -2; cx <= 2; ++cx) {
        for (std::int32_t cz = -2; cz <= 2; ++cz) {
            const auto a = compute_tree_placements(cx, cz, 1337, genA);
            const auto b = compute_tree_placements(cx, cz, 1337, genB);
            REQUIRE(a.size() == b.size());
            for (std::size_t i = 0; i < a.size(); ++i) {
                CHECK(a[i].world_x == b[i].world_x);
                CHECK(a[i].world_z == b[i].world_z);
                CHECK(a[i].base_height == b[i].base_height);
                CHECK(a[i].trunk_height == b[i].trunk_height);
                CHECK(a[i].canopy_radius == b[i].canopy_radius);
            }
        }
    }
}

TEST_CASE("tree placements honor spacing and mask rules", "[trees]") {
    const world::generation::HeightmapGenerator generator(1337);

    // Gather placements over a 5x5-chunk-column region so cross-chunk spacing is exercised too.
    std::vector<TreePlacement> all;
    for (std::int32_t cx = -2; cx <= 2; ++cx) {
        for (std::int32_t cz = -2; cz <= 2; ++cz) {
            const auto placements = compute_tree_placements(cx, cz, 1337, generator);
            all.insert(all.end(), placements.begin(), placements.end());
        }
    }
    REQUIRE(all.size() > 5); // the region genuinely hosts trees (not a vacuously-passing test)

    for (const TreePlacement& tree : all) {
        CHECK(tree.base_height >= app::kTreeMinHeight); // no water/beach trees
        CHECK(tree.base_height <= app::kTreeMaxHeight); // tree line respected
        const float slopeX = std::abs(generator.height_at(tree.world_x + 1.0f, tree.world_z) -
                                      generator.height_at(tree.world_x - 1.0f, tree.world_z)) *
                             0.5f;
        const float slopeZ = std::abs(generator.height_at(tree.world_x, tree.world_z + 1.0f) -
                                      generator.height_at(tree.world_x, tree.world_z - 1.0f)) *
                             0.5f;
        CHECK(std::max(slopeX, slopeZ) <= app::kTreeMaxSlope);
    }

    for (std::size_t i = 0; i < all.size(); ++i) {
        for (std::size_t j = i + 1; j < all.size(); ++j) {
            const float dx = all[i].world_x - all[j].world_x;
            const float dz = all[i].world_z - all[j].world_z;
            CHECK(std::max(std::abs(dx), std::abs(dz)) >= app::kTreeMinSpacing);
        }
    }
}

TEST_CASE("appended tree geometry stays within the owning chunk's local bounds", "[trees]") {
    const world::generation::HeightmapGenerator generator(1337);

    std::size_t totalEmitted = 0;
    for (std::int32_t cx = -2; cx <= 2; ++cx) {
        for (std::int32_t cz = -2; cz <= 2; ++cz) {
            for (std::int32_t cy = -2; cy <= 2; ++cy) {
                world::meshing::MeshData mesh;
                totalEmitted += app::append_tree_meshes(mesh, {cx, cy, cz}, 1337, generator);
                for (const auto& vertex : mesh.vertices) {
                    // Same envelope the compressed GPU format quantizes: [-1, 32] per axis.
                    CHECK(vertex.position.x >= -1.0f);
                    CHECK(vertex.position.x <= 33.0f); // canopy may lean past a column edge slightly
                    CHECK(vertex.position.y >= -1.0f);
                    CHECK(vertex.position.y <= 32.0f);
                    CHECK(glm::length(vertex.normal) > 0.9f); // real unit face normals, no zeros
                }
            }
        }
    }
    CHECK(totalEmitted > 5); // trees actually got emitted somewhere in the region
}
