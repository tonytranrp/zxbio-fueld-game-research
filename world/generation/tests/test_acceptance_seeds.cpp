// Prompt 006 goals 319 and 320: the acceptance suite as a permanent gate, across five seeds, with
// the OLD noise terrain measured beside the new pipeline as the before column.
//
// Research Part 7 §9's closing line is the whole design:
//
//   "Run as a nightly golden-seed suite: fixed seeds, dumped statistics, threshold assertions --
//    the same discipline as the engine's existing --verify-frame and slope-bound regression tests,
//    applied to geomorphology."
//
// WHY THIS IS NOT "ASSERT ALL TEN PASS". Five of the ten currently fail on the shipped seed, and
// §11 of the pass log explains each. **A gate that fails on the day it is written is not a gate.**
// Per the prompt's own rule on legitimate negative results -- "report the measured value, the band,
// the cost, and open a goal; do not widen the band to pass" -- this file:
//
//   * ASSERTS the metrics that currently pass, on every seed. Those are real regressions if they
//     ever break, and one seed can be lucky (Prompt 001 learned that when a single-seed test hid
//     sevenfold variance in tree tip counts).
//   * RECORDS the ones that fail, with their spread across seeds, so the numbers move under
//     observation rather than silently.
//
// Goal 319's Check asks for the SPREAD across seeds, not just the mean, "because that spread is
// what tells you whether a band is being met robustly or narrowly." So every assertion below runs
// per seed and the report prints min/max.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "world/generation/field/fluvial.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/field/rivers.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/validation/acceptance.hpp"

using namespace world::generation::validation;
using namespace world::generation::field;

