// Prompt 006 goal 316's Checks: per-biome stem densities inside Part 6's measured bands, ecotone
// behaviour, and determinism.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <vector>

#include "world/generation/field/biome.hpp"
#include "world/generation/field/macro_pipeline.hpp"

using namespace world::generation::field;

namespace {

[[nodiscard]] TerrainField baked(int seed = 1337, std::int32_t cells = 320) {
    const float half = 0.5f * 16.0f * static_cast<float>(cells);
    TerrainField f{FieldGeometry{.origin_x = -half, .origin_z = -half, .cell_size = 16.0f, .cells = cells}};
    run_pipeline(f, MacroParams{.seed = seed}, -1);
    return f;
}

[[nodiscard]] std::array<std::size_t, static_cast<std::size_t>(Biome::Count)>
histogram(const TerrainField& f) {
    std::array<std::size_t, static_cast<std::size_t>(Biome::Count)> counts{};
    for (const float v : f.plane(Plane::Biome)) {
        const auto b = static_cast<std::size_t>(std::lround(v));
        if (b < counts.size()) {
            ++counts[b];
        }
    }
    return counts;
}

} // namespace

TEST_CASE("every biome's stem density sits inside its own research band", "[generation][biome]") {
    // Acceptance test 10. The band that MATTERS is temperate forest's 400-700 (Part 7 §9.10), but
    // asserting only that one would leave seven numbers unchecked, and a table of citations nobody
    // tests is a table of decoration.
    for (const BiomeDefinition& d : biome_table()) {
        INFO("biome " << d.name << " targets " << d.stems_per_hectare << " against [" << d.band_lo << ", "
                      << d.band_hi << "] -- " << d.citation);
        CHECK(d.stems_per_hectare >= d.band_lo);
        CHECK(d.stems_per_hectare <= d.band_hi);
        CHECK_FALSE(d.citation.empty());
    }
    // And the one §9.10 is actually about.
    CHECK(stems_per_hectare(Biome::TemperateForest) >= 400.0f);
    CHECK(stems_per_hectare(Biome::TemperateForest) <= 700.0f);
}

TEST_CASE("the classifier produces a landscape, not one biome", "[generation][biome]") {
    const TerrainField f = baked();
    const auto counts = histogram(f);

    std::size_t populated = 0;
    for (const std::size_t n : counts) {
        populated += n > f.cell_count() / 200 ? 1u : 0u; // over half a percent of the field
    }
    INFO("biomes covering >0.5% of the field: " << populated);
    CHECK(populated >= 5);

    // Ocean and land must both be substantial -- a classifier that put the whole field in one class
    // would still pass the count above if the classes were tiny slivers.
    const std::size_t ocean = counts[static_cast<std::size_t>(Biome::Ocean)];
    INFO("ocean covers " << 100.0 * static_cast<double>(ocean) / static_cast<double>(f.cell_count()) << "%");
    CHECK(ocean > f.cell_count() / 10);
    CHECK(ocean < f.cell_count() * 9 / 10);
}

TEST_CASE("biomes follow the moisture axis, and forest is wetter than desert", "[generation][biome]") {
    // The substantive claim: this is a CLASSIFICATION of the climate field, not a decoration on
    // top of it. If mean precipitation under the forest were not above mean precipitation under
    // the desert, the classifier would be reading something else.
    const TerrainField f = baked();
    const std::span<const float> precip = f.plane(Plane::Precipitation);
    const std::span<const float> biome = f.plane(Plane::Biome);
    double forest = 0.0;
    double desert = 0.0;
    std::size_t nf = 0;
    std::size_t nd = 0;
    for (std::size_t i = 0; i < f.cell_count(); ++i) {
        const auto b = static_cast<std::size_t>(std::lround(biome[i]));
        if (b == static_cast<std::size_t>(Biome::TemperateForest)) {
            forest += precip[i];
            ++nf;
        } else if (b == static_cast<std::size_t>(Biome::Desert)) {
            desert += precip[i];
            ++nd;
        }
    }
    REQUIRE(nf > 100);
    REQUIRE(nd > 100);
    forest /= static_cast<double>(nf);
    desert /= static_cast<double>(nd);
    INFO("mean precipitation: forest " << forest << ", desert " << desert);
    CHECK(forest > desert * 2.0);
}

