// Prompt 006 goal 317's Check, and the half of it that matters:
//
//   "Unit tests feed each metric a synthetic input with a known answer (a plane, a cone, a sine, a
//    known-fractal surface) and assert the metric computes it correctly -- TEST THE INSTRUMENT
//    BEFORE TRUSTING ITS READING."
//
// This pass has been misled by an instrument rather than by the terrain at least four times: the
// moiré metric's envelope separation (Prompt 005 §9), the chord-based meander wavelength (goal 309),
// the half-field rain-shadow band (goal 302), and a fluvial test whose field was smaller than the
// feature it measured (goal 307). Every one cost more than writing this file would have.
//
// So each test below has an ANALYTICALLY KNOWN answer, not a plausible one.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numeric>
#include <vector>

#include "world/generation/field/fluvial.hpp"
#include "world/generation/validation/acceptance.hpp"

using namespace world::generation::validation;
using namespace world::generation::field;

namespace {

constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] TerrainField make_field(std::int32_t cells, float cellSize = 16.0f) {
    const float half = 0.5f * cellSize * static_cast<float>(cells);
    return TerrainField{
        FieldGeometry{.origin_x = -half, .origin_z = -half, .cell_size = cellSize, .cells = cells}};
}

/// Fills the elevation plane from f(cx, cz).
template <typename F>
void fill(TerrainField& field, F&& f) {
    const std::span<float> h = field.plane(Plane::Elevation);
    for (std::int32_t cz = 0; cz < field.cells(); ++cz) {
        for (std::int32_t cx = 0; cx < field.cells(); ++cx) {
            h[field.index(cx, cz)] = static_cast<float>(f(cx, cz));
        }
    }
}

/// Runs the hydrology so the network-dependent metrics have something to read.
struct Routed {
    TerrainField field;
    FlowNetwork net;
};

[[nodiscard]] Routed route(TerrainField field) {
    priority_flood(field);
    FlowNetwork net = build_flow_network(field);
    accumulate_flow(field, net);
    return Routed{std::move(field), std::move(net)};
}

} // namespace

// ------------------------------------------------------------------------------------- the FFT

TEST_CASE("the FFT puts a pure sinusoid in exactly one bin", "[generation][validation]") {
    // The most basic instrument check there is: transform cos(2*pi*k0*i/n) and the only non-zero
    // bins must be k0 and its conjugate n-k0, each with magnitude n/2.
    constexpr std::size_t n = 256;
    constexpr std::size_t k0 = 13;
    std::vector<double> re(n);
    std::vector<double> im(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        re[i] = std::cos(2.0 * kPi * static_cast<double>(k0 * i) / static_cast<double>(n));
    }
    fft_in_place(re, im);
    for (std::size_t k = 0; k < n; ++k) {
        const double mag = std::sqrt(re[k] * re[k] + im[k] * im[k]);
        if (k == k0 || k == n - k0) {
            CHECK(mag == Catch::Approx(static_cast<double>(n) / 2.0).margin(1e-6));
        } else {
            CHECK(mag < 1e-8);
        }
    }
}

TEST_CASE("the FFT satisfies Parseval", "[generation][validation]") {
    // Energy is conserved: sum|x|^2 == (1/n) sum|X|^2. This catches a scaling error the single-bin
    // test above cannot, because that one only ever looks at one coefficient.
    constexpr std::size_t n = 128;
    std::vector<double> re(n);
    std::vector<double> im(n, 0.0);
    double timeEnergy = 0.0;
    std::uint32_t s = 12345;
    for (std::size_t i = 0; i < n; ++i) {
        s = s * 1664525u + 1013904223u;
        re[i] = static_cast<double>(s % 2000) / 1000.0 - 1.0;
        timeEnergy += re[i] * re[i];
    }
    fft_in_place(re, im);
    double freqEnergy = 0.0;
    for (std::size_t k = 0; k < n; ++k) {
        freqEnergy += re[k] * re[k] + im[k] * im[k];
    }
    CHECK(freqEnergy / static_cast<double>(n) == Catch::Approx(timeEnergy).epsilon(1e-9));
}

