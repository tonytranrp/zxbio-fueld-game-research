#include <cmath>
#include <cstdio>
#include <random>

#include <catch2/catch_test_macros.hpp>

#include "world/svo/brick_tree.hpp"
#include "world/svo/ray_trace.hpp"
#include "world/svo/tree_builder.hpp"

#include "detail/tree_builder_impl.hpp"

#include "test_samplers.hpp"

using namespace world::svo;
using svo_tests::RandomSampler;
using svo_tests::SphereSampler;
using world::chunk::MaterialID;

namespace {

TreeGeometry geometry(int rootLog2, int voxelLog2, glm::vec3 origin = glm::vec3{0.0f}) {
    TreeGeometry g;
    g.origin = origin;
    g.root_size_log2 = rootLog2;
    g.voxel_size_log2 = voxelLog2;
    return g;
}

// Compares the hierarchical marcher against the brute-force finest-voxel DDA over `count`
// random rays: origins inside and outside the root, random directions. Returns the hit count.
std::size_t compare_against_oracle(const BrickTree& tree, std::size_t count, std::uint32_t seed) {
    std::mt19937 rng(seed);
    const float edge = tree.geometry.root_edge();
    std::uniform_real_distribution<float> inside(0.0f, edge);
    std::uniform_real_distribution<float> around(-edge, 2.0f * edge);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    std::size_t hits = 0;
    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < count; ++i) {
        Ray ray;
        const bool startInside = (i % 2) == 0;
        ray.origin = tree.geometry.origin + (startInside ? glm::vec3{inside(rng), inside(rng), inside(rng)}
                                                         : glm::vec3{around(rng), around(rng), around(rng)});
        glm::vec3 d{unit(rng), unit(rng), unit(rng)};
        if (i % 7 == 0) {
            d[static_cast<int>(i % 3)] = 0.0f; // axis-aligned-plane rays exercise the zero-component path
        }
        if (glm::length(d) < 1.0e-3f) {
            d = glm::vec3{1.0f, 0.0f, 0.0f};
        }
        ray.dir = glm::normalize(d);

        const Hit fast = trace_ray(tree, ray);
        const Hit truth = trace_ray_brute_force(tree, ray);
        bool ok = fast.hit == truth.hit;
        if (ok && fast.hit) {
            ok = std::abs(fast.t - truth.t) <= 1.0e-3f * edge && fast.material == truth.material &&
                 fast.normal == truth.normal;
            ++hits;
        }
        if (!ok) {
            if (mismatches < 8) {
                std::printf(
                    "ray %zu mismatch: origin(%.3f,%.3f,%.3f) dir(%.3f,%.3f,%.3f) fast{hit=%d t=%.4f m=%d "
                    "n=(%d,%d,%d) lvl=%d} truth{hit=%d t=%.4f m=%d n=(%d,%d,%d)}\n",
                    i, static_cast<double>(ray.origin.x), static_cast<double>(ray.origin.y),
                    static_cast<double>(ray.origin.z), static_cast<double>(ray.dir.x),
                    static_cast<double>(ray.dir.y), static_cast<double>(ray.dir.z), fast.hit ? 1 : 0,
                    static_cast<double>(fast.t), static_cast<int>(fast.material), fast.normal.x,
                    fast.normal.y, fast.normal.z, fast.level, truth.hit ? 1 : 0, static_cast<double>(truth.t),
                    static_cast<int>(truth.material), truth.normal.x, truth.normal.y, truth.normal.z);
            }
            ++mismatches;
        }
    }
    std::printf("oracle comparison: %zu rays, %zu hits, %zu mismatches\n", count, hits, mismatches);
    CHECK(mismatches == 0);
    return hits;
}

} // namespace