namespace {

/// Five fixed seeds. Goal 319: "over at least five seeds (one seed can be lucky)".
constexpr std::array<int, 5> kSeeds{1337, 99, 4242, 20260906, 7};

/// Smaller than the shipped 500x500 so five seeds run in a test rather than a coffee break; still
/// above the 4 km continent scale, which §7 of the log established is the floor below which the
/// field contains no drainage network to measure.
constexpr std::int32_t kCells = 320;
constexpr float kCellSize = 16.0f;

/// The 512-or-larger patch of final heights §9 asks the shape tests for.
constexpr std::int32_t kSurfaceCells = 512;
constexpr float kSurfaceSpacing = 7.8125f;

[[nodiscard]] TerrainField macro_field(int seed) {
    const float half = 0.5f * kCellSize * static_cast<float>(kCells);
    TerrainField f{
        FieldGeometry{.origin_x = -half, .origin_z = -half, .cell_size = kCellSize, .cells = kCells}};
    run_pipeline(f, MacroParams{.seed = seed}, -1);
    // Same reason as test_detail_spectrum.cpp: the suite must measure the world that ships, and the
    // shipped world puts its origin on land.
    recentre_on_land(f, 320.0f);
    return f;
}

/// Samples `height_at` onto a grid. With `macro` null this is the OLD four-octave noise terrain --
/// goal 320's before column, and the reason `HeightmapGenerator`'s single-argument constructor was
/// kept when the field was introduced.
[[nodiscard]] TerrainField sample_surface(int seed, std::shared_ptr<const TerrainField> macro) {
    TerrainField surface{FieldGeometry{.origin_x = -0.5f * kSurfaceCells * kSurfaceSpacing,
                                       .origin_z = -0.5f * kSurfaceCells * kSurfaceSpacing,
                                       .cell_size = kSurfaceSpacing,
                                       .cells = kSurfaceCells}};
    const world::generation::HeightmapGenerator gen =
        macro == nullptr ? world::generation::HeightmapGenerator{seed}
                         : world::generation::HeightmapGenerator{seed, std::move(macro)};
    const std::span<float> h = surface.plane(Plane::Elevation);
    for (std::int32_t cz = 0; cz < kSurfaceCells; ++cz) {
        for (std::int32_t cx = 0; cx < kSurfaceCells; ++cx) {
            const glm::vec2 w = surface.geometry().to_world(cx, cz);
            h[surface.index(cx, cz)] = gen.height_at(w.x, w.y);
        }
    }
    return surface;
}

struct SeedRun {
    int seed = 0;
    SuiteResult suite;
    /// Internal basins BEFORE the fill. The before column's most important number: the old terrain
    /// was never filled or routed at all, so measuring test 9 on it after running the new
    /// pipeline's own priority-flood over it would report zero for both columns and prove nothing.
    /// This is the quantity that actually differed.
    std::size_t unfilled_basins = 0;
    /// How log-log-linear the spectrum is. Reported because the pipeline's beta and the noise
    /// terrain's beta are not comparable if one of them is not a power law at all.
    double spectrum_r2 = 0.0;
};

/// The full pipeline, or -- with `usePipeline` false -- the old noise terrain routed the same way,
/// so the two columns are measured by identical instruments and differ only in the terrain.
[[nodiscard]] SeedRun run(int seed, bool usePipeline) {
    TerrainField macro = macro_field(seed);
    TerrainField surface = usePipeline ? sample_surface(seed, std::make_shared<const TerrainField>(macro))
                                       : sample_surface(seed, nullptr);
    // The NETWORK tests need a routed field. For the before column that is the noise terrain
    // itself, resampled onto the macro grid -- otherwise the two columns' network tests would be
    // measuring the same macro field and the comparison would be silently vacuous.
    TerrainField routed = macro;
    if (!usePipeline) {
        const world::generation::HeightmapGenerator gen{seed};
        const std::span<float> rh = routed.plane(Plane::Elevation);
        for (std::int32_t cz = 0; cz < routed.cells(); ++cz) {
            for (std::int32_t cx = 0; cx < routed.cells(); ++cx) {
                const glm::vec2 w = routed.geometry().to_world(cx, cz);
                rh[routed.index(cx, cz)] = gen.height_at(w.x, w.y);
            }
        }
    }
    const TerrainField unfilled = routed;
    const std::size_t basinsBefore = hydrological_coherence(unfilled, RiverNetwork{}).internal_basins;
    priority_flood(routed);
    FlowNetwork net = build_flow_network(routed);
    accumulate_flow(routed, net);

    AcceptanceInputs in;
    in.channel_threshold_km2 = 0.01;
    in.rivers = extract_rivers(routed, net, unfilled.plane(Plane::Elevation), HydrologyParams{});
    in.final_surface = &surface;
    SeedRun out{seed, run_suite(routed, net, in), basinsBefore, 0.0};
    out.spectrum_r2 = power_spectrum(surface).r_squared;
    return out;
}

/// min / max / mean of one metric across the runs, which is what goal 319's Check asks to see.
struct Spread {
    double lo = 0.0;
    double hi = 0.0;
    double mean = 0.0;
    std::size_t passes = 0;
    std::size_t applicable = 0;
};

[[nodiscard]] Spread spread_of(const std::vector<SeedRun>& runs, std::size_t metric) {
    Spread s;
    bool first = true;
    for (const SeedRun& r : runs) {
        const MetricResult& m = r.suite.metrics[metric];
        if (!m.applicable) {
            continue;
        }
        ++s.applicable;
        s.passes += m.passed ? 1u : 0u;
        s.mean += m.value;
        if (first) {
            s.lo = s.hi = m.value;
            first = false;
        }
        s.lo = std::min(s.lo, m.value);
        s.hi = std::max(s.hi, m.value);
    }
    if (s.applicable != 0) {
        s.mean /= static_cast<double>(s.applicable);
    }
    return s;
}

} // namespace

