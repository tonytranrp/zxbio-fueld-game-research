#include <cmath>
#include <cstdio>

#include <catch2/catch_test_macros.hpp>

#include "engine/jobs/thread_pool.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/tree_builder.hpp"

#include "detail/tree_builder_impl.hpp"

#include "test_samplers.hpp"

using namespace world::svo;
using svo_tests::SphereSampler;
using world::chunk::MaterialID;

namespace {

// 64 m root at 1 m voxels: V = 6, brick levels 0..3 -- small enough to compare every voxel, big
// enough (radius 20 vs 8 m bricks) that whole bricks fall inside the sphere and collapse to solid
// leaves.
TreeGeometry small_geometry() {
    TreeGeometry g;
    g.origin = glm::vec3{0.0f};
    g.root_size_log2 = 6;
    g.voxel_size_log2 = 0;
    return g;
}

SphereSampler small_sphere() {
    return SphereSampler{glm::vec3{32.0f, 32.0f, 32.0f}, 20.0f, MaterialID::Stone};
}

} // namespace

TEST_CASE("uniform-LOD tree reproduces the sampler at every finest voxel", "[svo][builder]") {
    const TreeGeometry g = small_geometry();
    const SphereSampler sphere = small_sphere();
    BuildParams params;
    params.uniform_lod = true;
    BuildStats stats;
    const BrickTree tree = build_tree(sphere, g, params, nullptr, &stats);
    REQUIRE_FALSE(tree.empty());

    std::size_t solid = 0;
    for (int z = 0; z < 64; ++z) {
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                const glm::vec3 center{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f,
                                       static_cast<float>(z) + 0.5f};
                const MaterialID expected = sphere.material_at_center(center);
                REQUIRE(tree.material_at(center) == expected);
                solid += expected != MaterialID::Air ? 1u : 0u;
            }
        }
    }
    CHECK(solid > 30000); // ~4/3 pi 20^3 = 33510 voxels: the sphere really is there

    const BrickTree::Stats ts = tree.stats();
    std::printf("sphere tree: %zu internal, %zu bricks, %zu solid leaves, %zu padding words, %zu bytes; "
                "builder: %zu classified, %zu bricks sampled, %zu kept\n",
                ts.internal_nodes, ts.brick_leaves, ts.solid_leaves, ts.padding_words, tree.memory_bytes(),
                stats.boxes_classified, stats.bricks_sampled, stats.bricks_kept);
    CHECK(ts.brick_leaves > 0);
    CHECK(ts.solid_leaves > 0); // the sphere's interior collapsed to solid leaves, not bricks
    CHECK(ts.brick_leaves == stats.bricks_kept);
    CHECK(ts.deepest_level == g.max_brick_level());
    CHECK(tree.stats().padding_words == stats.padding_words);
}

TEST_CASE("node attributes hold outward average normals and volume coverage", "[svo][builder]") {
    const TreeGeometry g = small_geometry();
    const SphereSampler sphere = small_sphere();
    BuildParams params;
    params.uniform_lod = true;
    const BrickTree tree = build_tree(sphere, g, params, nullptr, nullptr);
    REQUIRE_FALSE(tree.empty());

    // Every brick leaf on the sphere's surface should report a normal within ~35 degrees of the
    // radial direction at its center (a 1 m staircase over an 8 m brick is coarse, so allow the
    // slack), and a coverage strictly between 0 and 1. Solid interior leaves carry no attributes;
    // their parents do, with coverage weighted by how many solid octants they hold.
    std::size_t surfaceBricks = 0;
    for (float z = 4.0f; z < 64.0f; z += 8.0f) {
        for (float y = 4.0f; y < 64.0f; y += 8.0f) {
            for (float x = 4.0f; x < 64.0f; x += 8.0f) {
                const glm::vec3 p{x, y, z};
                if (tree.leaf_level_at(p) != g.max_brick_level() || tree.material_at(p) == MaterialID::Air) {
                    continue;
                }
                const std::uint32_t attr = tree.attributes_at(p, g.max_brick_level());
                if (attr == 0u) {
                    continue; // a solid leaf
                }
                const glm::vec3 n = node_attr_normal(attr);
                const glm::vec3 radial = glm::normalize(p - sphere.center);
                REQUIRE(glm::length(n) > 0.9f);
                CHECK(glm::dot(n, radial) > 0.8f);
                const float c = node_attr_coverage(attr);
                CHECK(c > 0.0f);
                CHECK(c < 1.0f);
                ++surfaceBricks;
            }
        }
    }
    CHECK(surfaceBricks > 20);

    // The root's coverage is the sphere's volume fraction of the 64 m cube (33510 / 262144).
    const std::uint32_t rootAttr = tree.nodes[tree.root + kNodeAttrSlotInternal];
    CHECK(std::abs(node_attr_coverage(rootAttr) - 33510.0f / 262144.0f) < 0.01f);
    // A closed sphere's summed outward normal is ~zero: nothing exposed in any preferred direction.
    CHECK(glm::length(node_attr_normal(rootAttr)) < 0.5f);
    // A node holding the sphere's top cap points up.
    const std::uint32_t capAttr = tree.attributes_at(glm::vec3{32.0f, 50.0f, 32.0f}, 2);
    REQUIRE(capAttr != 0u);
    CHECK(node_attr_normal(capAttr).y > 0.9f);
}