TEST_CASE("rays hit a voxelized sphere where the analytic sphere says they should", "[svo][trace]") {
    const TreeGeometry g = geometry(5, 0);
    const SphereSampler sphere{glm::vec3{16.0f}, 10.0f, MaterialID::Stone};
    BuildParams params;
    params.uniform_lod = true;
    const BrickTree tree = build_tree(sphere, g, params, nullptr, nullptr);

    // From far outside, straight at the center: t ~ distance - radius, +X face hit first.
    Ray ray;
    ray.origin = glm::vec3{-50.0f, 16.5f, 16.5f};
    ray.dir = glm::vec3{1.0f, 0.0f, 0.0f};
    const Hit hit = trace_ray(tree, ray);
    REQUIRE(hit.hit);
    CHECK(hit.material == MaterialID::Stone);
    CHECK(std::abs(hit.t - (50.0f + 16.0f - 10.0f)) <= 1.0f); // within one voxel of the analytic surface
    CHECK(hit.normal == glm::ivec3{-1, 0, 0});
    CHECK(hit.level == g.max_brick_level());
    CHECK_FALSE(hit.lod_cube);

    // Grazing past the sphere: miss.
    ray.origin = glm::vec3{-50.0f, 16.5f, 28.5f};
    CHECK_FALSE(trace_ray(tree, ray).hit);

    // Pointing away from the root entirely: miss.
    ray.dir = glm::vec3{-1.0f, 0.0f, 0.0f};
    CHECK_FALSE(trace_ray(tree, ray).hit);

    // Starting inside the solid: immediate hit at t = 0 with a fallback normal against the ray.
    ray.origin = glm::vec3{16.5f};
    ray.dir = glm::normalize(glm::vec3{0.2f, -1.0f, 0.1f});
    const Hit insideHit = trace_ray(tree, ray);
    REQUIRE(insideHit.hit);
    CHECK(insideHit.t == 0.0f);
    CHECK(insideHit.normal == glm::ivec3{0, 1, 0});

    // From directly above, downward: -Y direction hits the +Y face.
    ray.origin = glm::vec3{16.5f, 40.0f, 16.5f};
    ray.dir = glm::vec3{0.0f, -1.0f, 0.0f};
    const Hit top = trace_ray(tree, ray);
    REQUIRE(top.hit);
    CHECK(top.normal == glm::ivec3{0, 1, 0});
    CHECK(std::abs(top.t - (40.0f - 26.0f)) <= 1.0f);
}

TEST_CASE("hierarchical traversal matches the brute-force oracle on a dense random tree", "[svo][trace]") {
    const TreeGeometry g = geometry(5, 0, glm::vec3{-7.0f, 3.0f, 11.0f}); // off-origin root
    RandomSampler random;
    random.lattice = 1.0f;
    random.density256 = 38; // ~15% occupancy: lots of thin structure
    BuildParams params;
    params.uniform_lod = true;
    const BrickTree tree = build_tree(random, g, params, nullptr, nullptr);
    REQUIRE_FALSE(tree.empty());
    const std::size_t hits = compare_against_oracle(tree, 4000, 1);
    CHECK(hits > 1500);
}

TEST_CASE("traversal matches the oracle across mixed brick levels (distance LOD)", "[svo][trace]") {
    // 64 m root at 0.5 m voxels (V = 7): LOD near a corner leaves fine bricks next to coarse ones,
    // which is exactly the cross-level stepping a single-resolution test never exercises.
    const TreeGeometry g = geometry(6, -1);
    const SphereSampler sphere{glm::vec3{32.0f}, 28.0f, MaterialID::Grass};
    BuildParams params;
    params.lod_center = glm::vec3{4.0f, 32.0f, 32.0f};
    params.lod_radius = 3.0f;
    const BrickTree tree = build_tree(sphere, g, params, nullptr, nullptr);
    REQUIRE_FALSE(tree.empty());
    CHECK(tree.stats().deepest_level == g.max_brick_level());
    compare_against_oracle(tree, 3000, 2);
}

TEST_CASE("LOD early-out returns coarse cube hits no farther than the exact hit", "[svo][trace]") {
    const TreeGeometry g = geometry(5, 0);
    const SphereSampler sphere{glm::vec3{16.0f}, 10.0f, MaterialID::Stone};
    BuildParams params;
    params.uniform_lod = true;
    const BrickTree tree = build_tree(sphere, g, params, nullptr, nullptr);

    Ray ray;
    ray.origin = glm::vec3{-200.0f, 16.5f, 16.5f};
    ray.dir = glm::vec3{1.0f, 0.0f, 0.0f};
    const Hit exact = trace_ray(tree, ray);
    TraceParams lod;
    lod.lod_pixel_angle = 0.05f; // ~an 8 m cell subtends one "pixel" at 160 m: very coarse
    const Hit coarse = trace_ray(tree, ray, lod);
    REQUIRE(exact.hit);
    REQUIRE(coarse.hit);
    CHECK(coarse.lod_cube);
    CHECK(coarse.level < exact.level);
    CHECK(coarse.t <= exact.t + 1.0e-3f);
    CHECK(coarse.material == MaterialID::Stone);
}

TEST_CASE("empty tree never hits", "[svo][trace]") {
    BrickTree tree;
    tree.geometry = geometry(5, 0);
    Ray ray;
    ray.origin = glm::vec3{16.0f};
    CHECK_FALSE(trace_ray(tree, ray).hit);
    CHECK_FALSE(trace_ray_brute_force(tree, ray).hit);
}