TEST_CASE("the treeline is a sharp boundary and the moisture ecotones are graded", "[generation][biome]") {
    // Research Part 7 §6.4: "edge sharpness = f(cause)". Feedback-maintained boundaries snap;
    // climate gradients grade. This asserts the DIFFERENCE between the two, which is the part of
    // §6.4 that is a claim rather than a description.
    //
    // Measured as boundary raggedness: along a climate ecotone the dithering puts isolated cells of
    // each class on the other side, so a cell's 4-neighbourhood frequently disagrees with it. At an
    // elevation-threshold boundary it never does.
    //
    // The elevation-threshold class here is BEACH, not the alpine treeline, and that substitution
    // is itself a finding: on a 5.1 km field NO CELL REACHES the 60 m alpine base, so the treeline
    // does not exist to measure. It appears at 0.5% of the shipped 8 km field and nowhere below
    // that -- one more consequence of this world's 112 m of relief. Beach is the same KIND of
    // boundary (a pure elevation threshold with no dithering) and exists wherever there is a coast.
    const TerrainField f = baked();
    const std::span<const float> biome = f.plane(Plane::Biome);
    const auto classOf = [&](std::int32_t cx, std::int32_t cz) {
        return static_cast<std::size_t>(std::lround(biome[f.index(cx, cz)]));
    };

    std::size_t alpineEdge = 0;
    std::size_t alpineMixed = 0;
    std::size_t moistureEdge = 0;
    std::size_t moistureMixed = 0;
    for (std::int32_t cz = 1; cz + 1 < f.cells(); ++cz) {
        for (std::int32_t cx = 1; cx + 1 < f.cells(); ++cx) {
            const std::size_t c = classOf(cx, cz);
            const bool isAlpine = c == static_cast<std::size_t>(Biome::Beach);
            const bool isMoisture = c == static_cast<std::size_t>(Biome::Grassland) ||
                                    c == static_cast<std::size_t>(Biome::Shrubland);
            if (!isAlpine && !isMoisture) {
                continue;
            }
            std::size_t different = 0;
            different += classOf(cx + 1, cz) != c ? 1u : 0u;
            different += classOf(cx - 1, cz) != c ? 1u : 0u;
            different += classOf(cx, cz + 1) != c ? 1u : 0u;
            different += classOf(cx, cz - 1) != c ? 1u : 0u;
            if (isAlpine) {
                ++alpineEdge;
                alpineMixed += different >= 3 ? 1u : 0u;
            } else {
                ++moistureEdge;
                moistureMixed += different >= 3 ? 1u : 0u;
            }
        }
    }
    REQUIRE(alpineEdge > 50);
    REQUIRE(moistureEdge > 500);
    const double alpineRatio = static_cast<double>(alpineMixed) / static_cast<double>(alpineEdge);
    const double moistureRatio = static_cast<double>(moistureMixed) / static_cast<double>(moistureEdge);
    INFO("mixed-neighbourhood fraction: elevation-threshold (beach) " << alpineRatio << ", moisture "
                                                                      << moistureRatio);
    CHECK(moistureRatio > alpineRatio);
}

TEST_CASE("biome classification is deterministic", "[generation][biome]") {
    const auto bake = [] {
        const TerrainField f = baked(4242, 192);
        return std::vector<float>(f.plane(Plane::Biome).begin(), f.plane(Plane::Biome).end());
    };
    const std::vector<float> a = bake();
    const std::vector<float> b = bake();
    REQUIRE(!a.empty());
    CHECK(a == b);
}
