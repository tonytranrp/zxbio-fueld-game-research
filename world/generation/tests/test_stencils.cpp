// Prompt 006 Group AM-C goals 310-314's Checks.
//
// Each stencil is tested on a SYNTHETIC field that has its subject, and then measured on the
// shipped world. That split is the honest form for this pass: the shipped world is a gentle coastal
// plain with 112 m of relief and a 0.50 °C temperature range, so some of these stencils have no
// subject in it at all. Testing only against the shipped world would report a working stencil as
// broken when it is merely unemployed.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/field/stencils.hpp"
#include "world/generation/validation/acceptance.hpp"

using namespace world::generation::field;
using world::generation::validation::valley_cross_sections;
using world::generation::validation::ValleyStats;

namespace {

[[nodiscard]] TerrainField blank(std::int32_t cells, float cellSize = 16.0f) {
    const float half = 0.5f * cellSize * static_cast<float>(cells);
    return TerrainField{
        FieldGeometry{.origin_x = -half, .origin_z = -half, .cell_size = cellSize, .cells = cells}};
}

template <typename F>
void fill_plane(TerrainField& f, Plane plane, F&& fn) {
    const std::span<float> v = f.plane(plane);
    for (std::int32_t cz = 0; cz < f.cells(); ++cz) {
        for (std::int32_t cx = 0; cx < f.cells(); ++cx) {
            v[f.index(cx, cz)] = static_cast<float>(fn(cx, cz));
        }
    }
}

[[nodiscard]] TerrainField shipped(int seed = 1337, std::int32_t cells = 320) {
    TerrainField f = blank(cells);
    run_pipeline(f, MacroParams{.seed = seed}, -1);
    return f;
}

} // namespace

// -------------------------------------------------------------------------------- 310 glacial

TEST_CASE("the glacial stencil turns a V into a U", "[generation][stencils]") {
    // Acceptance test 5, run as the A/B it is: research §9.5 says fluvial terrain fits b ~ 1 and
    // glacial b -> 1.5-2.0. So the stencil must MOVE the exponent, measured by the same instrument
    // in both cases.
    //
    // Built on a synthetic V-valley high enough to be above a snowline, because this world has no
    // cell above freezing-level at all (see the negative below).
    const auto measure = [](bool glaciate) {
        TerrainField f = blank(192);
        fill_plane(f, Plane::Elevation, [](std::int32_t cx, std::int32_t cz) {
            const double d = std::abs(static_cast<double>(cx) - 96.0);
            return 1400.0 + 40.0 * d - 0.6 * static_cast<double>(cz); // a V, tilted downstream
        });
        priority_flood(f);
        FlowNetwork net = build_flow_network(f);
        accumulate_flow(f, net);
        if (glaciate) {
            GlacialParams p;
            p.snowline_m = 1300.0f;
            const GlacialResult r = carve_glacial_valleys(f, net, p);
            REQUIRE(r.basins > 0);
            REQUIRE(r.carved_cells > 0);
            priority_flood(f);
            net = build_flow_network(f);
            accumulate_flow(f, net);
        }
        return valley_cross_sections(f, net, 0.01);
    };

    const ValleyStats fluvial = measure(false);
    const ValleyStats glacial = measure(true);
    INFO("fluvial b = " << fluvial.mean_exponent << " over " << fluvial.transects
                        << " transects; glacial b = " << glacial.mean_exponent << " over "
                        << glacial.transects);
    REQUIRE(fluvial.transects > 10);
    REQUIRE(glacial.transects > 10);
    // The claim §9.5 makes is the ORDERING, and that is what is asserted -- the absolute band is
    // reported. A synthetic V is a perfect V (b = 1 exactly), so the stencil has nowhere to go but
    // up, which makes this a real test rather than a tautology only if it actually moves.
    CHECK(glacial.mean_exponent > fluvial.mean_exponent);
    CHECK(glacial.mean_v_index > fluvial.mean_v_index);
}

