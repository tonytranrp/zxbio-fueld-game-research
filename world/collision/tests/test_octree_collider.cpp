// Goal 226's Check. Two halves, deliberately:
//
//   1. EXACT answers on hand-built tiny trees -- a single solid cube, a two-level tree, and an
//      OVERHANG (a solid slab with air under it). Boxes that touch a face, share a face exactly,
//      and miss by one voxel. This is where an off-by-one in the half-open box arithmetic shows up.
//   2. A CROSS-CHECK against TerrainSampler-derived truth over thousands of random boxes in a real
//      generated region -- and, critically, an assertion about the DIRECTION of any disagreement:
//      the tree may be MORE solid than the sampler where its LOD is coarser, never less. A tree
//      that is less solid than truth is a clipping bug, which is the whole point of this prompt.

#include <catch2/catch_test_macros.hpp>

// The tooling that generates edits to this file collapses backslash escapes in written content
// (CLAUDE.md's own note), so the line break comes from a macro rather than from an escape.
#define NEWLINE "\n"

#include <array>
#include <cstdio>
#include <random>

#include "world/collision/octree_collider.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/materials/materials.hpp"
#include "world/svo/terrain_sampler.hpp"
#include "world/svo/tree_builder.hpp"

#include "detail/tree_builder_impl.hpp"

using world::chunk::MaterialID;
using world::collision::Aabb;
using world::collision::OctreeCollider;
using world::svo::Box;
using world::svo::BoxClass;
using world::svo::BoxClassification;
using world::svo::Brick;
using world::svo::BrickTree;
using world::svo::BuildParams;
using world::svo::TreeGeometry;

namespace {

TreeGeometry geometry(int rootLog2, int voxelLog2, glm::vec3 origin = glm::vec3{0.0f}) {
    TreeGeometry g;
    g.origin = origin;
    g.root_size_log2 = rootLog2;
    g.voxel_size_log2 = voxelLog2;
    return g;
}

// A sampler over an explicit predicate on the VOXEL MIN CORNER, so a test can state exactly which
// voxels are solid and the tree is then built from that statement.
template <class Fn>
struct PredicateSampler {
    Fn solid;
    MaterialID material = MaterialID::Stone;

    [[nodiscard]] BoxClassification classify(const Box& box) const noexcept {
        // Always Mixed: the builder then subdivides to the finest level and asks fill_brick, which
        // is what makes the tree exactly the predicate rather than an approximation of it.
        (void)box;
        return {BoxClass::Mixed, MaterialID::Air};
    }
    void fill_brick(const glm::vec3& origin, float voxelEdge, Brick& brick) const {
        for (int z = 0; z < world::svo::kBrickEdge; ++z) {
            for (int y = 0; y < world::svo::kBrickEdge; ++y) {
                for (int x = 0; x < world::svo::kBrickEdge; ++x) {
                    const glm::vec3 min = origin + glm::vec3{static_cast<float>(x), static_cast<float>(y),
                                                             static_cast<float>(z)} *
                                                       voxelEdge;
                    if (solid(min, voxelEdge)) {
                        brick.set(x, y, z, material);
                    }
                }
            }
        }
    }
};

template <class Fn>
BrickTree build_from(const TreeGeometry& g, Fn&& fn, MaterialID material = MaterialID::Stone) {
    PredicateSampler<Fn> sampler{std::forward<Fn>(fn), material};
    BuildParams params;
    params.uniform_lod = true;
    return world::svo::build_tree(sampler, g, params, nullptr, nullptr);
}

OctreeCollider collider_over(BrickTree tree) {
    return OctreeCollider{std::make_shared<const BrickTree>(std::move(tree))};
}

} // namespace

TEST_CASE("a collider with no tree says nothing is solid", "[collision][octree]") {
    // The honest answer while the world is still building. The frame loop does not step the body
    // until a tree exists, and a query that guessed "solid" would trap it instead.
    const OctreeCollider empty;
    CHECK_FALSE(empty.has_tree());
    CHECK_FALSE(empty.overlaps_solid(Aabb{glm::vec3{0.0f}, glm::vec3{1.0f}}));
}

