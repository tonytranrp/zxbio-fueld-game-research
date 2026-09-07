#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <random>

#include "../src/aim_query.hpp"
#include "world/materials/materials.hpp"
#include "world/svo/ray_trace.hpp"
#include "world/svo/terrain_sampler.hpp"
#include "world/svo/tree_builder.hpp"

using world::chunk::MaterialID;

// Goal 84's check: the crosshair query against known columns of the real generator.
//
// These pass an EXPLICIT 300 m range. They were written when that was the default; goal 242 made the
// default 34 m (the distance a 1 cm detail stays resolvable at 20/20), which is shorter than the
// drops these cases use. Their subject is classification and distance, not range -- the range itself
// is tested separately below -- so stating it here keeps each case about one thing.
TEST_CASE("aim query classifies known columns like the terrain fill", "[aim]") {
    const world::generation::HeightmapGenerator generator(1337);

    SECTION("straight down over land hits the surface with a land material") {
        // Find a genuinely above-sea column by scanning a line (deterministic for seed 1337).
        float lx = 0.0f;
        float lz = 0.0f;
        float surface = -1000.0f;
        for (float x = 0.0f; x < 200.0f && surface < 5.0f; x += 7.0f) {
            for (float z = 0.0f; z < 200.0f && surface < 5.0f; z += 7.0f) {
                const float h = generator.height_at(x, z);
                if (h > 5.0f) {
                    lx = x;
                    lz = z;
                    surface = h;
                }
            }
        }
        REQUIRE(surface > 5.0f);

        const app::AimHit hit =
            app::query_aim(generator, {lx, surface + 40.0f, lz}, {0.0f, -1.0f, 0.0f}, 300.0f);
        REQUIRE(hit.hit);
        CHECK(std::abs(hit.position.y - surface) < 0.6f); // refined onto the surface
        // Above the beach band, the material must be the grass/stone slope rule -- never water/sand.
        CHECK((hit.material == MaterialID::Grass || hit.material == MaterialID::Stone));
    }

    SECTION("straight down over deep water hits the water plane") {
        float lx = 0.0f;
        float lz = 0.0f;
        float surface = 1000.0f;
        for (float x = 0.0f; x < 300.0f && surface > -6.0f; x += 7.0f) {
            for (float z = 0.0f; z < 300.0f && surface > -6.0f; z += 7.0f) {
                const float h = generator.height_at(x, z);
                if (h < -6.0f) {
                    lx = x;
                    lz = z;
                    surface = h;
                }
            }
        }
        REQUIRE(surface < -6.0f);

        const app::AimHit hit = app::query_aim(generator, {lx, 30.0f, lz}, {0.0f, -1.0f, 0.0f}, 300.0f);
        REQUIRE(hit.hit);
        CHECK(hit.material == MaterialID::Water);
        CHECK(std::abs(hit.position.y) < 0.6f); // the sea-level plane, not the seabed
    }

    SECTION("aiming at open sky misses") {
        const app::AimHit hit = app::query_aim(generator, {0.0f, 120.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 300.0f);
        CHECK(!hit.hit);
    }

    SECTION("the reported distance is the real distance to the hit") {
        // Prompt 001 A4: the overlay line gained a range, so it has to be one.
        const app::AimHit hit = app::query_aim(generator, {0.0f, 200.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 300.0f);
        REQUIRE(hit.hit);
        CHECK(std::abs(hit.distance - glm::length(hit.position - glm::vec3{0.0f, 200.0f, 0.0f})) < 0.01f);
    }
}

// Prompt 001 A4: before this the query saw only the height field, so a trunk or a canopy filling
// the screen reported as whatever hillside stood BEHIND it.
TEST_CASE("aim query sees trees", "[aim][trees]") {
    constexpr int kSeed = 1337;
    const world::generation::HeightmapGenerator generator(kSeed);
    const app::TreeLookup trees(generator, kSeed);

    // Find a real tree the generator actually placed, rather than inventing one: the point of the
    // test is that the query agrees with the world the voxelizer builds from the same function.
    // The tree also has to be one you can actually SEE from 6 m away -- the first draft of this
    // test picked the first trunk it found and fired a ray from inside a hillside, which correctly
    // reported Stone at 2 mm. Requiring a clear line of sight (checked with the terrain-only query)
    // makes the test measure what it claims to.
    constexpr float kStandoff = 6.0f;
    std::optional<world::generation::TreePlacement> found;
    glm::vec3 origin{0.0f};
    glm::vec3 trunkMid{0.0f};
    for (std::int32_t cx = 0; cx < 8 && !found; ++cx) {
        for (std::int32_t cz = 0; cz < 8 && !found; ++cz) {
            for (const auto& tree : world::generation::compute_tree_placements(cx, cz, kSeed, generator)) {
                world::generation::TrunkBox trunk{};
                if (!world::generation::tree_trunk(tree, trunk)) {
                    continue; // Shrub: no trunk to aim at
                }
                const glm::vec3 mid{tree.world_x, (trunk.y0 + trunk.y1) * 0.5f, tree.world_z};
                const glm::vec3 eye = mid + glm::vec3{kStandoff, 0.0f, 0.0f};
                if (generator.height_at(eye.x, eye.z) >= eye.y) {
                    continue; // the viewpoint is underground
                }
                const app::AimHit terrainOnly = app::query_aim(generator, eye, {-1.0f, 0.0f, 0.0f}, 300.0f);
                if (terrainOnly.hit && terrainOnly.distance < kStandoff + 1.0f) {
                    continue; // a hillside stands between the eye and the trunk
                }
                found = tree;
                origin = eye;
                trunkMid = mid;
                break;
            }
        }
    }
    REQUIRE(found.has_value());

    SECTION("a horizontal ray into a trunk reads Wood, at the trunk's range") {
        const app::AimHit hit = app::query_aim(generator, origin, {-1.0f, 0.0f, 0.0f}, 300.0f, &trees);
        REQUIRE(hit.hit);
        CHECK(hit.material == MaterialID::Wood);
        CHECK(hit.distance > kStandoff - 1.0f);
        CHECK(hit.distance < kStandoff + 1.0f);
    }

    SECTION("without the tree source the same ray does not report Wood") {
        // The regression this closes, stated as a test: the pre-A4 query cannot see the trunk.
        const app::AimHit hit = app::query_aim(generator, origin, {-1.0f, 0.0f, 0.0f}, 300.0f);
        CHECK(hit.material != MaterialID::Wood);
    }

    SECTION("straight down through the canopy reads Leaves before the ground") {
        std::array<world::generation::CanopyLobe, world::generation::kMaxCanopyLobes> lobes{};
        const std::size_t count = world::generation::tree_canopy_lobes(*found, lobes);
        REQUIRE(count > 0);
        const app::AimHit hit = app::query_aim(generator, lobes[0].center + glm::vec3{0.0f, 30.0f, 0.0f},
                                               {0.0f, -1.0f, 0.0f}, 300.0f, &trees);
        REQUIRE(hit.hit);
        CHECK(hit.material == MaterialID::Leaves);
        CHECK(hit.position.y > found->base_height); // above the ground it would otherwise have hit
    }
}

// --- goal 241: the crosshair asks the same structure the body does --------------------------------

namespace {

world::svo::TreeGeometry aim_geometry(int rootLog2, int voxelLog2, glm::vec3 origin) {
    world::svo::TreeGeometry g;
    g.root_size_log2 = rootLog2;
    g.voxel_size_log2 = voxelLog2;
    g.origin = origin;
    return g;
}

} // namespace

TEST_CASE("the octree aim query agrees with the analytic one over 2000 random rays", "[aim][octree]") {
    // Goal 241's check. Uniform LOD, so the tree IS the sampler and any disagreement is a real
    // difference between the two ways of asking rather than a resolution artefact.
    const world::generation::HeightmapGenerator heightmap(1337);
    const world::svo::TreeGeometry g = aim_geometry(6, -3, glm::vec3{-32.0f, -16.0f, -32.0f});
    world::svo::TerrainSamplerParams sp;
    sp.seed = 1337;
    sp.trees = true;
    const world::svo::Box region{g.origin, g.max_corner()};
    const world::svo::TerrainSampler sampler(heightmap, sp, region);
    world::svo::BuildParams params;
    params.uniform_lod = true;
    const world::svo::BrickTree tree = world::svo::build_tree(sampler, g, params, nullptr, nullptr);
    REQUIRE_FALSE(tree.empty());

    std::mt19937 rng(20260907u);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    std::uniform_real_distribution<float> inside(0.15f, 0.85f);

    std::size_t rays = 0;
    std::size_t hits = 0;
    std::size_t materialMatches = 0;
    std::size_t materialMismatches = 0;
    std::size_t startedInsideRays = 0;
    constexpr std::size_t kRays = 2000;
    for (std::size_t i = 0; i < kRays; ++i) {
        const glm::vec3 origin = g.origin + glm::vec3{inside(rng), inside(rng), inside(rng)} * g.root_edge();
        glm::vec3 dir{unit(rng), unit(rng), unit(rng)};
        if (glm::length(dir) < 1.0e-3f) {
            continue;
        }
        dir = glm::normalize(dir);
        ++rays;

        const app::AimHit hit = app::query_aim_octree(tree, origin, dir, g.root_edge());
        if (!hit.hit) {
            continue;
        }
        ++hits;
        const world::svo::Hit raw =
            world::svo::trace_ray(tree, world::svo::Ray{origin, dir}, world::svo::TraceParams{});
        // THE check: the material the readout names must be the material the octree holds at the
        // point the readout reports.
        //
        // The probe steps a quarter voxel INWARD ALONG THE HIT FACE'S NORMAL, not along the ray.
        // Along the ray was the first version and it reported 27 mismatches in 1272 hits -- every
        // one of them Stone-vs-Dirt or Dirt-vs-Air at brick level, with no LOD cube or solid leaf
        // involved. Those are VERTICALLY ADJACENT material bands: on a shallow ray, a quarter voxel
        // of forward travel crosses into the next voxel down, so the check was sampling a different
        // voxel from the one that was hit and measuring the band boundary rather than agreement.
        // The face normal is the only direction guaranteed to point into the voxel that was hit.
        const float voxel = g.finest_voxel_edge();
        // A t == 0 hit is a ray that STARTED inside solid: the tracer reports the origin's own voxel
        // immediately and its face normal is a default, not a surface it crossed. Offsetting along
        // that default walks into a neighbour, which is what the last 2 of the original 27
        // mismatches were -- both at t = 0.0000 with n = (0,0,1). The origin IS the hit, so there is
        // nothing to step back from.
        //
        // Worth stating why this is a test artefact and not a defect: the origins here are uniform
        // over the region, so some are underground. A real crosshair's origin is the camera, and
        // goal 228's counter asserts every tick that the camera is not inside solid.
        const bool startedInside = raw.t <= 0.0f;
        startedInsideRays += startedInside ? 1u : 0u;
        const glm::vec3 justInside =
            startedInside ? origin : hit.position - glm::vec3{raw.normal} * (voxel * 0.25f);
        // ...and then SNAP to that voxel's centre. An offset alone still leaves the sample near a
        // boundary when the ray hits an edge or corner (two or three faces at once), where the
        // normal names only one of them: 2 of 1272 survived the offset for exactly that reason. The
        // centre of the voxel the hit is inside is unambiguous by construction.
        const glm::vec3 probe =
            glm::floor((justInside - g.origin) / voxel) * voxel + g.origin + glm::vec3{voxel * 0.5f};
        if (tree.material_at(probe) == hit.material) {
            ++materialMatches;
        } else {
            ++materialMismatches;
            if (materialMismatches <= 6) {
                std::printf("  mismatch: aim=%s probe=%s level=%d t=%.4f pos=(%.4f,%.4f,%.4f) "
                            "n=(%d,%d,%d) probe=(%.4f,%.4f,%.4f) edge=%.5f\n",
                            world::materials::name_of(hit.material),
                            world::materials::name_of(tree.material_at(probe)), raw.level,
                            static_cast<double>(raw.t), static_cast<double>(hit.position.x),
                            static_cast<double>(hit.position.y), static_cast<double>(hit.position.z),
                            raw.normal.x, raw.normal.y, raw.normal.z, static_cast<double>(probe.x),
                            static_cast<double>(probe.y), static_cast<double>(probe.z),
                            static_cast<double>(raw.cube_edge));
            }
        }
    }
    std::printf("aim-vs-octree over %zu rays: %zu hits (%zu started inside solid), %zu material "
                "matches, %zu mismatches\n",
                rays, hits, startedInsideRays, materialMatches, materialMismatches);
    REQUIRE(rays > 1900);
    REQUIRE(hits > 100); // the region really does contain something to hit
    CHECK(materialMismatches == 0);
}

TEST_CASE("the octree aim query and the analytic one agree about terrain materials", "[aim][octree]") {
    // The two ways of asking, compared directly: straight down from above a land column. The
    // analytic query re-derives the surface from the height function; the octree query reads the
    // voxel that was built from it. Trees are excluded here because a canopy legitimately puts a
    // different material in front of the ground, which is the analytic query's own A4 fix.
    const world::generation::HeightmapGenerator heightmap(1337);
    const world::svo::TreeGeometry g = aim_geometry(6, -3, glm::vec3{-32.0f, -16.0f, -32.0f});
    world::svo::TerrainSamplerParams sp;
    sp.seed = 1337;
    sp.trees = false; // terrain only, so the comparison is like for like
    const world::svo::Box region{g.origin, g.max_corner()};
    const world::svo::TerrainSampler sampler(heightmap, sp, region);
    world::svo::BuildParams params;
    params.uniform_lod = true;
    const world::svo::BrickTree tree = world::svo::build_tree(sampler, g, params, nullptr, nullptr);

    std::size_t compared = 0;
    std::size_t agreed = 0;
    for (float x = -28.0f; x <= 28.0f; x += 4.0f) {
        for (float z = -28.0f; z <= 28.0f; z += 4.0f) {
            const float surface = heightmap.height_at(x, z);
            if (surface < g.origin.y + 1.0f || surface > g.max_corner().y - 2.0f) {
                continue; // the column's surface is outside this small tree
            }
            const glm::vec3 origin{x, g.max_corner().y - 0.5f, z};
            const glm::vec3 down{0.0f, -1.0f, 0.0f};
            const app::AimHit octree = app::query_aim_octree(tree, origin, down, g.root_edge());
            const app::AimHit analytic = app::query_aim(heightmap, origin, down, g.root_edge(), nullptr);
            if (!octree.hit || !analytic.hit) {
                continue;
            }
            ++compared;
            agreed += octree.material == analytic.material ? 1u : 0u;
        }
    }
    std::printf("aim octree-vs-analytic over %zu land columns: %zu agreed\n", compared, agreed);
    REQUIRE(compared > 20);
    // Not demanded to be perfect: the analytic query classifies by a slope computed at 1 m spacing
    // while the sampler classifies each voxel by its own column, so a band boundary can legitimately
    // fall a voxel either way. A disagreement rate above a few percent would be a real bug.
    CHECK(static_cast<double>(agreed) / static_cast<double>(compared) > 0.90);
}

// --- goal 242: the readout's range is derived, not round -----------------------------------------

TEST_CASE("the aim range is the distance a 1 cm detail stays resolvable at 20/20", "[aim][range]") {
    // Re-derived rather than trusted: d = s / tan(MAR). The research's own table gives 34 m for a
    // 1 cm detail at 1 arcmin, and ~54 m at the 0.64 arcmin 94-ppd ceiling.
    CHECK(app::resolvable_distance(0.01f, 1.0f) == Catch::Approx(34.4).margin(0.5));
    CHECK(app::resolvable_distance(0.01f, 0.64f) == Catch::Approx(53.7).margin(0.5));
    // And the shipped constant is that number, not a round one.
    CHECK(app::kAimResolvableRange == Catch::Approx(app::resolvable_distance(0.01f, 1.0f)).margin(0.5));
    CHECK(app::kAimResolvableRange < 300.0f); // the value it replaced
}

TEST_CASE("the readout goes blank beyond its range", "[aim][range]") {
    // Straight down from far above: a hit inside the range, nothing beyond it. The range bounds the
    // march itself, so "blank" is a property of the query rather than of the overlay.
    const world::generation::HeightmapGenerator heightmap(1337);
    float x = 0.0f;
    float z = 0.0f;
    float surface = -1000.0f;
    for (float sx = 0.0f; sx < 200.0f && surface < 5.0f; sx += 7.0f) {
        for (float sz = 0.0f; sz < 200.0f && surface < 5.0f; sz += 7.0f) {
            const float h = heightmap.height_at(sx, sz);
            if (h > 5.0f) {
                x = sx;
                z = sz;
                surface = h;
            }
        }
    }
    REQUIRE(surface > 5.0f);
    const glm::vec3 down{0.0f, -1.0f, 0.0f};

    const glm::vec3 near{x, surface + 10.0f, z};
    CHECK(app::query_aim(heightmap, near, down, app::kAimResolvableRange, nullptr).hit);

    // Same column, but the ground is now further away than the eye can resolve a centimetre at.
    const glm::vec3 far{x, surface + app::kAimResolvableRange + 20.0f, z};
    CHECK_FALSE(app::query_aim(heightmap, far, down, app::kAimResolvableRange, nullptr).hit);
    // ...and it is a RANGE limit, not a missing column: with the old 300 m it hits.
    CHECK(app::query_aim(heightmap, far, down, 300.0f, nullptr).hit);
}
