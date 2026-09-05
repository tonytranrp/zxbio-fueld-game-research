#include <catch2/catch_test_macros.hpp>

#include "world/svo/tree_layout.hpp"

using namespace world::svo;
using world::chunk::MaterialID;

TEST_CASE("node header packs kind, child mask and material losslessly", "[svo][layout]") {
    const std::uint32_t h = make_node_header(kNodeKindInternal, 0b10110001u, MaterialID::Grass);
    CHECK(node_kind(h) == kNodeKindInternal);
    CHECK(node_child_mask(h) == 0b10110001u);
    CHECK(node_material(h) == MaterialID::Grass);

    const std::uint32_t solid = make_node_header(kNodeKindSolid, 0u, MaterialID::Water);
    CHECK(node_kind(solid) == kNodeKindSolid);
    CHECK(node_child_mask(solid) == 0u);
    CHECK(node_material(solid) == MaterialID::Water);

    const std::uint32_t brick = make_node_header(kNodeKindBrick, 0u, MaterialID::Leaves);
    CHECK(node_kind(brick) == kNodeKindBrick);
}

TEST_CASE("child slots are packed in octant order behind the header", "[svo][layout]") {
    // Octants 0, 4, 5, 7 present -> pointers at header+1..header+4 in that order (the SVDAG
    // paper's layout: 4 + 4*popcount bytes per internal node).
    const std::uint32_t h = make_node_header(kNodeKindInternal, 0b10110001u, MaterialID::Stone);
    CHECK(node_child_slot(h, 0) == 1);
    CHECK(node_child_slot(h, 4) == 2);
    CHECK(node_child_slot(h, 5) == 3);
    CHECK(node_child_slot(h, 7) == 4);
}

TEST_CASE("octant bit layout is x, y, z", "[svo][layout]") {
    CHECK(octant_of(0, 0, 0) == 0);
    CHECK(octant_of(1, 0, 0) == 1);
    CHECK(octant_of(0, 1, 0) == 2);
    CHECK(octant_of(0, 0, 1) == 4);
    CHECK(octant_of(3, 5, 7) == 7); // only the low bit of each coordinate matters
    CHECK(octant_of(2, 4, 6) == 0);
}

TEST_CASE("tree geometry derives its levels from the two power-of-two exponents", "[svo][layout]") {
    TreeGeometry g;
    g.origin = glm::vec3{-256.0f, -256.0f, -256.0f};
    g.root_size_log2 = 9;   // 512 m
    g.voxel_size_log2 = -7; // 1/128 m
    CHECK(g.voxel_bits() == 16);
    CHECK(g.max_brick_level() == 13);
    CHECK(g.root_edge() == 512.0f);
    CHECK(g.finest_voxel_edge() == 1.0f / 128.0f);
    CHECK(g.level_edge(0) == 512.0f);
    CHECK(g.level_edge(13) == 0.0625f); // finest brick: 8 voxels of 1/128 m
    CHECK(g.level_voxel_edge(13) == 1.0f / 128.0f);
    CHECK(g.level_voxel_edge(0) == 64.0f);
    CHECK(g.contains(glm::vec3{0.0f}));
    CHECK_FALSE(g.contains(glm::vec3{256.0f, 0.0f, 0.0f})); // half-open max edge
    CHECK(g.contains(glm::vec3{-256.0f, -256.0f, -256.0f}));
}
