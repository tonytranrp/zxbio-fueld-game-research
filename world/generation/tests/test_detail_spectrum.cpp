// Prompt 006, goal 320's follow-up: the detail term, SWEPT against the acceptance suite.
//
// Goal 320 measured that this pass made the surface spectrum worse -- beta 2.27 -> 0.50, Hurst
// 0.208 -> 0.129 -- because the detail term was never re-tuned to continue the macro field's
// spectrum after the macro field arrived. This file is how the replacement was chosen: a sweep over
// `DetailParams` with the metrics read off each configuration, rather than a value reasoned toward
// and then defended.
//
// It stays in the suite as a REGRESSION GATE on the chosen values. Anyone retuning the detail term
// has to move these numbers deliberately, and will see immediately what it costs.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <vector>

#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/validation/acceptance.hpp"

using namespace world::generation::validation;
using namespace world::generation::field;
using world::generation::DetailParams;
using world::generation::HeightmapGenerator;

namespace {

constexpr std::int32_t kSurfaceCells = 512;
constexpr float kSurfaceSpacing = 7.8125f;

[[nodiscard]] std::shared_ptr<const TerrainField> macro(int seed) {
    // THE SHIPPED FIELD SIZE, not a smaller one. The macro spectrum depends on the field's extent,
    // so a sweep run at 5.1 km selects an amplitude for a world the app does not build -- measured:
    // the same parameters read beta 2.11 at 320 cells and 2.66 at the shipped 500. Same reasoning
    // as recentring the origin: the selection has to be made on the world that ships.
    constexpr std::int32_t kCells = 500;
    const float half = 0.5f * 16.0f * static_cast<float>(kCells);
    TerrainField f{FieldGeometry{.origin_x = -half, .origin_z = -half, .cell_size = 16.0f, .cells = kCells}};
    run_pipeline(f, MacroParams{.seed = seed}, -1);
    // RECENTRE, because the app does. The shape metrics are sampled in a window at world (0, 0),
    // and without this that window is wherever the seed put the field's centre -- which for the
    // shipped seed is a bay. Measuring the detail term's spectrum over open water would select an
    // amplitude for a surface no player stands on.
    recentre_on_land(f, 320.0f);
    return std::make_shared<const TerrainField>(std::move(f));
}

struct Reading {
    double beta = 0.0;
    double beta_r2 = 0.0;
    double hurst = 0.0;
    double hurst_r2 = 0.0;
    double mean_slope_deg = 0.0;
};

[[nodiscard]] Reading measure(const std::shared_ptr<const TerrainField>& m, int seed, const DetailParams& p) {
    TerrainField surface{FieldGeometry{.origin_x = -0.5f * kSurfaceCells * kSurfaceSpacing,
                                       .origin_z = -0.5f * kSurfaceCells * kSurfaceSpacing,
                                       .cell_size = kSurfaceSpacing,
                                       .cells = kSurfaceCells}};
    {
        const HeightmapGenerator gen{seed, m, p};
        const std::span<float> h = surface.plane(Plane::Elevation);
        for (std::int32_t cz = 0; cz < kSurfaceCells; ++cz) {
            for (std::int32_t cx = 0; cx < kSurfaceCells; ++cx) {
                const glm::vec2 w = surface.geometry().to_world(cx, cz);
                h[surface.index(cx, cz)] = gen.height_at(w.x, w.y);
            }
        }
    }
    const SpectrumStats s = power_spectrum(surface);
    const VariogramStats v = variogram(surface);
    const SlopeStats sl = slope_distribution(surface);
    return Reading{s.beta, s.r_squared, v.hurst, v.r_squared, sl.mean_slope_deg};
}

} // namespace

