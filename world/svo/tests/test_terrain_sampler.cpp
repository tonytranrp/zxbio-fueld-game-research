#include <cstdio>
#include <random>

#include <catch2/catch_test_macros.hpp>

#include "engine/jobs/thread_pool.hpp"
#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_coord.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/generation/tree_placement.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/terrain_sampler.hpp"
#include "world/svo/tree_builder.hpp"

using namespace world::svo;
using world::chunk::Chunk;
using world::chunk::ChunkCoord;
using world::chunk::kChunkSize;
using world::chunk::local_index;
using world::chunk::MaterialID;

namespace {
constexpr int kSeed = 1337;
}

// The tie between the new representation and the shipped world: at voxel size 1 m the sampler
// must answer exactly what fill_terrain wrote into the chunk, voxel for voxel (trees off -- the
// chunk world never voxelizes trees). Columns with a NEGATIVE surface height are skipped: there
// fill_terrain's `static_cast<int32_t>(surfaceHeight)` truncates toward zero instead of flooring,
// so its underwater terrain sits one voxel higher than the geometric rule -- a real fill_terrain
// quirk (docs/goals.md), not something the meter-based sampler should reproduce.
TEST_CASE("terrain sampler reproduces fill_terrain exactly at 1 m voxels", "[svo][terrain]") {
    const world::generation::HeightmapGenerator heightmap(kSeed);
    TerrainSamplerParams params;
    params.seed = kSeed;
    params.trees = false;
    const Box region{glm::vec3{-96.0f, -128.0f, -96.0f}, glm::vec3{96.0f, 128.0f, 96.0f}};
    const TerrainSampler sampler(heightmap, params, region);

    const ChunkCoord coords[] = {{0, 0, 0}, {1, -1, 0}, {-1, 0, 1}, {2, 0, -2}, {-2, 1, -1}, {0, -2, 2}};
    std::size_t compared = 0;
    std::size_t skipped = 0;
    std::size_t nonAir = 0;
    for (const ChunkCoord& coord : coords) {
        Chunk chunk(coord);
        world::generation::fill_terrain(chunk, heightmap);
        for (std::int32_t lz = 0; lz < kChunkSize; ++lz) {
            for (std::int32_t lx = 0; lx < kChunkSize; ++lx) {
                const float wx = static_cast<float>(coord.x * kChunkSize + lx);
                const float wz = static_cast<float>(coord.z * kChunkSize + lz);
                float surface = 0.0f;
                heightmap.generate_column_heights_spaced(wx, wz, 1, 1, 1.0f, &surface);
                if (surface < 0.0f) {
                    skipped += static_cast<std::size_t>(kChunkSize);
                    continue;
                }
                for (std::int32_t ly = 0; ly < kChunkSize; ++ly) {
                    const float wy = static_cast<float>(coord.y * kChunkSize + ly);
                    const MaterialID expected = chunk.voxels().at(local_index(lx, ly, lz));
                    const MaterialID got = sampler.material_at(glm::vec3{wx, wy, wz}, 1.0f);
                    if (expected != got) {
                        std::printf("mismatch at (%g,%g,%g): chunk %d, sampler %d (surface %.3f)\n",
                                    static_cast<double>(wx), static_cast<double>(wy), static_cast<double>(wz),
                                    static_cast<int>(expected), static_cast<int>(got),
                                    static_cast<double>(surface));
                    }
                    REQUIRE(expected == got);
                    ++compared;
                    nonAir += expected != MaterialID::Air ? 1u : 0u;
                }
            }
        }
    }
    std::printf("fill_terrain equivalence: %zu voxels compared (%zu non-air), %zu skipped below sea level\n",
                compared, nonAir, skipped);
    CHECK(compared > 100000);
    CHECK(nonAir > 1000);
}