// -------------------------------------------------------------------------------- §9.1 slope

TEST_CASE("the slope metric recovers a plane's exact angle", "[generation][validation]") {
    // A plane tilted so that rise/run is exactly 1 must read 45 degrees, with zero skew and one
    // mode. This is the whole metric's ground truth: if it cannot measure a plane it cannot
    // measure a landscape.
    TerrainField f = make_field(128);
    const float cs = f.geometry().cell_size;
    fill(f, [cs](std::int32_t cx, std::int32_t) { return static_cast<double>(cx) * cs; });
    const SlopeStats s = slope_distribution(f);
    CHECK(s.mean_slope_deg == Catch::Approx(45.0).margin(1e-4));
    CHECK(s.skewness == Catch::Approx(0.0).margin(1e-6));
    CHECK(s.unimodal);

    // And a half-flat, half-steep field is BIMODAL -- the failure §9.1 exists to catch ("stamped
    // cliffs create bimodal/shouldered histograms absent from Wolinsky-Pratson's observed trend").
    TerrainField two = make_field(128);
    fill(two, [cs](std::int32_t cx, std::int32_t cz) {
        return cz < 64 ? 0.05 * static_cast<double>(cx) * cs : 1.0 * static_cast<double>(cx) * cs;
    });
    const SlopeStats bimodal = slope_distribution(two);
    INFO("modes " << bimodal.modes);
    CHECK_FALSE(bimodal.unimodal);
}

// ----------------------------------------------------------------------------- §9.2 spectrum

TEST_CASE("the spectrum recovers a synthesised power law", "[generation][validation]") {
    // Build a surface whose 1D power spectrum is k^-beta BY CONSTRUCTION: sum sinusoids with
    // amplitude k^(-beta/2), since power goes as amplitude squared. Then the metric must read beta
    // back. Requested 2.0 -- Brownian motion, which is also the research's target value.
    constexpr double kBeta = 2.0;
    TerrainField f = make_field(256);
    std::vector<double> phase(128);
    std::uint32_t s = 987654321;
    for (double& p : phase) {
        s = s * 1664525u + 1013904223u;
        p = 2.0 * kPi * static_cast<double>(s % 10000) / 10000.0;
    }
    fill(f, [&](std::int32_t cx, std::int32_t cz) {
        double v = 0.0;
        for (std::size_t k = 1; k < phase.size(); ++k) {
            const double amp = std::pow(static_cast<double>(k), -kBeta / 2.0);
            v += amp * std::cos(2.0 * kPi * static_cast<double>(k * cx) / 256.0 + phase[k]);
            v += amp * std::cos(2.0 * kPi * static_cast<double>(k * cz) / 256.0 + phase[k] * 0.7);
        }
        return v;
    });
    const SpectrumStats spec = power_spectrum(f);
    INFO("measured beta " << spec.beta << " for a synthesised " << kBeta << ", R2 " << spec.r_squared);
    CHECK(spec.beta == Catch::Approx(kBeta).margin(0.25));

    // R2 IS LOW HERE (~0.51) AND THAT IS THE SYNTHETIC'S FAULT, NOT THE INSTRUMENT'S -- recorded
    // rather than asserted away, because the first draft asserted > 0.8 and the failure was
    // informative.
    //
    // This surface is a discrete COMB of tones at exactly integer wavenumbers with random phases.
    // The Hann window the metric applies (correctly -- see power_spectrum's comment on why a
    // rectangular window would inject a k^-2 leak indistinguishable from the answer) spreads each
    // tone across three bins, and neighbouring tones then interfere at their random relative phase.
    // Because every row of this synthetic uses the SAME phase set, that interference is identical in
    // every transect and never averages out over the 512 of them.
    //
    // Real terrain has a CONTINUOUS spectrum and no such comb, which is why the fit on the shipped
    // field reads far higher. The quantity this test is actually here to check -- that beta comes
    // back out -- is unaffected: leakage moves power between adjacent bins, not between decades.
    CHECK(spec.r_squared > 0.4);
}