TEST_CASE("the detail parameters were selected by sweeping, and the sweep is reproducible",
          "[generation][validation][detail]") {
    // The sweep itself. Reported through INFO so the table is visible in a verbose run, and the
    // configurations are the ones that mattered rather than a full grid: the first is what goal 320
    // measured, the last is what shipped.
    const std::shared_ptr<const TerrainField> m = macro(1337);

    struct Case {
        const char* label;
        DetailParams p;
    };
    const std::array<Case, 8> cases{{
        {"as measured by 320: 2 octaves @ 50 m, gain 0.5, 16 m",
         DetailParams{
             .scale_m = 50.0f, .octaves = 2, .lacunarity = 2.0f, .gain = 0.5f, .amplitude_m = 16.0f}},
        {"5 octaves @ 32 m, gain 0.5, 16 m",
         DetailParams{
             .scale_m = 32.0f, .octaves = 5, .lacunarity = 2.0f, .gain = 0.5f, .amplitude_m = 16.0f}},
        {"5 octaves, gain 0.71, 16 m",
         DetailParams{
             .scale_m = 32.0f, .octaves = 5, .lacunarity = 2.0f, .gain = 0.71f, .amplitude_m = 16.0f}},
        {"amplitude 6 m", DetailParams{.amplitude_m = 6.0f}},
        {"amplitude 4 m", DetailParams{.amplitude_m = 4.0f}},
        {"amplitude 3 m", DetailParams{.amplitude_m = 3.0f}},
        {"amplitude 2 m", DetailParams{.amplitude_m = 2.0f}},
        {"shipped default", DetailParams{}},
    }};

    {
        // DIAGNOSTIC: the raw semivariogram ladder and the surface's own spread, printed because
        // two instruments both reading "uncorrelated" is either a real surface or a shared mistake,
        // and gamma(h) in metres-squared is a number that can be checked by hand.
        TerrainField s{FieldGeometry{.origin_x = -0.5f * kSurfaceCells * kSurfaceSpacing,
                                     .origin_z = -0.5f * kSurfaceCells * kSurfaceSpacing,
                                     .cell_size = kSurfaceSpacing,
                                     .cells = kSurfaceCells}};
        const HeightmapGenerator gen{1337, m, DetailParams{}};
        const std::span<float> h = s.plane(Plane::Elevation);
        double lo = 1e30;
        double hi = -1e30;
        for (std::int32_t cz = 0; cz < kSurfaceCells; ++cz) {
            for (std::int32_t cx = 0; cx < kSurfaceCells; ++cx) {
                const glm::vec2 w = s.geometry().to_world(cx, cz);
                const float v = gen.height_at(w.x, w.y);
                h[s.index(cx, cz)] = v;
                lo = std::min(lo, static_cast<double>(v));
                hi = std::max(hi, static_cast<double>(v));
            }
        }
        UNSCOPED_INFO("surface range " << lo << " .. " << hi << " m over "
                                       << kSurfaceCells * kSurfaceSpacing << " m");
        for (std::int32_t lag = 1; lag <= 64; lag *= 2) {
            double sum = 0.0;
            std::size_t pairs = 0;
            for (std::int32_t cz = 0; cz < kSurfaceCells; cz += 2) {
                for (std::int32_t cx = 0; cx + lag < kSurfaceCells; cx += 2) {
                    const double d = static_cast<double>(h[s.index(cx + lag, cz)]) - h[s.index(cx, cz)];
                    sum += d * d;
                    ++pairs;
                }
            }
            UNSCOPED_INFO("  gamma(" << static_cast<double>(lag) * kSurfaceSpacing
                                     << " m) = " << 0.5 * sum / static_cast<double>(pairs));
        }
    }

    for (const Case& c : cases) {
        const Reading r = measure(m, 1337, c.p);
        UNSCOPED_INFO(c.label << ":  beta " << r.beta << " (R2 " << r.beta_r2 << ")   H " << r.hurst
                              << " (R2 " << r.hurst_r2 << ")   mean slope " << r.mean_slope_deg << " deg");
    }

    // THE GATE. The shipped configuration must stay inside the two research bands it was chosen to
    // satisfy, and must not go back to being a cliff field.
    const Reading shipped = measure(m, 1337, DetailParams{});
    INFO("shipped: beta " << shipped.beta << " (R2 " << shipped.beta_r2 << "), H " << shipped.hurst
                          << ", mean slope " << shipped.mean_slope_deg << " deg");
    CHECK(kSpectralBeta.contains(shipped.beta));
    CHECK(kVariogramHurst.contains(shipped.hurst));
    // The surface must actually BE a power law, not just have a fitted slope inside the band --
    // which is the distinction goal 320 turned on. 0.54 was the broken configuration's R2.
    CHECK(shipped.beta_r2 > 0.75);
    // And it must be walkable terrain rather than the 42.6-degree cliff field the two-octave
    // version produced. Prompt 003's body slides above 40 degrees.
    CHECK(shipped.mean_slope_deg < 35.0);
}
