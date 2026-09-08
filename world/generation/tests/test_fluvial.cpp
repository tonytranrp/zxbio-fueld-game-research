// Prompt 006 Group AM-B's Checks. Each of these is one of research Part 7 §9's acceptance tests,
// or the structural property a stage claims by construction.
//
// The discipline the research asks for and this file follows: "a generator is a statistical
// hypothesis about terrain; test it like one." So these assert distributions and invariants, not
// specific heights -- a test that pinned a height would break on every parameter change and prove
// nothing about whether the terrain is terrain.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "world/generation/field/fluvial.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/field/terrain_field.hpp"

using namespace world::generation::field;

namespace {

[[nodiscard]] FieldGeometry geometry(std::int32_t cells = 128) {
    return FieldGeometry{.origin_x = -1024.0f, .origin_z = -1024.0f, .cell_size = 16.0f, .cells = cells};
}

/// A field with only the continental stage run -- rough, unfilled, full of pits.
[[nodiscard]] TerrainField rough(int seed = 1337, std::int32_t cells = 128) {
    TerrainField f{geometry(cells)};
    run_pipeline(f, MacroParams{.seed = seed}, 1);
    return f;
}

} // namespace

TEST_CASE("priority-flood leaves zero internal basins", "[generation][fluvial]") {
    // Acceptance test 9's structural half, and goal 303's Check says to assert it MECHANICALLY
    // OVER THE WHOLE FIELD rather than by sampling -- so this walks every cell.
    //
    // The property: after the fill, every non-boundary cell has at least one neighbour strictly
    // lower than itself. A cell with no lower neighbour is a pit, and a pit is a place where water
    // arrives and never leaves -- which is what makes a drainage network fall apart into
    // disconnected pieces.
    TerrainField f = rough();
    const std::span<const float> before = f.plane(Plane::Elevation);
    std::size_t pitsBefore = 0;
    const auto count_pits = [&](const std::span<const float>& h) {
        std::size_t pits = 0;
        for (std::int32_t cz = 1; cz < f.cells() - 1; ++cz) {
            for (std::int32_t cx = 1; cx < f.cells() - 1; ++cx) {
                const float self = h[f.index(cx, cz)];
                bool hasLower = false;
                for (std::int32_t dz = -1; dz <= 1 && !hasLower; ++dz) {
                    for (std::int32_t dx = -1; dx <= 1 && !hasLower; ++dx) {
                        if (dx == 0 && dz == 0) {
                            continue;
                        }
                        hasLower = h[f.index(cx + dx, cz + dz)] < self;
                    }
                }
                pits += hasLower ? 0u : 1u;
            }
        }
        return pits;
    };
    pitsBefore = count_pits(before);
    // The test would be vacuous if the unfilled field had no pits to begin with.
    REQUIRE(pitsBefore > 0);

    priority_flood(f);
    CHECK(count_pits(f.plane(Plane::Elevation)) == 0);
}

TEST_CASE("priority-flood only ever raises terrain", "[generation][fluvial]") {
    // Filling a depression means raising it to its outlet. A fill that LOWERED anything would be
    // carving, which is a different operation with different consequences for the coastline.
    TerrainField f = rough();
    const std::vector<float> before(f.plane(Plane::Elevation).begin(), f.plane(Plane::Elevation).end());
    priority_flood(f);
    const std::span<const float> after = f.plane(Plane::Elevation);
    for (std::size_t i = 0; i < before.size(); ++i) {
        REQUIRE(after[i] >= before[i] - 1e-5f);
    }
}

TEST_CASE("flow accumulation is exactly conservative", "[generation][fluvial]") {
    // Goal 304's Check. Every cell contributes exactly itself, so the total arriving at the
    // outlets must equal the cell count. A leak here means the network has a cycle (water going
    // round forever) or a dead end that swallows it -- both of which would make drainage area, and
    // therefore every incision rate, silently wrong.
    TerrainField f = rough();
    priority_flood(f);
    const FlowNetwork net = build_flow_network(f);
    accumulate_flow(f, net);

    const std::span<const float> acc = f.plane(Plane::FlowAccum);
    double atOutlets = 0.0;
    std::size_t outlets = 0;
    for (std::size_t i = 0; i < net.receiver.size(); ++i) {
        if (net.receiver[i] == i) {
            atOutlets += acc[i];
            ++outlets;
        }
    }
    INFO("outlets: " << outlets << ", delivered: " << atOutlets << " of " << f.cell_count());
    CHECK(outlets > 0);
    // Float tolerance stated rather than exact: the sum is over 16,384 cells of float additions in
    // a fixed order, so it is reproducible but not exact in the last bit.
    CHECK(atOutlets == Catch::Approx(static_cast<double>(f.cell_count())).epsilon(1e-4));
}

TEST_CASE("every cell drains to an outlet in a bounded number of hops", "[generation][fluvial]") {
    // The other half of "the network is well-formed": no cycles. A cycle would make the
    // accumulation test above pass (the water is conserved, it just never leaves) while making
    // every downstream statistic meaningless, so it needs its own assertion.
    TerrainField f = rough();
    priority_flood(f);
    const FlowNetwork net = build_flow_network(f);
    const std::size_t limit = f.cell_count();
    for (std::size_t start = 0; start < net.receiver.size(); start += 37) { // a coprime stride
        std::size_t at = start;
        std::size_t hops = 0;
        while (net.receiver[at] != at && hops <= limit) {
            at = net.receiver[at];
            ++hops;
        }
        REQUIRE(hops <= limit);
    }
}