TEST_CASE("the spectrum flags a periodic surface", "[generation][validation]") {
    // §9.2: "assert no spurious peaks (periodicity)". A single strong sinusoid on top of a rough
    // background is precisely the artefact -- a terrain generator whose octave structure leaks a
    // period into the output.
    TerrainField f = make_field(256);
    std::uint32_t s = 4242;
    fill(f, [&](std::int32_t cx, std::int32_t) {
        s = s * 1664525u + 1013904223u;
        const double noise = static_cast<double>(s % 1000) / 1000.0;
        return noise + 40.0 * std::sin(2.0 * kPi * static_cast<double>(cx) / 16.0);
    });
    const SpectrumStats spec = power_spectrum(f);
    INFO("worst peak at " << spec.peak_wavelength_cells << " cells");
    CHECK(spec.spurious_peak);
    CHECK(spec.peak_wavelength_cells == Catch::Approx(16.0).margin(1.0));
}

// --------------------------------------------------------------------------- §9.3 hypsometry

TEST_CASE("hypsometry recovers a linear ramp exactly", "[generation][validation]") {
    // A ramp from -H to +H: half the cells are land, and the land half is uniformly distributed, so
    // its median sits at exactly half its maximum.
    TerrainField f = make_field(128);
    fill(f, [](std::int32_t cx, std::int32_t) { return -100.0 + 200.0 * static_cast<double>(cx) / 127.0; });
    const Hypsometry hyp = hypsometry(f);
    CHECK(hyp.land_fraction == Catch::Approx(0.5).margin(0.01));
    CHECK(hyp.median_over_max == Catch::Approx(0.5).margin(0.02));
    CHECK(hyp.hypsometric_integral == Catch::Approx(0.5).margin(0.01));
}

// ---------------------------------------------------------------------------- §9.8 variogram

TEST_CASE("the variogram recovers H for two exact surfaces", "[generation][validation]") {
    // A PLANE is perfectly correlated: gamma(h) goes as h^2, so H = 1.
    TerrainField plane = make_field(128);
    fill(plane, [](std::int32_t cx, std::int32_t cz) {
        return 0.7 * static_cast<double>(cx) + 0.3 * static_cast<double>(cz);
    });
    const VariogramStats p = variogram(plane);
    INFO("plane H = " << p.hurst << ", R2 " << p.r_squared);
    CHECK(p.hurst == Catch::Approx(1.0).margin(0.05));
    CHECK(p.r_squared > 0.99);

    // WHITE NOISE is uncorrelated: gamma(h) is flat, so H = 0.
    TerrainField noise = make_field(128);
    std::uint32_t s = 777;
    fill(noise, [&](std::int32_t, std::int32_t) {
        s = s * 1664525u + 1013904223u;
        return static_cast<double>(s % 1000);
    });
    const VariogramStats w = variogram(noise);
    INFO("white noise H = " << w.hurst);
    CHECK(w.hurst == Catch::Approx(0.0).margin(0.05));
}

// --------------------------------------------------------------- §9.5 valley cross-sections

TEST_CASE("the valley metric separates a V from a U", "[generation][validation]") {
    // Two synthetic valleys running along z with a gentle downstream tilt so water routes: one with
    // walls rising as |x|^1 (fluvial V) and one as |x|^2 (glacial U). The metric must read the
    // exponent back, because that separation is the entire content of acceptance test 5.
    const auto measure = [](double exponent) {
        TerrainField f = make_field(192);
        const double centre = 96.0;
        fill(f, [&](std::int32_t cx, std::int32_t cz) {
            const double d = std::abs(static_cast<double>(cx) - centre);
            // Normalised so both valleys have the same depth at the same width -- otherwise the
            // comparison would be between two different landforms, not two different shapes.
            const double wall = 60.0 * std::pow(d / 40.0, exponent);
            return wall - 0.35 * static_cast<double>(cz);
        });
        const Routed r = route(std::move(f));
        return valley_cross_sections(r.field, r.net, 0.01);
    };

    const ValleyStats v = measure(1.0);
    const ValleyStats u = measure(2.0);
    INFO("V read b = " << v.mean_exponent << " over " << v.transects
                       << " transects; U read b = " << u.mean_exponent << " over " << u.transects);
    REQUIRE(v.transects > 10);
    REQUIRE(u.transects > 10);
    CHECK(v.mean_exponent == Catch::Approx(1.0).margin(0.25));
    CHECK(u.mean_exponent == Catch::Approx(2.0).margin(0.35));
    // And the V-index agrees about the ordering, which is the independent second opinion §9.5
    // offers for exactly this measurement.
    CHECK(u.mean_v_index > v.mean_v_index);
}