TEST_CASE("the acceptance suite holds across five seeds", "[generation][validation][seeds]") {
    std::vector<SeedRun> pipeline;
    pipeline.reserve(kSeeds.size());
    for (const int seed : kSeeds) {
        pipeline.push_back(run(seed, true));
    }
    REQUIRE(pipeline.front().suite.metrics.size() == 10);

    // Report the whole table first, so a failure below is readable in context rather than as one
    // orphaned number.
    for (std::size_t m = 0; m < 10; ++m) {
        const Spread s = spread_of(pipeline, m);
        UNSCOPED_INFO(pipeline.front().suite.metrics[m].name << ": " << s.lo << " .. " << s.hi << " (mean "
                                                             << s.mean << "), " << s.passes << "/"
                                                             << s.applicable << " seeds pass");
    }

    // THE GATE: the metrics that pass on ALL FIVE seeds. Real regressions if they ever stop.
    //
    // Spectral beta is the one that is NOT here: it passes on the shipped seed and 3 of 5 overall,
    // reading 2.019 .. 2.639 against a ceiling of 2.5. See the spread assertions below for the
    // cause (an absolute detail amplitude against a seed-varying macro relief) and the fix.
    // FIVE of the ten now hold on every seed, up from three: recentring the world origin onto land
    // (goal 321) put hypsometry back to 5/5 and took Hurst from 4/5 to 5/5, because both were
    // previously being measured over a window that was half ocean.
    const std::array<std::size_t, 5> gated{0, 2, 3, 7, 8};
    for (const std::size_t m : gated) {
        const Spread s = spread_of(pipeline, m);
        INFO("metric " << pipeline.front().suite.metrics[m].name << " spread " << s.lo << " .. " << s.hi);
        CHECK(s.applicable == kSeeds.size());
        CHECK(s.passes == kSeeds.size());
    }

    // And the spread itself is the point of running five: a band met by a hair on one seed and
    // missed on another is not met. Drainage density has the tightest band of the four, so it is
    // the one worth pinning a spread on.
    const Spread density = spread_of(pipeline, 3);
    INFO("drainage density across seeds: " << density.lo << " .. " << density.hi);
    CHECK(density.lo > kDrainageDensityKmPerKm2.lo);
    CHECK(density.hi < kDrainageDensityKmPerKm2.hi);

    // THE SPECTRUM'S SPREAD, and the finding it carries. Beta reads 1.56 .. 2.64 across seeds
    // against a band of [1.6, 2.5] -- centred, but straddling both edges. Hurst reads 0.45 .. 0.74
    // against [0.46, 0.77], the same shape of result.
    //
    // The cause is structural, not a bad constant: `DetailParams::amplitude_m` is an ABSOLUTE 2 m,
    // and the macro field's relief varies by seed. A seed with more relief gets a steeper spectrum
    // and a seed with less gets a flatter one, because the join between the two terms moves. The fix
    // is to express the detail amplitude as a FRACTION OF THE MACRO'S LOCAL RELIEF so the join is
    // scale-invariant -- which changes `height_at`'s cost and is opened as a goal rather than done
    // here.
    //
    // What is asserted is that the MEAN sits inside the band on both, since that is what the
    // amplitude sweep selected and is what a retune must not lose.
    const Spread beta = spread_of(pipeline, 1);
    const Spread hurst = spread_of(pipeline, 7);
    INFO("spectral beta " << beta.lo << " .. " << beta.hi << " (mean " << beta.mean << "); Hurst "
                          << hurst.lo << " .. " << hurst.hi << " (mean " << hurst.mean << ")");
    CHECK(kSpectralBeta.contains(beta.mean));
    CHECK(kVariogramHurst.contains(hurst.mean));
}

