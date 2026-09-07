// Prompt 004 goal 266: the beam bound's correctness, which is the ONLY thing that matters about it.
//
// A start-t seeded past real geometry makes a ray miss or land on a farther surface, and the
// artefact -- a hole, or a wrong material -- appears somewhere unrelated to the code that caused
// it. So the property under test is not "the bound is tight" but:
//
//     for every ray inside the cone, beam_start_t(cone) <= trace_ray(ray).t
//
// asserted over thousands of real rays through a real tree, plus the degenerate cases where a
// wrong bound is easiest to write: an apex inside geometry, a cone that misses everything, and a
// zero-angle cone (which must agree with the ray it degenerates to).

#include <cmath>
#include <limits>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "detail/tree_builder_impl.hpp"
#include "test_samplers.hpp"
#include "world/svo/beam.hpp"
#include "world/svo/ray_trace.hpp"
#include "world/svo/tree_builder.hpp"

using namespace world::svo;
using svo_tests::SphereSampler;

namespace {

// A tree with structure at several scales, so the descent has real pruning to do rather than
// bottoming out on one flat surface.
BrickTree make_tree() {
    TreeGeometry geometry;
    geometry.root_size_log2 = 6;   // 64 m
    geometry.voxel_size_log2 = -2; // 0.25 m
    return build_tree(SphereSampler{glm::vec3{32.0f, 32.0f, 32.0f}, 18.0f}, geometry, BuildParams{});
}

// Rays filling the cone: the centre, the four corners, and a spread of interior directions.
std::vector<glm::vec3> cone_rays(const glm::vec3& dir, float tanHalf, int count) {
    // An orthonormal frame around dir.
    const glm::vec3 up = std::abs(dir.y) < 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{1.0f, 0.0f, 0.0f};
    const glm::vec3 right = glm::normalize(glm::cross(dir, up));
    const glm::vec3 realUp = glm::cross(right, dir);

    std::vector<glm::vec3> out;
    out.push_back(dir);
    std::uint32_t rng = 0x1234567u;
    const auto next = [&] {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return static_cast<float>(rng % 20001u) / 10000.0f - 1.0f; // [-1, 1]
    };
    for (int i = 0; i < count; ++i) {
        // Sample the square that INSCRIBES the tile inside the cone -- every such ray is within
        // the cone's half-angle by construction, since the cone was built from the tile's corner.
        const float u = next() * tanHalf * 0.70710678f;
        const float v = next() * tanHalf * 0.70710678f;
        out.push_back(glm::normalize(dir + right * u + realUp * v));
    }
    return out;
}

void expect_conservative(const BrickTree& tree, const glm::vec3& origin, const glm::vec3& dir,
                         float tanHalf) {
    Beam beam;
    beam.origin = origin;
    beam.dir = glm::normalize(dir);
    beam.tan_half_angle = tanHalf;
    const float bound = beam_start_t(tree, beam);

    for (const glm::vec3& rayDir : cone_rays(beam.dir, tanHalf, 400)) {
        Ray ray;
        ray.origin = origin;
        ray.dir = rayDir;
        const Hit hit = trace_ray(tree, ray, TraceParams{});
        if (!hit.hit) {
            continue; // a miss constrains nothing
        }
        // The bound may not exceed any real hit. A small slack absorbs float slab arithmetic; it is
        // four orders of magnitude below the 0.25 m voxel, so it cannot hide a real overshoot.
        REQUIRE(bound <= hit.t + 1.0e-4f);
    }
}

} // namespace

