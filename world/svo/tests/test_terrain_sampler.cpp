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
// chunk world never voxelizes trees).
//
// GOAL 161 IS FIXED, so this test no longer skips anything. It used to exclude columns with a
// NEGATIVE surface height, because `fill_terrain` truncated toward zero (`static_cast<int32_t>`)
// instead of flooring -- a column at -3.4 m became -3, one voxel HIGH, everywhere below sea level.
// Above sea level truncation and flooring agree, which is exactly why skipping the negative columns
// hid it. `fill_terrain` floors now and EVERY column is compared, which is what makes this a tie
// between the two representations rather than a tie over the half of the world where they happened
// to agree.
TEST_CASE("terrain sampler reproduces fill_terrain exactly at 1 m voxels", "[svo][terrain]") {
    const world::generation::HeightmapGenerator heightmap(kSeed);
    TerrainSamplerParams params;
    // GOAL 321'S DECISION, AND ITS REASON. This file compares the sparse-brick sampler against
    // `fill_terrain` byte for byte, and goal 313 gave the sampler CAVES, which `fill_terrain` does
    // not have. The prompt's question was: does the mesh path read the new field too, or is the
    // test rewritten?
    //
    // Neither, deliberately. The test's purpose is to prove the two representations share the SAME
    // BANDING RULES -- that is what "byte-identical at 1 m" was ever evidence for -- and caves are a
    // feature the mesh path is the documented fallback for and is not getting. So these tests run
    // with caves DISABLED, which is exactly the world they were written about. The property that
    // makes that sound is asserted separately in `test_caves.cpp`: at threshold zero the sampler is
    // bit-identical to the cave-free world, and with caves on it genuinely differs.
    //
    // Not weakened, not deleted. What it proves is unchanged.
    params.caves.threshold = 0.0f;
    params.seed = kSeed;
    params.trees = false;
    const Box region{glm::vec3{-96.0f, -128.0f, -96.0f}, glm::vec3{96.0f, 128.0f, 96.0f}};
    const TerrainSampler sampler(heightmap, params, region);

    const ChunkCoord coords[] = {{0, 0, 0}, {1, -1, 0}, {-1, 0, 1}, {2, 0, -2}, {-2, 1, -1}, {0, -2, 2}};
    std::size_t compared = 0;
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
    std::printf("fill_terrain equivalence: %zu voxels compared (%zu non-air), 0 skipped -- goal 161\n",
                compared, nonAir);
    CHECK(compared > 100000);
    CHECK(nonAir > 1000);
}

TEST_CASE("fill_brick agrees with the pointwise material rule at sub-meter voxels", "[svo][terrain]") {
    const world::generation::HeightmapGenerator heightmap(kSeed);
    TerrainSamplerParams params;
    params.caves.threshold = 0.0f; // see the note on the first of these: goal 321
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
    params.caves.threshold = 0.0f; // see the note on the first of these: goal 321
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
    params.caves.threshold = 0.0f; // see the note on the first of these: goal 321
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
    params.caves.threshold = 0.0f; // see the note on the first of these: goal 321
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

// --- goal 251: the focus tiers are shareable, and sharing them changes nothing --------------------

TEST_CASE("adopted focus tiers give the same answers as sampled ones", "[svo][terrain][focus]") {
    // The whole point of `adopt_focus` is that a cell grid can build the tiers ONCE and hand them to
    // every cell in a rebuild instead of paying ~1.3 M noise samples per cell. That is only sound if
    // an adopted tier is indistinguishable from a sampled one, which is what this pins.
    const world::generation::HeightmapGenerator heightmap(kSeed);
    TerrainSamplerParams params;
    params.caves.threshold = 0.0f; // see the note on the first of these: goal 321
    params.seed = kSeed;
    params.trees = true;
    const Box region{glm::vec3{-32.0f, -32.0f, -32.0f}, glm::vec3{32.0f, 32.0f, 32.0f}};
    const glm::vec3 center{0.0f, 8.0f, 0.0f};
    constexpr float kRadius = 16.0f;

    TerrainSampler sampled(heightmap, params, region);
    sampled.set_focus(center, kRadius);

    TerrainSampler adopted(heightmap, params, region);
    TerrainSampler::FocusTiers tiers;
    for (const TerrainSampler::FocusKey& key : TerrainSampler::focus_keys(params, center, kRadius)) {
        tiers.push_back(TerrainSampler::make_focus_tier(heightmap, key));
    }
    adopted.adopt_focus(std::move(tiers));

    std::size_t compared = 0;
    for (float z = -16.0f; z < 16.0f; z += 1.0f) {
        for (float x = -16.0f; x < 16.0f; x += 1.0f) {
            for (float y = -8.0f; y < 24.0f; y += 4.0f) {
                const glm::vec3 p{x, y, z};
                REQUIRE(sampled.material_at(p, 1.0f) == adopted.material_at(p, 1.0f));
                ++compared;
            }
        }
    }
    REQUIRE(compared > 5000);
}

TEST_CASE("focus keys snap identically for nearby centres", "[svo][terrain][focus]") {
    // The keys are what any sharing scheme compares. Two centres inside the same coarse cell must
    // produce the same rectangle -- and two a trigger-distance apart must not, which is the
    // measurement that killed the cross-build cache (0% hit rate at an 8 m trigger).
    TerrainSamplerParams params;
    params.caves.threshold = 0.0f; // see the note on the first of these: goal 321
    params.seed = kSeed;
    const auto a = TerrainSampler::focus_keys(params, glm::vec3{0.0f, 8.0f, 0.0f}, 16.0f);
    const auto b = TerrainSampler::focus_keys(params, glm::vec3{0.01f, 8.0f, 0.01f}, 16.0f);
    const auto far = TerrainSampler::focus_keys(params, glm::vec3{8.0f, 8.0f, 0.0f}, 16.0f);
    CHECK(a[0] == b[0]);
    CHECK(a[1] == b[1]);
    CHECK_FALSE(a[0] == far[0]);
}