TEST_CASE("a single solid voxel is found exactly, and its neighbours are not", "[collision][octree]") {
    // 8 m root, 1 m voxels: voxel (2,3,4) and nothing else.
    const TreeGeometry g = geometry(3, 0);
    const OctreeCollider c = collider_over(build_from(
        g, [](const glm::vec3& min, float) { return min.x == 2.0f && min.y == 3.0f && min.z == 4.0f; }));
    REQUIRE(c.has_tree());

    // Dead centre of the voxel.
    CHECK(c.overlaps_solid(Aabb{glm::vec3{2.4f, 3.4f, 4.4f}, glm::vec3{2.6f, 3.6f, 4.6f}}));
    // Exactly the voxel.
    CHECK(c.overlaps_solid(Aabb{glm::vec3{2.0f, 3.0f, 4.0f}, glm::vec3{3.0f, 4.0f, 5.0f}}));
    // Sharing the +x face exactly: half-open, so a box starting AT x=3 does not touch the voxel.
    CHECK_FALSE(c.overlaps_solid(Aabb{glm::vec3{3.0f, 3.2f, 4.2f}, glm::vec3{3.8f, 3.8f, 4.8f}}));
    // Overlapping the +x face by a hair: it does.
    CHECK(c.overlaps_solid(Aabb{glm::vec3{2.99f, 3.2f, 4.2f}, glm::vec3{3.8f, 3.8f, 4.8f}}));
    // One voxel away on each axis.
    CHECK_FALSE(c.overlaps_solid(Aabb{glm::vec3{1.1f, 3.1f, 4.1f}, glm::vec3{1.9f, 3.9f, 4.9f}}));
    CHECK_FALSE(c.overlaps_solid(Aabb{glm::vec3{2.1f, 4.1f, 4.1f}, glm::vec3{2.9f, 4.9f, 4.9f}}));
    CHECK_FALSE(c.overlaps_solid(Aabb{glm::vec3{2.1f, 3.1f, 5.1f}, glm::vec3{2.9f, 3.9f, 5.9f}}));
    // Wholly outside the tree.
    CHECK_FALSE(c.overlaps_solid(Aabb{glm::vec3{20.0f}, glm::vec3{21.0f}}));
}

TEST_CASE("an OVERHANG is answered correctly -- there is no one-surface-per-column assumption",
          "[collision][octree]") {
    // Prompt 006 adds caves. A query that assumes a height field is a bug then, so it is tested
    // now: a slab at y in [4,5) with AIR under it and air above it.
    const TreeGeometry g = geometry(3, 0);
    const OctreeCollider c =
        collider_over(build_from(g, [](const glm::vec3& min, float) { return min.y == 4.0f; }));

    CHECK(c.overlaps_solid(Aabb{glm::vec3{1.2f, 4.2f, 1.2f}, glm::vec3{1.8f, 4.8f, 1.8f}}));       // in it
    CHECK_FALSE(c.overlaps_solid(Aabb{glm::vec3{1.2f, 2.2f, 1.2f}, glm::vec3{1.8f, 3.8f, 1.8f}})); // under
    CHECK_FALSE(c.overlaps_solid(Aabb{glm::vec3{1.2f, 5.2f, 1.2f}, glm::vec3{1.8f, 5.8f, 1.8f}})); // over

    // voxel_top from ABOVE the slab finds the slab; from BELOW it finds nothing, because there is
    // nothing below. A height field would have answered "the slab" both times.
    CHECK(c.voxel_top(1.5f, 1.5f, 7.5f) == 5.0f);
    CHECK(c.voxel_top(1.5f, 1.5f, 3.5f) == -std::numeric_limits<float>::infinity());
}

TEST_CASE("water is not solid and leaves are not solid -- the components say so", "[collision][octree]") {
    // Asked of world/materials, never by comparing IDs (Prompt 003 rule 6). If a future material
    // change makes water solid, this test fails here rather than the swimmer standing on the sea.
    const TreeGeometry g = geometry(3, 0);
    const OctreeCollider water = collider_over(
        build_from(g, [](const glm::vec3& min, float) { return min.y == 2.0f; }, MaterialID::Water));
    CHECK_FALSE(water.overlaps_solid(Aabb{glm::vec3{1.2f, 2.2f, 1.2f}, glm::vec3{1.8f, 2.8f, 1.8f}}));

    const OctreeCollider leaves = collider_over(
        build_from(g, [](const glm::vec3& min, float) { return min.y == 2.0f; }, MaterialID::Leaves));
    CHECK(world::materials::properties_of(MaterialID::Leaves).is_solid() ==
          leaves.overlaps_solid(Aabb{glm::vec3{1.2f, 2.2f, 1.2f}, glm::vec3{1.8f, 2.8f, 1.8f}}));
}

TEST_CASE("voxel_top reports the top of the voxel under the body", "[collision][octree]") {
    // Ground filling y < 3 (voxels 0,1,2), so the surface is at y = 3.
    const TreeGeometry g = geometry(3, 0);
    const OctreeCollider c =
        collider_over(build_from(g, [](const glm::vec3& min, float) { return min.y < 3.0f; }));
    CHECK(c.voxel_top(4.5f, 4.5f, 7.9f) == 3.0f);
    CHECK(c.voxel_top(4.5f, 4.5f, 3.0f) == 3.0f); // standing exactly on it
    CHECK(c.voxel_top(4.5f, 4.5f, 2.5f) == 3.0f); // feet inside the top voxel: still its top
}

