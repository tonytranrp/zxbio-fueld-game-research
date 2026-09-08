// Prompt 006 Group AM-A's Checks: the baked macro field, its reconstruction, and the derivative
// everything downstream asks for.
//
// The property under test in the first two cases is CONTINUITY, and it is the reason the
// reconstruction is Catmull-Rom rather than bilinear. `height_at`'s slope decides grass vs rock
// (`grass_max_slope`), masks tree placement, and sets the collider's walkability limit -- so a
// derivative that jumps at every cell boundary would put a 16 m grid of those decisions through
// the world, and it would be diagnosed as a shading bug three passes later. A test is cheaper.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

#include "world/generation/field/field_sampler.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/field/terrain_field.hpp"
#include "world/generation/heightmap_generator.hpp"

using world::generation::HeightmapGenerator;
using world::generation::field::FieldGeometry;
using world::generation::field::FieldSampler;
using world::generation::field::MacroParams;
using world::generation::field::Plane;
using world::generation::field::TerrainField;

namespace {

/// A small field so the tests are fast; the geometry is what matters, not the extent.
[[nodiscard]] FieldGeometry small_geometry() {
    return FieldGeometry{.origin_x = -512.0f, .origin_z = -512.0f, .cell_size = 16.0f, .cells = 64};
}

[[nodiscard]] std::shared_ptr<TerrainField> baked(int seed = 1337) {
    auto field = std::make_shared<TerrainField>(small_geometry());
    world::generation::field::run_pipeline(*field, MacroParams{.seed = seed});
    return field;
}

} // namespace

TEST_CASE("the macro field is SoA and reports its own size", "[generation][field]") {
    const TerrainField field{small_geometry()};
    CHECK(field.cells() == 64);
    CHECK(field.cell_count() == 64u * 64u);
    // Eight planes of 64x64 floats. Reported rather than estimated, which goal 296's Check asks for.
    CHECK(field.bytes() == 64ull * 64ull * static_cast<std::uint64_t>(Plane::Count) * sizeof(float));
    // Each plane is a distinct block: writing one must not disturb another.
    TerrainField writable{small_geometry()};
    writable.plane(Plane::Elevation)[10] = 5.0f;
    writable.plane(Plane::Temperature)[10] = -3.0f;
    CHECK(writable.plane(Plane::Elevation)[10] == Catch::Approx(5.0f));
    CHECK(writable.plane(Plane::Temperature)[10] == Catch::Approx(-3.0f));
}

TEST_CASE("the field geometry round-trips cell and world coordinates", "[generation][field]") {
    const FieldGeometry g = small_geometry();
    for (std::int32_t c : {0, 1, 17, 63}) {
        const glm::vec2 w = g.to_world(c, c);
        const glm::vec2 back = g.to_cell(w.x, w.y);
        CHECK(back.x == Catch::Approx(static_cast<float>(c)));
        CHECK(back.y == Catch::Approx(static_cast<float>(c)));
    }
    CHECK(g.cell_area() == Catch::Approx(256.0f)); // 16 m cells
    CHECK(g.extent() == Catch::Approx(1024.0f));
}

TEST_CASE("the reconstruction passes through the baked samples", "[generation][field]") {
    // Catmull-Rom is INTERPOLATING, not approximating. If it were not, the baked field would be a
    // suggestion rather than a result and no acceptance statistic computed on the grid would
    // describe the world the player stands on.
    const auto field = baked();
    const FieldSampler sampler{*field, Plane::Elevation};
    const FieldGeometry& g = field->geometry();
    for (std::int32_t cz : {8, 20, 33}) {
        for (std::int32_t cx : {5, 21, 40}) {
            const glm::vec2 w = g.to_world(cx, cz);
            CHECK(sampler.value_at(w.x, w.y) ==
                  Catch::Approx(field->plane(Plane::Elevation)[field->index(cx, cz)]).margin(1e-3));
        }
    }
}