TEST_CASE("parallel build produces byte-identical nodes and bricks", "[svo][builder]") {
    const TreeGeometry g = small_geometry();
    const SphereSampler sphere = small_sphere();
    BuildParams params;
    params.uniform_lod = true;
    const BrickTree serial = build_tree(sphere, g, params, nullptr, nullptr);

    engine::jobs::ThreadPool pool(4);
    for (int split = 1; split <= 3; ++split) {
        params.parallel_split_level = split;
        const BrickTree parallel = build_tree(sphere, g, params, &pool, nullptr);
        REQUIRE(parallel.root == serial.root);
        REQUIRE(parallel.nodes == serial.nodes);
        REQUIRE(parallel.bricks == serial.bricks);
    }
}

TEST_CASE("an all-air region builds an empty tree", "[svo][builder]") {
    const TreeGeometry g = small_geometry();
    const SphereSampler nothing{glm::vec3{-100.0f}, 1.0f, MaterialID::Stone};
    BuildParams params;
    params.uniform_lod = true;
    const BrickTree tree = build_tree(nothing, g, params, nullptr, nullptr);
    CHECK(tree.empty());
    CHECK(tree.material_at(glm::vec3{32.0f}) == MaterialID::Air);
}

TEST_CASE("distance LOD keeps full resolution near the center and coarsens with distance", "[svo][builder]") {
    // 128 m root at 0.25 m voxels: V = 9, brick levels 0..6.
    TreeGeometry g;
    g.origin = glm::vec3{0.0f};
    g.root_size_log2 = 7;
    g.voxel_size_log2 = -2;
    const SphereSampler sphere{glm::vec3{64.0f}, 60.0f, MaterialID::Dirt};

    BuildParams uniform;
    uniform.uniform_lod = true;
    const BrickTree full = build_tree(sphere, g, uniform, nullptr, nullptr);

    BuildParams lod;
    lod.lod_center = glm::vec3{64.0f, 4.0f, 64.0f}; // on the sphere's bottom surface
    lod.lod_radius = 4.0f;
    BuildStats stats;
    const BrickTree coarse = build_tree(sphere, g, lod, nullptr, &stats);
    REQUIRE_FALSE(coarse.empty());
    std::printf("LOD tree: %zu bricks / %zu bytes vs uniform %zu bricks / %zu bytes (%.3fs)\n",
                coarse.brick_count(), coarse.memory_bytes(), full.brick_count(), full.memory_bytes(),
                stats.seconds);
    CHECK(coarse.brick_count() < full.brick_count() / 4);

    // Within lod_radius the tree is at full depth and agrees with the uniform tree voxel by voxel.
    for (float dx = -3.0f; dx <= 3.0f; dx += 0.25f) {
        for (float dz = -3.0f; dz <= 3.0f; dz += 0.25f) {
            const glm::vec3 p = lod.lod_center + glm::vec3{dx + 0.125f, 0.125f, dz + 0.125f};
            CHECK(coarse.leaf_level_at(p) == full.leaf_level_at(p));
            CHECK(coarse.material_at(p) == full.material_at(p));
        }
    }
    // Far away (the sphere's top surface, ~120 m from the center) the leaf level is shallower.
    const glm::vec3 far{64.0f, 123.9f, 64.0f};
    CHECK(coarse.leaf_level_at(far) < full.leaf_level_at(far));
    CHECK(full.leaf_level_at(far) == g.max_brick_level());
    // Every leaf of the LOD tree is no coarser than the distance rule asks for.
    for (float x = 2.0f; x < 128.0f; x += 9.7f) {
        for (float z = 2.0f; z < 128.0f; z += 9.7f) {
            const glm::vec3 p{x, 5.0f, z};
            const int level = coarse.leaf_level_at(p);
            if (level < 0 || coarse.material_at(p) == MaterialID::Air) {
                continue;
            }
            const float d = glm::length(p - lod.lod_center);
            const float target = glm::max(g.finest_voxel_edge(), d * g.finest_voxel_edge() / lod.lod_radius);
            // A leaf at `level` holds voxels of level_voxel_edge(level); an internal node's missing
            // child (Air) is skipped above. Solid leaves may be arbitrarily coarse by construction.
            if (coarse.leaf_level_at(p) < g.max_brick_level()) {
                CHECK(g.level_voxel_edge(level) <= target * 2.0f + 1.0e-4f);
            }
        }
    }
}