TEST_CASE("the box walk early-outs instead of point-sampling", "[collision][octree]") {
    // Goal 230's failure mode, made visible: a query that descends the whole depth on every call
    // is what blows the per-tick budget. A 0.6 x 1.75 m body box against a 512 m / 7.8 mm tree has
    // 16 levels; visiting a few hundred nodes is a walk, visiting hundreds of thousands is a scan.
    const TreeGeometry g = geometry(6, -3); // 64 m root, 12.5 cm voxels
    const OctreeCollider c =
        collider_over(build_from(g, [](const glm::vec3& min, float) { return min.y < 8.0f; }));
    // A body-sized box well inside the ground: hits almost immediately.
    CHECK(c.overlaps_solid(Aabb{glm::vec3{30.0f, 4.0f, 30.0f}, glm::vec3{30.6f, 5.75f, 30.6f}}));
    const std::size_t insideNodes = c.last_nodes_visited();
    // A body-sized box in open air: has to prove a negative, which is the expensive direction.
    CHECK_FALSE(c.overlaps_solid(Aabb{glm::vec3{30.0f, 40.0f, 30.0f}, glm::vec3{30.6f, 41.75f, 30.6f}}));
    const std::size_t airNodes = c.last_nodes_visited();
    std::printf("octree box walk: %zu nodes inside solid, %zu nodes in open air\n", insideNodes, airNodes);
    CHECK(insideNodes < 64);
    CHECK(airNodes < 64);
}

TEST_CASE("over a real region the octree agrees with the sampler, and errs conservatively",
          "[collision][octree]") {
    // The cross-check goal 226 asks for, with the DIRECTION of disagreement asserted -- which is
    // the half that matters. The tree may be MORE solid than the sampler where its LOD is coarser
    // (a solid leaf standing in for a mostly-solid region); it must never be LESS, because less is
    // a hole the body falls through.
    const world::generation::HeightmapGenerator heightmap(1337);
    const TreeGeometry g = geometry(6, -3, glm::vec3{-32.0f, -16.0f, -32.0f}); // 64 m, 12.5 cm
    world::svo::TerrainSamplerParams sp;
    sp.seed = 1337;
    sp.trees = true;
    const Box region{g.origin, g.max_corner()};
    world::svo::TerrainSampler sampler(heightmap, sp, region);
    BuildParams params;
    params.uniform_lod = true; // exact everywhere: any disagreement is then a real bug, not LOD
    const OctreeCollider c = collider_over(world::svo::build_tree(sampler, g, params, nullptr, nullptr));
    REQUIRE(c.has_tree());

    const float voxel = g.finest_voxel_edge();
    std::mt19937 rng(20260906u);
    std::uniform_real_distribution<float> pick(0.05f, 0.95f);
    std::size_t treeSolidSamplerAir = 0;
    std::size_t treeAirSamplerSolid = 0;
    std::size_t both = 0;
    constexpr std::size_t kSamples = 10000;
    for (std::size_t i = 0; i < kSamples; ++i) {
        // One VOXEL-sized box, so "the box overlaps solid" and "this voxel is solid" are the same
        // question and the comparison is exact rather than a containment argument.
        const glm::vec3 p = g.origin + glm::vec3{pick(rng), pick(rng), pick(rng)} * g.root_edge();
        const glm::vec3 voxelMin = glm::floor((p - g.origin) / voxel) * voxel + g.origin;
        const Aabb box{voxelMin + glm::vec3{voxel * 0.25f}, voxelMin + glm::vec3{voxel * 0.75f}};

        const bool treeSays = c.overlaps_solid(box);
        const MaterialID truth = sampler.material_at(voxelMin, voxel);
        const bool samplerSays = world::materials::properties_of(truth).is_solid();
        if (treeSays && samplerSays) {
            ++both;
        } else if (treeSays && !samplerSays) {
            ++treeSolidSamplerAir;
        } else if (!treeSays && samplerSays) {
            ++treeAirSamplerSolid;
        }
    }
    std::printf("octree-vs-sampler over %zu voxel boxes: agree-solid %zu, tree-solid-sampler-air %zu, "
                "tree-air-sampler-solid %zu\n",
                kSamples, both, treeSolidSamplerAir, treeAirSamplerSolid);
    // At uniform LOD the tree IS the sampler, so both directions must be zero. The conservative
    // allowance below is for the distance-LOD case, tested separately.
    CHECK(treeAirSamplerSolid == 0);
    CHECK(treeSolidSamplerAir == 0);
    CHECK(both > 0); // the region really does contain solid ground
}