TEST_CASE("height and slope are continuous across cell boundaries", "[generation][field]") {
    // THE test this reconstruction exists for -- and it is written as a RATIO against the same
    // measurement taken mid-cell, not as an absolute tolerance.
    //
    // A first version asserted "the height jump across a boundary is under 4 cm" and failed at
    // 12.6 cm. That was not a seam: over a 4 cm step, 12.6 cm is a slope of 3, and this terrain
    // genuinely reaches that. An absolute tolerance on a jump is really a statement about how
    // steep the world is allowed to be, which is not the property under test. **Comparing the
    // boundary against a mid-cell control removes the terrain's own steepness from the answer**,
    // and a bilinear reconstruction would still fail it -- its derivative jump at a boundary is
    // unbounded relative to the smooth interior.
    const auto field = baked();
    const HeightmapGenerator generator{1337, field};
    const FieldGeometry& g = field->geometry();

    constexpr float kStep = 0.02f;
    const auto jumps = [&](float atX, float z) {
        const float hL = generator.height_at(atX - kStep, z);
        const float hR = generator.height_at(atX + kStep, z);
        const glm::vec2 sL = generator.slope_at(atX - kStep, z);
        const glm::vec2 sR = generator.slope_at(atX + kStep, z);
        return glm::vec2{std::abs(hR - hL), glm::length(sR - sL)};
    };

    float boundaryHeight = 0.0f;
    float controlHeight = 0.0f;
    float boundarySlope = 0.0f;
    float controlSlope = 0.0f;
    int samples = 0;
    for (std::int32_t cx = 4; cx < 40; ++cx) {
        const float cellX = g.to_world(cx, 0).x;
        for (float z : {-300.0f, -64.0f, 0.0f, 128.0f}) {
            const glm::vec2 onBoundary = jumps(cellX + 0.5f * g.cell_size, z);
            const glm::vec2 midCell = jumps(cellX + 0.25f * g.cell_size, z);
            boundaryHeight += onBoundary.x;
            boundarySlope += onBoundary.y;
            controlHeight += midCell.x;
            controlSlope += midCell.y;
            ++samples;
        }
    }
    REQUIRE(samples > 0);
    const float heightRatio = boundaryHeight / std::max(controlHeight, 1e-9f);
    const float slopeRatio = boundarySlope / std::max(controlSlope, 1e-9f);
    INFO("height jump at a boundary vs mid-cell: " << heightRatio << "x");
    INFO("slope jump at a boundary vs mid-cell:  " << slopeRatio << "x");
    // A C1 reconstruction makes a cell boundary an ordinary point. 2x allows for the boundary
    // genuinely being where curvature is highest without allowing a seam.
    CHECK(heightRatio < 2.0f);
    CHECK(slopeRatio < 2.0f);
}

TEST_CASE("slope_at agrees with a difference of height_at", "[generation][field]") {
    // The analytic gradient and the thing every caller used to compute by hand must be the same
    // quantity, or replacing one with the other silently changes where grass grows. Compared at
    // the SAME 1 m epsilon slope_at's own detail term uses -- comparing across different epsilons
    // measures the surface's curvature, not the two estimators' agreement, which is how a first
    // version of this test came to fail at 0.9.
    const auto field = baked();
    const HeightmapGenerator generator{1337, field};
    constexpr float kEps = 1.0f;
    float worst = 0.0f;
    for (float z : {-200.0f, 0.0f, 96.0f}) {
        for (float x : {-180.0f, -8.0f, 64.0f, 210.0f}) {
            const glm::vec2 analytic = generator.slope_at(x, z);
            const glm::vec2 numeric{
                (generator.height_at(x + kEps, z) - generator.height_at(x - kEps, z)) / (2.0f * kEps),
                (generator.height_at(x, z + kEps) - generator.height_at(x, z - kEps)) / (2.0f * kEps)};
            worst = std::max(worst, glm::length(analytic - numeric));
        }
    }
    INFO("worst gradient disagreement: " << worst);
    // The remaining difference is the macro term: slope_at differentiates the reconstruction
    // exactly while the numeric version differences it over 2 m. Small, and bounded.
    CHECK(worst < 0.05f);
}

TEST_CASE("the field is deterministic in the seed", "[generation][field]") {
    // Rule 1 of this pass. Same seed, same field, byte for byte -- and a different seed must
    // actually differ, or the test would pass on a generator that ignored its seed.
    const auto a = baked(1337);
    const auto b = baked(1337);
    const auto c = baked(9001);
    const std::span<const float> pa = a->plane(Plane::Elevation);
    const std::span<const float> pb = b->plane(Plane::Elevation);
    const std::span<const float> pc = c->plane(Plane::Elevation);
    REQUIRE(pa.size() == pb.size());
    bool identical = true;
    bool differsFromOtherSeed = false;
    for (std::size_t i = 0; i < pa.size(); ++i) {
        identical = identical && pa[i] == pb[i];
        differsFromOtherSeed = differsFromOtherSeed || pa[i] != pc[i];
    }
    CHECK(identical);
    CHECK(differsFromOtherSeed);
}

TEST_CASE("the pipeline can be stopped after N stages", "[generation][field]") {
    // What isolates one stage's contribution for a capture or a statistic. With zero stages run
    // the field must still be a valid, zeroed field rather than uninitialised memory.
    TerrainField none{small_geometry()};
    world::generation::field::run_pipeline(none, MacroParams{}, 0);
    for (float v : none.plane(Plane::Elevation)) {
        REQUIRE(v == 0.0f);
    }
    CHECK_FALSE(world::generation::field::stages().empty());
}

TEST_CASE("a generator with no macro field still answers", "[generation][field]") {
    // The analytic-only constructor is what every existing caller uses, and it must keep working
    // unchanged -- this whole group is an architecture change, not a behaviour change.
    const HeightmapGenerator analytic{1337};
    CHECK(analytic.macro_field() == nullptr);
    CHECK(std::isfinite(analytic.height_at(12.0f, -34.0f)));
    CHECK(std::isfinite(analytic.slope_at(12.0f, -34.0f).x));
}