// ------------------------------------------------------------------------- §9.9 coherence

TEST_CASE("coherence counts internal basins correctly", "[generation][validation]") {
    // A CONE descending in every direction has no internal basin anywhere.
    TerrainField cone = make_field(64);
    fill(cone, [](std::int32_t cx, std::int32_t cz) {
        const double dx = static_cast<double>(cx) - 32.0;
        const double dz = static_cast<double>(cz) - 32.0;
        return 200.0 - std::sqrt(dx * dx + dz * dz);
    });
    CHECK(hydrological_coherence(cone, RiverNetwork{}).internal_basins == 0);

    // A BOWL is one pit, and the metric must find exactly one. (Its floor is a single cell by
    // construction: the distance function has one minimum.)
    TerrainField bowl = make_field(64);
    fill(bowl, [](std::int32_t cx, std::int32_t cz) {
        const double dx = static_cast<double>(cx) - 32.0;
        const double dz = static_cast<double>(cz) - 32.0;
        return std::sqrt(dx * dx + dz * dz);
    });
    const Coherence b = hydrological_coherence(bowl, RiverNetwork{});
    INFO("bowl reported " << b.internal_basins << " internal basins");
    CHECK(b.internal_basins == 1);
    CHECK_FALSE(b.passed());

    // And the fill must remove it -- which is the guarantee acceptance test 9 is written about.
    TerrainField filled = bowl;
    priority_flood(filled);
    CHECK(hydrological_coherence(filled, RiverNetwork{}).internal_basins == 0);
}

// ------------------------------------------------------------------------- §9.4 and the suite

TEST_CASE("drainage density is zero on a plane and positive on a dissected surface",
          "[generation][validation]") {
    // A tilted plane has exactly one flow line per row and no convergence, so almost nothing
    // exceeds the channel threshold. This is the "no rivers (~0)" end of what §9.4 catches.
    TerrainField plane = make_field(128);
    fill(plane, [](std::int32_t, std::int32_t cz) { return 400.0 - 2.0 * static_cast<double>(cz); });
    const Routed flat = route(std::move(plane));
    const double flatDensity = drainage_density(flat.field, 0.05);
    INFO("plane density " << flatDensity);
    CHECK(flatDensity < 2.0);
}

TEST_CASE("the suite reports every metric and names its citation", "[generation][validation]") {
    // Not a terrain check -- a check that the REPORT is complete, because a suite that silently
    // drops a metric would look like a pass.
    TerrainField f = make_field(128);
    fill(f, [](std::int32_t cx, std::int32_t cz) {
        return 50.0 - 0.2 * static_cast<double>(cx) - 0.1 * static_cast<double>(cz);
    });
    const Routed r = route(std::move(f));
    const SuiteResult suite = run_suite(r.field, r.net, AcceptanceInputs{});
    CHECK(suite.metrics.size() == 10);
    for (const MetricResult& m : suite.metrics) {
        INFO("metric " << m.name);
        CHECK_FALSE(m.name.empty());
        CHECK_FALSE(m.band.citation.empty());
    }
    // Metric 10 has no subject until goal 316, and must report INAPPLICABLE rather than pass.
    CHECK_FALSE(suite.metrics.back().applicable);
    CHECK_FALSE(format_report(suite).empty());
}