TEST_CASE("fill_brick agrees with the pointwise material rule at sub-meter voxels", "[svo][terrain]") {
    const world::generation::HeightmapGenerator heightmap(kSeed);
    TerrainSamplerParams params;
    params.seed = kSeed;
    params.trees = true;
    const Box region{glm::vec3{-64.0f, -128.0f, -64.0f}, glm::vec3{64.0f, 128.0f, 64.0f}};
    const TerrainSampler sampler(heightmap, params, region);

    std::mt19937 rng(5);
    std::uniform_real_distribution<float> xz(-60.0f, 56.0f);
    std::size_t bricksWithContent = 0;
    for (int trial = 0; trial < 60; ++trial) {
        const float voxelEdge = trial % 3 == 0 ? 0.125f : (trial % 3 == 1 ? 0.5f : 0.03125f);
        const float x = xz(rng);
        const float z = xz(rng);
        const float surface = heightmap.height_at(x, z);
        const glm::vec3 origin{x, surface - 3.0f * voxelEdge, z}; // straddles the surface
        Brick brick;
        sampler.fill_brick(origin, voxelEdge, brick);
        bricksWithContent += brick.empty() ? 0u : 1u;
        for (int k = 0; k < 8; ++k) {
            for (int j = 0; j < 8; ++j) {
                for (int i = 0; i < 8; ++i) {
                    const glm::vec3 voxelMin =
                        origin +
                        glm::vec3{static_cast<float>(i), static_cast<float>(j), static_cast<float>(k)} *
                            voxelEdge;
                    REQUIRE(brick.at(i, j, k) == sampler.material_at(voxelMin, voxelEdge));
                }
            }
        }
    }
    CHECK(bricksWithContent > 50);
}

// The builder trusts classify() blindly, so a wrong Air/Solid is a hole in the world: check
// random boxes against dense pointwise sampling, with and without trees.
TEST_CASE("box classification is sound against dense sampling", "[svo][terrain]") {
    const world::generation::HeightmapGenerator heightmap(kSeed);
    const Box region{glm::vec3{-64.0f, -128.0f, -64.0f}, glm::vec3{64.0f, 128.0f, 64.0f}};
    for (const bool trees : {false, true}) {
        TerrainSamplerParams params;
        params.seed = kSeed;
        params.trees = trees;
        const TerrainSampler sampler(heightmap, params, region);

        std::mt19937 rng(trees ? 11u : 12u);
        std::uniform_real_distribution<float> xz(-64.0f, 60.0f);
        std::uniform_real_distribution<float> y(-80.0f, 80.0f);
        std::uniform_real_distribution<float> sizeLog(-2.0f, 4.0f);
        std::size_t air = 0;
        std::size_t solid = 0;
        std::size_t mixed = 0;
        for (int trial = 0; trial < 3000; ++trial) {
            const float size = std::exp2(sizeLog(rng));
            const Box box{glm::vec3{xz(rng), y(rng), xz(rng)}, glm::vec3{0.0f}};
            const Box b{box.min, box.min + glm::vec3{size}};
            const BoxClassification cls = sampler.classify(b);
            if (cls.cls == BoxClass::Mixed) {
                ++mixed;
                continue;
            }
            (cls.cls == BoxClass::Air ? air : solid) += 1;
            // 5x5x5 voxels of edge size/5 tile the box exactly; each must match the claim.
            const float voxelEdge = size / 5.0f;
            for (int k = 0; k < 5; ++k) {
                for (int j = 0; j < 5; ++j) {
                    for (int i = 0; i < 5; ++i) {
                        const glm::vec3 voxelMin =
                            b.min +
                            glm::vec3{static_cast<float>(i), static_cast<float>(j), static_cast<float>(k)} *
                                voxelEdge;
                        const MaterialID m = sampler.material_at(voxelMin, voxelEdge);
                        const MaterialID expected = cls.cls == BoxClass::Air ? MaterialID::Air : cls.material;
                        if (m != expected) {
                            std::printf(
                                "unsound %s box at (%.2f,%.2f,%.2f) size %.3f: voxel (%d,%d,%d) is %d\n",
                                cls.cls == BoxClass::Air ? "Air" : "Solid", static_cast<double>(b.min.x),
                                static_cast<double>(b.min.y), static_cast<double>(b.min.z),
                                static_cast<double>(size), i, j, k, static_cast<int>(m));
                        }
                        REQUIRE(m == expected);
                    }
                }
            }
        }
        std::printf("classification (trees=%d): %zu air, %zu solid, %zu mixed\n", trees ? 1 : 0, air, solid,
                    mixed);
        CHECK(air > 100);
        CHECK(solid > 100);
    }
}

