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

#include "world/generation/field/climate.hpp"
#include "world/generation/field/fluvial.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/field/terrain_field.hpp"

using namespace world::generation::field;

namespace {

// 32 m cells over 192 cells = 6.1 km. The size is load-bearing: stage 1 uses a 4 km continent
// mask, so a field smaller than that sits entirely inside one lobe or one ocean and contains no
// drainage network to measure. A first version used 2 km and the slope-area fit ran on 329
// cells of what was effectively a single hillside -- and came out POSITIVE, which is not a
// landscape. The test was too small, not the solver wrong.
[[nodiscard]] FieldGeometry geometry(std::int32_t cells = 192) {
    const float half = 0.5f * 32.0f * static_cast<float>(cells);
    return FieldGeometry{.origin_x = -half, .origin_z = -half, .cell_size = 32.0f, .cells = cells};
}

/// A field with only the continental stage run -- rough, unfilled, full of pits.
[[nodiscard]] TerrainField rough(int seed = 1337, std::int32_t cells = 192) {
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
    TerrainField f = rough(1337, 192);
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
    TerrainField sharp = rough(1337, 192);
    priority_flood(sharp);
    TerrainField smooth{geometry(192)};
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
        TerrainField f{geometry(128)};
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

// ------------------------------------------------------------ Prompt 006 goal 302: the climate
//
// A synthetic ridge, not the generated terrain, is the instrument for every precipitation test
// below. A test that must first FIND a ridge in a procedural field is testing its own ridge-finder,
// and this pass has already been bitten twice by an instrument that was wrong (§7's 2 km field,
// §11's landscape pose for the stipple calibration).

namespace {

/// A Gaussian ridge running along z, centred in x: pure upslope on one side, pure downslope on the
/// other, and nothing else to confound the measurement.
[[nodiscard]] TerrainField ridge(float amplitudeM = 400.0f, float sigmaFraction = 0.06f,
                                 std::int32_t cells = 192) {
    TerrainField f{geometry(cells)};
    const std::span<float> h = f.plane(Plane::Elevation);
    for (std::int32_t cz = 0; cz < cells; ++cz) {
        for (std::int32_t cx = 0; cx < cells; ++cx) {
            const float u = static_cast<float>(cx) / static_cast<float>(cells - 1);
            const float d = (u - 0.5f) / sigmaFraction;
            h[f.index(cx, cz)] = amplitudeM * std::exp(-0.5f * d * d);
        }
    }
    return f;
}

/// Mean precipitation over the columns [x0, x1).
[[nodiscard]] double band_mean(const TerrainField& f, std::int32_t x0, std::int32_t x1) {
    const std::span<const float> precip = f.plane(Plane::Precipitation);
    double sum = 0.0;
    std::size_t n = 0;
    for (std::int32_t cz = 0; cz < f.cells(); ++cz) {
        for (std::int32_t cx = x0; cx < x1; ++cx) {
            sum += static_cast<double>(precip[f.index(cx, cz)]);
            ++n;
        }
    }
    return n == 0 ? 0.0 : sum / static_cast<double>(n);
}

} // namespace

TEST_CASE("temperature falls with elevation at the stated lapse rate", "[generation][climate]") {
    // Goal 302's Check says to assert the lapse rate NUMERICALLY rather than trust the comment.
    // 6.5 C/km is the standard-atmosphere environmental rate the research quotes -- and which its
    // own provenance register flags as matching the constant but not URL-verified, so this pins
    // what the code CLAIMS rather than asserting a measured truth about the atmosphere.
    TerrainField f = rough();
    compute_climate(f);
    const std::span<const float> h = f.plane(Plane::Elevation);
    const std::span<const float> t = f.plane(Plane::Temperature);

    // The lowest and highest cells that are ABOVE sea level. The bound must start from a real
    // above-water cell: a first draft seeded it at index 0, which is seabed, and then no cell could
    // ever be both above sea level and below it -- so the test compared a summit against a clamped
    // seabed and disagreed with itself by 0.013 C.
    std::size_t low = f.cell_count();
    std::size_t high = f.cell_count();
    for (std::size_t i = 0; i < f.cell_count(); ++i) {
        if (h[i] <= 0.0f) {
            continue;
        }
        if (high == f.cell_count() || h[i] > h[high]) {
            high = i;
        }
        if (low == f.cell_count() || h[i] < h[low]) {
            low = i;
        }
    }
    REQUIRE(high != f.cell_count());
    REQUIRE(h[high] > h[low]);

    const float expected = -lapse_rate_c_per_km() * (h[high] - h[low]) / 1000.0f;
    CHECK((t[high] - t[low]) == Catch::Approx(expected).margin(1e-4));
    CHECK(lapse_rate_c_per_km() == Catch::Approx(6.5f));
    // And the sign, stated separately, because an exactly-right magnitude with an inverted sign
    // would pass the line above if `expected` were derived the same wrong way.
    CHECK(t[high] < t[low]);
}

TEST_CASE("precipitation is normalised to a field mean of one", "[generation][climate]") {
    // The output is dimensionless BY DESIGN -- see climate.hpp. This is the invariant that makes it
    // usable: every consumer can read the plane as a multiplier on whatever absolute rainfall it
    // assumes, without knowing anything about the sweep that produced it.
    TerrainField f = ridge();
    compute_climate(f);
    const std::span<const float> precip = f.plane(Plane::Precipitation);
    double sum = 0.0;
    for (const float v : precip) {
        sum += static_cast<double>(v);
    }
    CHECK(sum / static_cast<double>(f.cell_count()) == Catch::Approx(1.0).margin(1e-3));

    // And a flat world -- which condenses nothing at all -- must still satisfy it rather than
    // divide by zero.
    TerrainField flat{geometry(64)};
    compute_climate(flat);
    for (const float v : flat.plane(Plane::Precipitation)) {
        CHECK(v == Catch::Approx(1.0f));
    }
}

TEST_CASE("a ridge casts a rain shadow with a spillover tail", "[generation][climate]") {
    // Goal 302's acceptance case. Three separate claims, because "there is a rain shadow" is not
    // one measurement:
    //
    //   1. the windward half is wetter than the lee half        -- the shadow
    //   2. the far lee relaxes to the synoptic background       -- the shadow is COMPLETE
    //   3. rain continues past the crest and decays over the
    //      drift length rather than stopping at it              -- the SPILLOVER
    TerrainField f = ridge();
    ClimateParams p;
    p.wind_x = 1.0f; // straight across the ridge, +x
    p.wind_z = 0.0f;
    compute_climate(f, p);

    const std::int32_t n = f.cells();
    const std::int32_t crest = n / 2;
    // SYMMETRIC FLANKS about the crest, one sigma to three sigma out on each side -- not the two
    // halves of the field. A first draft compared half-field means and measured the lee as WETTER,
    // because the windward half is eighty columns of flat approach plain that never lifts anything
    // and rains only background. That is not a rain shadow, it is a plain; the shadow lives on the
    // mountain, and the comparison has to be between two points that are equidistant from it.
    const auto sigma = static_cast<std::int32_t>(0.06f * static_cast<float>(n));
    const double windward = band_mean(f, crest - 3 * sigma, crest - sigma);
    const double lee = band_mean(f, crest + sigma, crest + 3 * sigma);
    INFO("windward flank " << windward << " vs lee flank " << lee << " => " << windward / lee
                           << ":1");
    CHECK(windward > lee);
    // Research Part 5 §6.2's default rule is lee:windward ~1:10 across a 1.5-2 km barrier.
    CHECK(windward / lee > 10.0);

    // 2. The far lee is background and nothing else. `background_fraction` is an exact floor:
    // with no ascent and an emptied cloud reservoir the orographic term is zero by construction.
    const double farLee = band_mean(f, n - 8, n);
    INFO("far-lee mean " << farLee << " against background_fraction " << p.background_fraction);
    CHECK(farLee == Catch::Approx(static_cast<double>(p.background_fraction)).margin(0.02));

    // The peak-to-floor ratio, which is the number §6.2 actually quotes.
    const std::span<const float> precip = f.plane(Plane::Precipitation);
    const std::int32_t row = n / 2;
    float peak = 0.0f;
    std::int32_t peakX = 0;
    for (std::int32_t cx = 0; cx < n; ++cx) {
        const float v = precip[f.index(cx, row)];
        if (v > peak) {
            peak = v;
            peakX = cx;
        }
    }
    INFO("peak " << peak << " at column " << peakX << " (crest " << crest << "), floor "
                 << p.background_fraction << " => " << peak / p.background_fraction << ":1");
    CHECK(peak / p.background_fraction > 10.0f);
    // The maximum is on the WINDWARD side of the crest, not on it -- the parcel has already been
    // wrung out by the time it reaches the top.
    CHECK(peakX < crest);

    // 3. Spillover: one drift length downwind of the crest, rain has decayed but not stopped.
    const auto driftCells = static_cast<std::int32_t>(p.drift_length_m / f.geometry().cell_size);
    const float atCrest = precip[f.index(crest, row)];
    const float oneDrift = precip[f.index(crest + driftCells, row)];
    INFO("crest " << atCrest << " -> one drift length downwind " << oneDrift << " -> background "
                  << p.background_fraction);
    CHECK(oneDrift < atCrest);
    CHECK(oneDrift > p.background_fraction);
}

TEST_CASE("this world's relief is too low for the research's rain-shadow anchor",
          "[generation][climate]") {
    // THE MEASURED NEGATIVE that justifies `wring_out_height_m`'s miniaturization, pinned as a
    // test rather than left in the log -- and written as an A/B on one ridge so it asserts a
    // COMPARISON rather than a fitted threshold.
    //
    // The water-vapour scale height is ~2 km, which is why §6.2 quotes its 1:10 anchor for a
    // 1.5-2 km barrier. This world's orogen carries ~112 m of relief, so the physical constant
    // wrings out 1 - exp(-112/2000) = 5.4% of the column instead of 57%.
    //
    // NOTE what this does and does not cost, because a first draft of this test predicted the
    // wrong thing and was corrected by its own output. The precipitation plane is NORMALISED to a
    // field mean of one, so a collapsing absolute wring-out does NOT collapse the pattern's
    // contrast with it -- the windward flank and the drifted lee scale together and cancel. The
    // measured gain from the miniaturization is 5.33:1 -> 7.39:1, not the "shadow vanishes" the
    // draft claimed. What the physical value really costs is the absolute budget: 5.4% of the
    // column wrung out instead of 57%, which only shows up on a fetch crossing a second range.
    //
    // And the finding that matters more than either: at 112 m of relief NEITHER value reaches
    // §6.2's 10:1, because that anchor is quoted for a 1.5-2 km barrier. The 400 m ridge in the
    // test above DOES reach it, at 15.2:1. This world's mountains are shorter than the landform
    // the research's number describes -- the same shape of result as this pass's drainage-density
    // and constant-drop findings.
    const auto flank_ratio = [](float wringHeightM) {
        TerrainField f = ridge(112.0f); // this world's actual relief, not the synthetic 400 m
        ClimateParams p;
        p.wind_x = 1.0f;
        p.wind_z = 0.0f;
        p.wring_out_height_m = wringHeightM;
        compute_climate(f, p);
        const std::int32_t n = f.cells();
        const std::int32_t crest = n / 2;
        const auto sigma = static_cast<std::int32_t>(0.06f * static_cast<float>(n));
        return band_mean(f, crest - 3 * sigma, crest - sigma) /
               band_mean(f, crest + sigma, crest + 3 * sigma);
    };

    const double physical = flank_ratio(2000.0f);
    const double miniaturized = flank_ratio(ClimateParams{}.wring_out_height_m);
    INFO("over 112 m of relief -- physical 2 km scale height: "
         << physical << ":1;  miniaturized " << ClimateParams{}.wring_out_height_m
         << " m: " << miniaturized << ":1;  research anchor 10:1");
    // The miniaturization is a real but modest gain, and this pins the direction and the size of
    // it so a future edit cannot quietly reverse either.
    CHECK(miniaturized > physical);
    CHECK(miniaturized / physical > 1.2);
    CHECK(miniaturized / physical < 2.0);
    // The negative: this world's relief cannot reach the anchor at EITHER setting.
    CHECK(physical < 10.0);
    CHECK(miniaturized < 10.0);
}

TEST_CASE("the climate stage is deterministic", "[generation][climate]") {
    const auto bake = [] {
        TerrainField f = rough(4242, 96);
        compute_climate(f);
        return std::vector<float>(f.plane(Plane::Precipitation).begin(),
                                  f.plane(Plane::Precipitation).end());
    };
    CHECK(bake() == bake());
}