TEST_CASE("the pipeline beats the old noise terrain where it should", "[generation][validation][seeds]") {
    // GOAL 320: the before column. The prompt's own prediction, which this tests rather than
    // assumes: "Expect noise to fail 4, 7, 9 outright and to sit at the wrong end of 1 and 8; if it
    // unexpectedly passes one, that is interesting and worth a sentence."
    std::vector<SeedRun> noise;
    std::vector<SeedRun> pipeline;
    for (const int seed : kSeeds) {
        noise.push_back(run(seed, false));
        pipeline.push_back(run(seed, true));
    }

    for (std::size_t m = 0; m < 10; ++m) {
        const Spread n = spread_of(noise, m);
        const Spread p = spread_of(pipeline, m);
        UNSCOPED_INFO(pipeline.front().suite.metrics[m].name
                      << ":  noise " << n.mean << " (" << n.passes << "/" << n.applicable << ")   pipeline "
                      << p.mean << " (" << p.passes << "/" << p.applicable << ")");
    }

    // The three the pipeline exists to buy, asserted as a COMPARISON rather than against a band --
    // which is the honest form of "how much did this pass buy", and survives the bands moving.
    //
    // 4 (drainage density) and 9 (hydrological coherence) are the pair the research singles out:
    // "no fill -> dead-end rivers ... this one test catches both 'no rivers' and 'noise gullies
    // everywhere'".
    // TEST 9, MEASURED THE WAY IT ACTUALLY DIFFERS. A first version compared internal basins AFTER
    // running priority-flood over both columns and got 0 versus 0 -- which proves only that the fill
    // works, since the old terrain was never filled or routed in the first place. The honest
    // comparison is BEFORE the fill: the pipeline's own output has been through fill, incision and
    // diffusion, and the question is how many undrained pits each surface carries on its own.
    double noiseBasins = 0.0;
    double pipeBasins = 0.0;
    for (std::size_t k = 0; k < kSeeds.size(); ++k) {
        noiseBasins += static_cast<double>(noise[k].unfilled_basins);
        pipeBasins += static_cast<double>(pipeline[k].unfilled_basins);
    }
    noiseBasins /= static_cast<double>(kSeeds.size());
    pipeBasins /= static_cast<double>(kSeeds.size());
    INFO("internal basins before any fill: noise " << noiseBasins << ", pipeline " << pipeBasins);
    CHECK(pipeBasins < noiseBasins);

    // Drainage density: both columns pass the band once the new machinery is applied to them, which
    // is itself worth knowing -- but the noise terrain sits at the band's very edge and the
    // pipeline sits mid-band, which is the difference that matters.
    const Spread noiseDensity = spread_of(noise, 3);
    const Spread pipeDensity = spread_of(pipeline, 3);
    INFO("drainage density: noise " << noiseDensity.mean << " (band edge " << kDrainageDensityKmPerKm2.lo
                                    << "), pipeline " << pipeDensity.mean);
    CHECK(pipeDensity.mean > noiseDensity.mean);

    // And the spectrum, which the pipeline now WINS after goal 320's follow-up retuned the detail
    // term. This assertion is INVERTED from the one first written here, and kept rather than
    // deleted: goal 320 found the pipeline had made the spectrum worse, the retune fixed it, and the
    // test that recorded the regression is the same test that now records the repair.
    // Formerly: where the pipeline was WORSE and the reason had to be recorded
    // rather than smoothed over: four-octave noise is self-similar by construction and fits a power
    // law cleanly, while macro + a two-octave detail term is not self-similar at all. Comparing
    // their betas without comparing their R2 would be comparing a fit to a non-fit.
    double noiseR2 = 0.0;
    double pipeR2 = 0.0;
    for (std::size_t k = 0; k < kSeeds.size(); ++k) {
        noiseR2 += noise[k].spectrum_r2;
        pipeR2 += pipeline[k].spectrum_r2;
    }
    noiseR2 /= static_cast<double>(kSeeds.size());
    pipeR2 /= static_cast<double>(kSeeds.size());
    UNSCOPED_INFO("spectrum log-log R2: noise " << noiseR2 << ", pipeline " << pipeR2);
    CHECK(pipeR2 > noiseR2);

    // And the surface-correlation win, the largest single improvement in the table: Hurst goes from
    // 0.208 (0/5 seeds in band) to 0.628 (4/5).
    const Spread noiseHurst = spread_of(noise, 7);
    const Spread pipeHurst = spread_of(pipeline, 7);
    INFO("Hurst: noise " << noiseHurst.mean << " (" << noiseHurst.passes << "/5), pipeline "
                         << pipeHurst.mean << " (" << pipeHurst.passes << "/5)");
    CHECK(pipeHurst.passes > noiseHurst.passes);
}