TEST_CASE("trees are voxelized at their placements", "[svo][terrain]") {
    const world::generation::HeightmapGenerator heightmap(kSeed);
    TerrainSamplerParams params;
    params.seed = kSeed;
    params.trees = true;
    const Box region{glm::vec3{-64.0f, -128.0f, -64.0f}, glm::vec3{64.0f, 128.0f, 64.0f}};
    const TerrainSampler sampler(heightmap, params, region);
    REQUIRE(sampler.trees().size() > 5);

    std::size_t woodColumns = 0;
    std::size_t leafHits = 0;
    for (const world::generation::TreePlacement& tree : sampler.trees()) {
        if (tree.shape != world::generation::TreeShape::Shrub) {
            // One meter up the trunk, at the trunk's own column: Wood at 1/8 m voxels.
            const glm::vec3 p{tree.world_x - 0.0625f, tree.base_height + 1.0f, tree.world_z - 0.0625f};
            if (sampler.material_at(p, 0.125f) == MaterialID::Wood) {
                ++woodColumns;
            }
        }
        std::array<world::generation::CanopyLobe, world::generation::kMaxCanopyLobes> lobes{};
        const std::size_t n = world::generation::tree_canopy_lobes(tree, lobes);
        for (std::size_t i = 0; i < n; ++i) {
            const glm::vec3 c = lobes[i].center + glm::vec3{0.0f, 0.3f * lobes[i].rv, 0.0f};
            if (sampler.material_at(c - glm::vec3{0.0625f}, 0.125f) == MaterialID::Leaves) {
                ++leafHits;
            }
        }
    }
    std::printf("trees: %zu placements, %zu wood columns, %zu leaf lobes hit\n", sampler.trees().size(),
                woodColumns, leafHits);
    CHECK(woodColumns > 0);
    CHECK(leafHits > 0);
}

TEST_CASE("a terrain tree builds at sub-centimeter resolution near the camera", "[svo][terrain]") {
    const world::generation::HeightmapGenerator heightmap(kSeed);
    TerrainSamplerParams params;
    params.seed = kSeed;
    params.trees = true;
    TreeGeometry g;
    g.origin = glm::vec3{-32.0f, -64.0f, -32.0f};
    g.root_size_log2 = 6;   // 64 m
    g.voxel_size_log2 = -7; // 7.8 mm
    const Box region{g.origin, g.max_corner()};
    TerrainSampler sampler(heightmap, params, region);

    BuildParams build;
    build.lod_center = glm::vec3{0.0f, heightmap.height_at(0.0f, 0.0f) + 1.7f, 0.0f};
    build.lod_radius = 2.0f;
    sampler.set_focus(build.lod_center, 4.0f * build.lod_radius);
    BuildStats stats;
    engine::jobs::ThreadPool pool(4);
    const BrickTree tree = build_tree(sampler, g, build, &pool, &stats);
    REQUIRE_FALSE(tree.empty());
    const BrickTree::Stats ts = tree.stats();
    std::printf(
        "terrain tree (64 m, 7.8 mm near camera): %zu bricks, %zu internal, %zu solid, %.1f MB, %.3fs, "
        "%zu classified, %zu bricks sampled, deepest level %d\n",
        ts.brick_leaves, ts.internal_nodes, ts.solid_leaves, static_cast<double>(tree.memory_bytes()) / 1.0e6,
        stats.seconds, stats.boxes_classified, stats.bricks_sampled, ts.deepest_level);
    CHECK(ts.deepest_level == g.max_brick_level());
    // Directly under the camera the surface voxel is at full resolution and is a real material.
    const glm::vec3 underfoot{0.0f, heightmap.height_at(0.0f, 0.0f) - 0.02f, 0.0f};
    CHECK(tree.leaf_level_at(underfoot) == g.max_brick_level());
    CHECK(tree.material_at(underfoot) != MaterialID::Air);
}