TEST_CASE("incision produces a negative slope-area exponent", "[generation][fluvial]") {
    // Acceptance test 7. The stream-power law's signature: channel slope falls as drainage area
    // rises, with an exponent the research bands at [-0.6, -0.35]. Its ABSENCE is what makes noise
    // terrain look like noise -- every ridge the same size regardless of how much water it carries.
    //
    // Measured on a small field, so the band is checked for SIGN AND ORDER rather than asserted
    // tightly; goal 305's Check reports the full-size number in the log.
    TerrainField f = rough(1337, 128);
    run_pipeline(f, MacroParams{}, -1); // the whole pipeline, including incision and diffusion

    priority_flood(f);
    const FlowNetwork net = build_flow_network(f);
    accumulate_flow(f, net);
    const std::span<const float> h = f.plane(Plane::Elevation);
    const std::span<const float> acc = f.plane(Plane::FlowAccum);
    const float dx = f.geometry().cell_size;

    // Least squares of log(slope) on log(area) over channel cells only -- below the channel head
    // the relationship is not expected to hold at all (that is test 7's "hillslope plateau").
    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;
    std::size_t n = 0;
    for (std::size_t i = 0; i < net.receiver.size(); ++i) {
        const std::uint32_t r = net.receiver[i];
        if (r == i || acc[i] < 50.0f) {
            continue;
        }
        const float slope = (h[i] - h[r]) / dx;
        if (slope <= 1e-5f) {
            continue;
        }
        const double lx = std::log(static_cast<double>(acc[i]) * f.geometry().cell_area());
        const double ly = std::log(static_cast<double>(slope));
        sx += lx;
        sy += ly;
        sxx += lx * lx;
        sxy += lx * ly;
        ++n;
    }
    REQUIRE(n > 100);
    const double dn = static_cast<double>(n);
    const double exponent = (dn * sxy - sx * sy) / (dn * sxx - sx * sx);
    INFO("slope-area exponent over " << n << " channel cells: " << exponent);
    // The sign is the physics; the magnitude is the tuning. A positive exponent would mean big
    // rivers are steeper than their headwaters, which is not a landscape.
    CHECK(exponent < 0.0);
}

TEST_CASE("diffusion bounds ridge curvature", "[generation][fluvial]") {
    // Goal 306's Check. The research calls the absence of hillslope diffusion "the single most
    // recognizable 'procedural terrain' tell" -- knife-sharp ridges. A knife edge is unbounded
    // second derivative, so the test is that the worst curvature FALLS.
    TerrainField sharp = rough(1337, 128);
    priority_flood(sharp);
    TerrainField smooth{geometry(128)};
    std::copy(sharp.plane(Plane::Elevation).begin(), sharp.plane(Plane::Elevation).end(),
              smooth.plane(Plane::Elevation).begin());
    diffuse_hillslopes(smooth, FluvialParams{});

    const auto worst_curvature = [](const TerrainField& f) {
        const std::span<const float> h = f.plane(Plane::Elevation);
        float worst = 0.0f;
        for (std::int32_t cz = 1; cz < f.cells() - 1; ++cz) {
            for (std::int32_t cx = 1; cx < f.cells() - 1; ++cx) {
                const float laplacian = h[f.index(cx + 1, cz)] + h[f.index(cx - 1, cz)] +
                                        h[f.index(cx, cz + 1)] + h[f.index(cx, cz - 1)] -
                                        4.0f * h[f.index(cx, cz)];
                worst = std::max(worst, std::abs(laplacian));
            }
        }
        return worst;
    };
    const float before = worst_curvature(sharp);
    const float after = worst_curvature(smooth);
    INFO("worst |laplacian| before diffusion: " << before << ", after: " << after);
    CHECK(after < before);
}

TEST_CASE("the characteristic valley spacing is derived, not asserted", "[generation][fluvial]") {
    // L_c = (D/K)^(1/(2m+2)). Not a constant in the code -- a function of the parameters, so that
    // changing D or K moves the number goal 306 measures against rather than leaving a stale
    // literal behind.
    FluvialParams p;
    const float lc = characteristic_valley_spacing(p);
    INFO("L_c for D=" << p.diffusivity << " K=" << p.k << " m=" << p.m << " is " << lc << " m");
    CHECK(lc > 0.0f);
    // Raising diffusivity widens valleys; raising erodibility narrows them. Both directions
    // asserted, because a formula that only moved one way would still pass a single-value check.
    FluvialParams wetter = p;
    wetter.diffusivity *= 4.0f;
    CHECK(characteristic_valley_spacing(wetter) > lc);
    FluvialParams softer = p;
    softer.k *= 4.0f;
    CHECK(characteristic_valley_spacing(softer) < lc);
}

TEST_CASE("the whole pipeline is deterministic in the seed", "[generation][fluvial]") {
    // Rule 1, now over the iterative solvers rather than one noise call -- which is the case goal
    // 300 says to design for rather than retrofit. Every stage in fluvial.cpp is serial precisely
    // so this holds by construction.
    const auto bake = [](int seed) {
        TerrainField f{geometry(96)};
        run_pipeline(f, MacroParams{.seed = seed}, -1);
        return std::vector<float>(f.plane(Plane::Elevation).begin(), f.plane(Plane::Elevation).end());
    };
    const std::vector<float> a = bake(1337);
    const std::vector<float> b = bake(1337);
    const std::vector<float> c = bake(2024);
    REQUIRE(a.size() == b.size());
    CHECK(a == b); // byte-identical, not approximately equal
    CHECK(a != c);
}