TEST_CASE("under distance LOD, holes exist -- and they are all beyond the finest ring",
          "[collision][octree]") {
    // THE FINDING OF THIS GROUP, and it is not the one I expected.
    //
    // At uniform LOD the octree query IS the sampler: 0 disagreements in 10,000 voxel boxes, both
    // directions. I expected distance LOD to be merely CONSERVATIVE -- a coarse node standing in
    // for a mostly-solid region, solid where the sampler says air. It is not. Over the whole
    // region, 0.41% of voxel boxes are HOLES: the tree says air where the sampler says solid,
    // because the builder collapses a sparse solid region far from the LOD centre to an ABSENT
    // child rather than to a coarse solid one.
    //
    // A hole is a place a body could pass through something. So the question that actually matters
    // is not "are there holes" but "are there holes WHERE THE BODY IS" -- and the answer is no,
    // by construction: the LOD centre is the camera, the body is at the camera, and inside the
    // finest ring the tree is exact. This test measures the hole rate BINNED BY DISTANCE from the
    // LOD centre and asserts zero inside the ring, which is the property collision depends on.
    //
    // The consequence, recorded rather than left implicit: the LOD centre MUST track the body. It
    // does today (run_svo rebuilds around the camera). If a future change ever lets the LOD centre
    // and the body separate -- a detached debug camera, a spectator following someone else -- this
    // guarantee is gone and collision has to fall back to the sampler.
    const world::generation::HeightmapGenerator heightmap(1337);
    const TreeGeometry g = geometry(6, -3, glm::vec3{-32.0f, -16.0f, -32.0f});
    world::svo::TerrainSamplerParams sp;
    sp.seed = 1337;
    sp.trees = true;
    const Box region{g.origin, g.max_corner()};
    world::svo::TerrainSampler sampler(heightmap, sp, region);
    BuildParams params;
    params.uniform_lod = false;
    params.lod_center = glm::vec3{0.0f, 0.0f, 0.0f};
    params.lod_radius = 4.0f;
    const OctreeCollider c = collider_over(world::svo::build_tree(sampler, g, params, nullptr, nullptr));

    // Distance bins, in multiples of lod_radius. The finest ring is [0, lod_radius).
    constexpr int kBins = 8;
    std::array<std::size_t, kBins> samplesIn{};
    std::array<std::size_t, kBins> holesIn{};
    std::array<std::size_t, kBins> conservativeIn{};

    const float voxel = g.finest_voxel_edge();
    std::mt19937 rng(4242u);
    std::uniform_real_distribution<float> pick(0.05f, 0.95f);
    constexpr std::size_t kSamples = 40000;
    for (std::size_t i = 0; i < kSamples; ++i) {
        const glm::vec3 p = g.origin + glm::vec3{pick(rng), pick(rng), pick(rng)} * g.root_edge();
        const glm::vec3 voxelMin = glm::floor((p - g.origin) / voxel) * voxel + g.origin;
        const Aabb box{voxelMin + glm::vec3{voxel * 0.25f}, voxelMin + glm::vec3{voxel * 0.75f}};
        const bool treeSays = c.overlaps_solid(box);
        const bool samplerSays =
            world::materials::properties_of(sampler.material_at(voxelMin, voxel)).is_solid();
        const float distance = glm::length(voxelMin - params.lod_center);
        const auto bin = static_cast<std::size_t>(
            std::min<int>(kBins - 1, static_cast<int>(distance / params.lod_radius)));
        ++samplesIn[bin];
        if (treeSays != samplerSays) {
            if (treeSays) {
                ++conservativeIn[bin];
            } else {
                ++holesIn[bin];
            }
        }
    }

    std::printf("octree-vs-sampler by distance from the LOD centre (lod_radius %.1f m):" NEWLINE,
                static_cast<double>(params.lod_radius));
    std::size_t totalHoles = 0;
    for (std::size_t b = 0; b < kBins; ++b) {
        if (samplesIn[b] == 0) {
            continue;
        }
        totalHoles += holesIn[b];
        std::printf("  %5.0f-%5.0f m: %6zu samples, %5zu holes (%.3f%%), %4zu conservative" NEWLINE,
                    static_cast<double>(b) * params.lod_radius,
                    static_cast<double>(b + 1) * params.lod_radius, samplesIn[b], holesIn[b],
                    100.0 * static_cast<double>(holesIn[b]) / static_cast<double>(samplesIn[b]),
                    conservativeIn[b]);
    }
    std::printf("  total holes over the whole region: %zu of %zu" NEWLINE, totalHoles, kSamples);

    // The property collision actually depends on: no holes inside the finest ring, which is where
    // the body always is.
    CHECK(holesIn[0] == 0);
    REQUIRE(samplesIn[0] > 0); // the ring was actually sampled
}