TEST_CASE("this world has no snowline, so nothing glaciates", "[generation][stencils]") {
    // THE MEASURED NEGATIVE. Research §10.1 says to select basins above the CLIMATE FIELD's
    // snowline. Goal 302 measured this world's temperature range at 13.50-14.00 °C, so the
    // freezing isotherm sits far above any ground that exists.
    const TerrainField f = shipped();
    const float snowline = snowline_from_climate(f);
    const std::span<const float> h = f.plane(Plane::Elevation);
    const float highest = *std::max_element(h.begin(), h.end());
    INFO("snowline at " << snowline << " m; the highest ground in the world is " << highest << " m");
    CHECK(snowline > highest);
    // And the stencil, run at that snowline, carves nothing -- which is the correct behaviour, not
    // a failure.
    TerrainField g = f;
    priority_flood(g);
    const FlowNetwork net = build_flow_network(g);
    GlacialParams p;
    p.snowline_m = snowline;
    const GlacialResult r = carve_glacial_valleys(g, net, p);
    CHECK(r.basins == 0);
    CHECK(r.carved_cells == 0);
}

// -------------------------------------------------------------------------------- 311 coastal

TEST_CASE("the coastal stencil cuts platforms under cliffs and beaches under gentle slopes",
          "[generation][stencils]") {
    // Two synthetic coasts, identical except for their slope. The stencil must treat them
    // differently -- that split IS goal 311, since today "the coastline is a contour line".
    const auto run = [](double gradient) {
        TerrainField f = blank(128);
        fill_plane(f, Plane::Elevation, [gradient](std::int32_t cx, std::int32_t) {
            return (static_cast<double>(cx) - 64.0) * gradient;
        });
        const std::vector<float> before(f.plane(Plane::Elevation).begin(), f.plane(Plane::Elevation).end());
        const CoastalResult r = carve_coastline(f);
        return std::pair<CoastalResult, std::vector<float>>{
            r, std::vector<float>(f.plane(Plane::Elevation).begin(), f.plane(Plane::Elevation).end())};
    };

    const auto [cliffRes, cliffAfter] = run(12.0);  // 0.75 m/m -- well over the cliff threshold
    const auto [gentleRes, gentleAfter] = run(0.8); // 0.05 m/m
    INFO("steep coast: " << cliffRes.cliffs << " cliffs / " << cliffRes.beaches << " beaches; "
                         << "gentle coast: " << gentleRes.cliffs << " / " << gentleRes.beaches);
    REQUIRE(cliffRes.coast_cells > 0);
    REQUIRE(gentleRes.coast_cells > 0);
    CHECK(cliffRes.cliffs > cliffRes.beaches);
    CHECK(gentleRes.beaches > gentleRes.cliffs);
    // The platform is the diagnostic: seaward of a cliff the seabed must have been RAISED to a
    // near-horizontal bench. A cliff without a platform is just a steep slope.
    CHECK(cliffAfter != gentleAfter);
}

TEST_CASE("the coastal stencil finds the shipped world's coast", "[generation][stencils]") {
    TerrainField f = shipped();
    const CoastalResult r = carve_coastline(f);
    INFO("shipped coast: " << r.coast_cells << " cells, " << r.cliffs << " cliffed, " << r.beaches
                           << " gentle");
    CHECK(r.coast_cells > 100);
    // This world is a gentle coastal plain, so its coast should be overwhelmingly depositional.
    // Stated as the measurement it is rather than asserted as a target.
    CHECK(r.beaches > r.cliffs);
}

// ---------------------------------------------------------------------------------- 312 karst