TEST_CASE("the beam bound never exceeds any ray in its cone", "[svo][beam]") {
    const BrickTree tree = make_tree();
    REQUIRE_FALSE(tree.empty());

    // Outside looking in, from several sides and at several cone widths -- 0.02 rad is roughly a
    // 16x16 tile at this project's field of view and resolution.
    for (const float tanHalf : {0.0f, 0.005f, 0.02f, 0.06f}) {
        expect_conservative(tree, glm::vec3{32.0f, 32.0f, -20.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, tanHalf);
        expect_conservative(tree, glm::vec3{-20.0f, 40.0f, 32.0f}, glm::vec3{1.0f, -0.2f, 0.0f}, tanHalf);
        expect_conservative(tree, glm::vec3{80.0f, 80.0f, 80.0f}, glm::vec3{-1.0f, -1.0f, -1.0f}, tanHalf);
        // A grazing angle: the pose class where a conservative bound is tightest and most likely to
        // be got wrong.
        expect_conservative(tree, glm::vec3{-20.0f, 50.5f, 32.0f}, glm::vec3{1.0f, -0.02f, 0.0f}, tanHalf);
    }
}

TEST_CASE("an apex inside the geometry bounds at zero", "[svo][beam]") {
    const BrickTree tree = make_tree();
    Beam beam;
    beam.origin = glm::vec3{32.0f, 32.0f, 32.0f}; // the sphere's centre
    beam.dir = glm::vec3{0.0f, 0.0f, 1.0f};
    beam.tan_half_angle = 0.02f;
    REQUIRE(beam_start_t(tree, beam) == Catch::Approx(0.0f));
}

TEST_CASE("a cone that meets nothing returns infinity", "[svo][beam]") {
    const BrickTree tree = make_tree();
    Beam beam;
    beam.origin = glm::vec3{32.0f, 32.0f, -20.0f};
    beam.dir = glm::vec3{0.0f, -1.0f, 0.0f}; // straight down, away from the root entirely
    beam.tan_half_angle = 0.01f;
    REQUIRE(std::isinf(beam_start_t(tree, beam)));

    // And `max_t` is honoured: a cone aimed at real geometry but cut short of it finds nothing.
    Beam capped;
    capped.origin = glm::vec3{32.0f, 32.0f, -20.0f};
    capped.dir = glm::vec3{0.0f, 0.0f, 1.0f};
    capped.tan_half_angle = 0.0f;
    capped.max_t = 1.0f;
    REQUIRE(std::isinf(beam_start_t(tree, capped)));
}

TEST_CASE("an empty tree bounds at infinity rather than reading a root that is not there", "[svo][beam]") {
    const BrickTree empty;
    Beam beam;
    beam.dir = glm::vec3{0.0f, 0.0f, 1.0f};
    REQUIRE(std::isinf(beam_start_t(empty, beam)));
}

TEST_CASE("a wider cone gives a bound no tighter than a narrower one", "[svo][beam]") {
    // Monotonicity: widening the cone can only admit more geometry, so the bound can only fall.
    // This is the property a pruning bug breaks -- prune too eagerly and a wide cone reports a
    // LARGER bound than a narrow one covering the same rays.
    const BrickTree tree = make_tree();
    const glm::vec3 origin{-20.0f, 41.0f, 30.0f};
    const glm::vec3 dir = glm::normalize(glm::vec3{1.0f, -0.15f, 0.05f});

    float previous = std::numeric_limits<float>::infinity();
    for (const float tanHalf : {0.0f, 0.002f, 0.01f, 0.03f, 0.08f, 0.2f}) {
        Beam beam;
        beam.origin = origin;
        beam.dir = dir;
        beam.tan_half_angle = tanHalf;
        const float bound = beam_start_t(tree, beam);
        REQUIRE(bound <= previous + 1.0e-4f);
        previous = bound;
    }
}

TEST_CASE("a zero-angle cone agrees with the ray it degenerates to", "[svo][beam]") {
    const BrickTree tree = make_tree();
    const glm::vec3 origin{32.0f, 32.0f, -20.0f};
    const glm::vec3 dir{0.0f, 0.0f, 1.0f};

    Beam beam;
    beam.origin = origin;
    beam.dir = dir;
    beam.tan_half_angle = 0.0f;
    const float bound = beam_start_t(tree, beam);

    Ray ray;
    ray.origin = origin;
    ray.dir = dir;
    const Hit hit = trace_ray(tree, ray, TraceParams{});
    REQUIRE(hit.hit);
    // A BOUND, not an equality: it stops at the entry of the node containing the surface, which
    // for a coarse brick leaf can be a whole node early. This ray is the pathological case and it
    // is worth naming, because chasing it as a bug cost real time: it runs exactly along the corner
    // where four octants meet, so it grazes the boundary of a 16 m brick node at t = 20 whose
    // geometry does not start until t = 34. The box IS entered at 20 and DOES contain geometry, so
    // 20 is correct -- the looseness is the node's own extent, not an error.
    REQUIRE(bound <= hit.t + 1.0e-4f);
    REQUIRE(bound > 0.0f);
}

TEST_CASE("the bound's tightness is the LOD leaf size, and that is not a defect", "[svo][beam]") {
    // WHAT LIMITS THIS BOUND, measured rather than assumed -- and worth stating because chasing it
    // as a bug cost real time twice.
    //
    // The bound is the entry distance of the first node that CONTAINS geometry, so its slack is
    // that node's own extent. In this test tree the builder represents the sphere's near face with
    // a brick leaf 16 m across, whose front face is at the root's own boundary -- so the bound is
    // 20 (the root entry) for geometry at 34. Correct, and as tight as this tree allows.
    //
    // The consequence for the real engine is the point: a beam pre-pass buys exactly as much as the
    // tree is FINE along the ray, and this engine's tree is deliberately coarse at distance. That
    // is why goal 266's decision rests on a measurement over real terrain (see
    // research/frame-time-log.md section 13), not on this test.
    const BrickTree tree = make_tree();
    const glm::vec3 origin{30.5f, 33.25f, -20.0f};
    const glm::vec3 dir = glm::normalize(glm::vec3{0.02f, -0.01f, 1.0f});

    Beam beam;
    beam.origin = origin;
    beam.dir = dir;
    beam.tan_half_angle = 0.0f;
    const float bound = beam_start_t(tree, beam);

    Ray ray;
    ray.origin = origin;
    ray.dir = dir;
    const Hit hit = trace_ray(tree, ray, TraceParams{});
    REQUIRE(hit.hit);
    REQUIRE(bound <= hit.t + 1.0e-4f);
    REQUIRE(bound > 0.0f);
}

TEST_CASE("seeding a trace with the bound does not change what it hits", "[svo][beam]") {
    // The end-to-end property: t_start from the beam must be invisible in the result. This is the
    // check that would have caught a non-conservative bound as a changed image.
    const BrickTree tree = make_tree();
    const glm::vec3 origin{-20.0f, 44.0f, 30.0f};

    int compared = 0;
    for (int i = -12; i <= 12; ++i) {
        for (int j = -12; j <= 12; ++j) {
            const glm::vec3 dir =
                glm::normalize(glm::vec3{1.0f, static_cast<float>(i) * 0.02f, static_cast<float>(j) * 0.02f});
            Beam beam;
            beam.origin = origin;
            beam.dir = dir;
            beam.tan_half_angle = 0.02f;

            Ray ray;
            ray.origin = origin;
            ray.dir = dir;
            const Hit plain = trace_ray(tree, ray, TraceParams{});

            TraceParams seeded;
            seeded.t_start = beam_start_t(tree, beam);
            if (std::isinf(seeded.t_start)) {
                REQUIRE_FALSE(plain.hit); // nothing in the cone means nothing on this ray either
                continue;
            }
            const Hit fast = trace_ray(tree, ray, seeded);
            REQUIRE(fast.hit == plain.hit);
            if (plain.hit) {
                REQUIRE(fast.t == Catch::Approx(plain.t));
                REQUIRE(fast.material == plain.material);
                REQUIRE(fast.normal == plain.normal);
            }
            ++compared;
        }
    }
    REQUIRE(compared > 500);
}