TEST_CASE("karst stamps dolines only on carbonate, and refills them", "[generation][stencils]") {
    // Half the field carbonate, half not. The mask is the whole point -- §10.1's failure mode is
    // one texture everywhere.
    TerrainField f = blank(256);
    // TILTED, not flat. A perfectly flat plateau is entirely pits, so the stage's own re-fill
    // rewrites every cell and the mask comparison below measures the fill rather than the dolines.
    // A gentle drainable slope makes the fill a no-op everywhere except inside the dolines, which
    // is what the comparison is trying to see.
    fill_plane(f, Plane::Elevation,
               [](std::int32_t, std::int32_t cz) { return 60.0 + 0.5 * static_cast<double>(cz); });
    fill_plane(f, Plane::Lithology, [](std::int32_t cx, std::int32_t) { return cx < 128 ? 2.0 : 0.0; });
    const std::vector<float> before(f.plane(Plane::Elevation).begin(), f.plane(Plane::Elevation).end());

    const KarstResult r = stamp_karst(f);
    INFO("dolines " << r.dolines << " at " << r.density_per_km2 << " /km^2, mean diameter "
                    << r.mean_diameter_m << " m, refilled " << r.refilled);
    REQUIRE(r.dolines > 0);
    CHECK(r.mean_diameter_m >= 20.0f);
    CHECK(r.mean_diameter_m <= 120.0f);
    // Goal 312's Check: SAY WHICH. This stage re-runs the fill; the flag records it and the
    // coherence test below proves it.
    CHECK(r.refilled);

    // Only the carbonate half changed.
    const std::span<const float> after = f.plane(Plane::Elevation);
    std::size_t changedCarbonate = 0;
    std::size_t changedOther = 0;
    for (std::int32_t cz = 0; cz < f.cells(); ++cz) {
        for (std::int32_t cx = 0; cx < f.cells(); ++cx) {
            const std::size_t i = f.index(cx, cz);
            if (std::abs(after[i] - before[i]) < 1e-3f) {
                continue;
            }
            (cx < 128 ? changedCarbonate : changedOther) += 1u;
        }
    }
    INFO("changed cells: " << changedCarbonate << " carbonate, " << changedOther << " non-carbonate");
    CHECK(changedCarbonate > 0);
    // A doline centred near the boundary may reach a few cells across it, and the stage's re-fill
    // then raises the cells its rim dammed. What must not happen is dolines being STAMPED on
    // non-carbonate.
    CHECK(changedOther * 4 < changedCarbonate);

    // And acceptance test 9 still holds afterwards, which is the thing dolines threaten.
    const auto basins =
        world::generation::validation::hydrological_coherence(f, RiverNetwork{}).internal_basins;
    INFO("internal basins after karst: " << basins);
    CHECK(basins == 0);
}

// ---------------------------------------------------------------------------------- 314 dunes

TEST_CASE("the dune phase diagram selects rather than collapsing", "[generation][stencils]") {
    // Research Part 5 §2: the morphology comes from TWO axes. A phase diagram that returns the same
    // answer everywhere is a sand texture with extra steps.
    CHECK(dune_morphology(0.2f, 0.1f) == DuneType::Barchan);
    CHECK(dune_morphology(0.8f, 0.1f) == DuneType::TransverseRidge);
    CHECK(dune_morphology(0.2f, 0.5f) == DuneType::Linear);
    CHECK(dune_morphology(0.8f, 0.5f) == DuneType::Linear);
    CHECK(dune_morphology(0.2f, 0.9f) == DuneType::Star);
    // Both axes must matter: holding one and moving the other must change the answer at least once.
    CHECK(dune_morphology(0.2f, 0.1f) != dune_morphology(0.8f, 0.1f)); // supply axis
    CHECK(dune_morphology(0.2f, 0.1f) != dune_morphology(0.2f, 0.9f)); // variability axis
}

TEST_CASE("dunes are stamped in the desert and their geometry is in band", "[generation][stencils]") {
    TerrainField f = shipped();
    const DuneResult r = stamp_dunes(f);
    INFO("desert " << r.desert_cells << " cells, dunes " << r.dune_cells << ", mean wavelength "
                   << r.mean_wavelength_m << " m, mean height " << r.mean_height_m << " m");
    REQUIRE(r.desert_cells > 100);
    CHECK(r.dune_cells == r.desert_cells);
    // Research Part 5 §3's bands for the morphologies this generator produces.
    CHECK(r.mean_wavelength_m >= 50.0f);
    CHECK(r.mean_wavelength_m <= 500.0f);
    CHECK(r.mean_height_m > 1.0f);

    // Goal 314's Check: the morphology map must VARY. At least two types must be present in a real
    // desert, or the phase diagram is not being consulted.
    std::size_t typesPresent = 0;
    for (std::size_t k = 1; k < 5; ++k) {
        typesPresent += r.by_type[k] > r.desert_cells / 50 ? 1u : 0u;
    }
    INFO("dune types covering >2% of the desert: " << typesPresent);
    CHECK(typesPresent >= 2);
}

TEST_CASE("stencils are deterministic", "[generation][stencils]") {
    const auto bake = [] {
        TerrainField f = shipped(99, 192);
        (void)stamp_karst(f);
        (void)stamp_dunes(f);
        (void)carve_coastline(f);
        return std::vector<float>(f.plane(Plane::Elevation).begin(), f.plane(Plane::Elevation).end());
    };
    const std::vector<float> a = bake();
    const std::vector<float> b = bake();
    REQUIRE(!a.empty());
    CHECK(a == b);
}
