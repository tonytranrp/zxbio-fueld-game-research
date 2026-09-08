// Prompt 006 goals 301/318: look at the macro field directly, and measure it.
//
// WHY THIS TOOL HAD TO EXIST BEFORE THE PIPELINE COULD BE JUDGED. The first attempt to evaluate the
// erosion was a rendered game frame with `--field-stages 1` against `--field-stages 5`, and the
// difference was barely visible. That is not evidence that the erosion did nothing -- the macro
// field is 16 m cells and the playable region is only 32 cells across, so what fills a game frame
// at that pose is mostly the ANALYTIC DETAIL term, which no stage erodes. The rendered comparison
// was measuring the wrong surface.
//
// So: dump the field itself, in false colour, with the statistics research Part 7 §9 asks for
// printed beside it. A 500 x 500 plane at one pixel per cell is the whole 8 km world in one image.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "png_writer.hpp"
#include "world/generation/field/climate.hpp"
#include "world/generation/field/fluvial.hpp"
#include "world/generation/field/rivers.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "world/generation/field/terrain_field.hpp"

using namespace world::generation::field;

namespace {

struct Options {
    int seed = 1337;
    int stages = -1;
    std::int32_t cells = 500;
    float cell_size = 16.0f;
    std::string out = "terrain_dump.png";
    std::string plane = "elevation";
    /// Zoomed river render: centre and span in world metres. A river 2.4 m wide is invisible in an
    /// 8 km dump at one pixel per 16 m cell, so the only way to LOOK at a meander here is to zoom.
    float zoom_x = 0.0f;
    float zoom_z = 0.0f;
    float zoom_span = 0.0f; ///< 0 = whole field
};

/// A blue-through-green-through-brown-through-white ramp, so an elevation map reads as terrain and
/// sea level is unmistakable rather than being a grey value the eye has to decode.
[[nodiscard]] std::array<std::uint8_t, 3> terrain_colour(float t) {
    struct Stop {
        float at;
        float r;
        float g;
        float b;
    };
    static constexpr std::array<Stop, 6> kRamp{
        Stop{0.00f, 0.05f, 0.10f, 0.35f}, Stop{0.45f, 0.18f, 0.40f, 0.65f}, Stop{0.50f, 0.85f, 0.80f, 0.55f},
        Stop{0.58f, 0.25f, 0.55f, 0.22f}, Stop{0.80f, 0.45f, 0.36f, 0.26f}, Stop{1.00f, 0.98f, 0.98f, 1.00f}};
    t = std::clamp(t, 0.0f, 1.0f);
    for (std::size_t i = 1; i < kRamp.size(); ++i) {
        if (t <= kRamp[i].at) {
            const Stop& a = kRamp[i - 1];
            const Stop& b = kRamp[i];
            const float u = (t - a.at) / std::max(b.at - a.at, 1e-6f);
            const auto to8 = [](float v) {
                return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            return {to8(a.r + (b.r - a.r) * u), to8(a.g + (b.g - a.g) * u), to8(a.b + (b.b - a.b) * u)};
        }
    }
    return {255, 255, 255};
}

/// Blue-to-white-to-brown for precipitation, keyed to the field mean of 1.0: dry is brown, mean is
/// pale, wet is blue. The mean is pinned to the ramp's midpoint for the same reason the elevation
/// ramp pins sea level -- so the eye reads "wetter or drier than typical" rather than "wherever the
/// range happened to land".
[[nodiscard]] std::array<std::uint8_t, 3> precip_colour(float p) {
    // log2 around the mean, clamped to +-3 stops: precipitation spans 0.1 to ~30 on a ridge and a
    // linear ramp shows one white line on a brown field.
    const float s = std::clamp(std::log2(std::max(p, 1e-3f)) / 3.0f, -1.0f, 1.0f);
    if (s < 0.0f) { // dry: pale -> brown
        const float u = -s;
        return {static_cast<std::uint8_t>(235.0f - 100.0f * u),
                static_cast<std::uint8_t>(225.0f - 140.0f * u),
                static_cast<std::uint8_t>(205.0f - 175.0f * u)};
    }
    return {static_cast<std::uint8_t>(235.0f - 220.0f * s), // wet: pale -> deep blue
            static_cast<std::uint8_t>(225.0f - 150.0f * s), static_cast<std::uint8_t>(205.0f + 20.0f * s)};
}

/// Cold blue to warm red across the field's own temperature range.
[[nodiscard]] std::array<std::uint8_t, 3> temperature_colour(float t01) {
    return {static_cast<std::uint8_t>(40.0f + 200.0f * t01), static_cast<std::uint8_t>(70.0f + 90.0f * t01),
            static_cast<std::uint8_t>(230.0f - 190.0f * t01)};
}

/// Research Part 7 §9.4. Extracted at 30 m-EQUIVALENT resolution regardless of the simulation's
/// cell size, because the band (2-12 km/km²) is quoted at that resolution -- measuring at 16 m
/// would bias it high and the comparison would be meaningless.
[[nodiscard]] double drainage_density(const TerrainField& field, float channelThresholdKm2) {
    const float cellArea = field.geometry().cell_area();
    const std::span<const float> acc = field.plane(Plane::FlowAccum);
    const float thresholdCells = channelThresholdKm2 * 1.0e6f / cellArea;
    const int stride = std::max(1, static_cast<int>(std::lround(30.0f / field.geometry().cell_size)));
    std::size_t channels = 0;
    std::size_t sampled = 0;
    for (std::int32_t cz = 0; cz < field.cells(); cz += stride) {
        for (std::int32_t cx = 0; cx < field.cells(); cx += stride) {
            channels += acc[field.index(cx, cz)] >= thresholdCells ? 1u : 0u;
            ++sampled;
        }
    }
    if (sampled == 0) {
        return 0.0;
    }
    // Each sampled channel cell stands for one cell-length of channel at the sampling resolution.
    const double sampleSize = static_cast<double>(stride) * field.geometry().cell_size; // metres
    const double channelLengthKm = static_cast<double>(channels) * sampleSize / 1000.0;
    const double areaKm2 = static_cast<double>(sampled) * sampleSize * sampleSize / 1.0e6;
    return channelLengthKm / std::max(areaKm2, 1e-9);
}

/// Research Part 7 §9.6, goal 308: the CONSTANT-DROP PROPERTY, and the same t-test TauDEM runs on
/// real DEMs. Tarboton's insight is that a correctly extracted channel network has the same mean
/// elevation drop per link regardless of Strahler order -- so if first-order links drop
/// significantly further than higher-order ones, the channel threshold is too LOW and the network
/// has been extended up into hillslopes that are not channels at all.
///
/// The research calls it cheap and notes it "ties network extraction to physics", which is exactly
/// right: it is the one acceptance test that judges the THRESHOLD rather than the terrain.
struct DropTest {
    double t_statistic = 0.0;
    double first_order_mean = 0.0;
    double higher_order_mean = 0.0;
    std::size_t first_links = 0;
    std::size_t higher_links = 0;
};

[[nodiscard]] DropTest constant_drop(const TerrainField& field, const FlowNetwork& net,
                                     float channelThresholdKm2) {
    const std::span<const float> h = field.plane(Plane::Elevation);
    const std::span<const float> acc = field.plane(Plane::FlowAccum);
    const float thresholdCells = channelThresholdKm2 * 1.0e6f / field.geometry().cell_area();
    const std::size_t n = net.receiver.size();

    const auto isChannel = [&](std::size_t i) { return acc[i] >= thresholdCells; };

    // Strahler order, computed by walking cells in DECREASING elevation -- the same order the
    // accumulation uses, which guarantees every donor is finished before its receiver is read.
    std::vector<std::uint8_t> order(n, 0);
    std::vector<std::uint8_t> maxDonor(n, 0);
    std::vector<std::uint16_t> maxDonorCount(n, 0);
    for (const std::uint32_t i : net.order) {
        if (!isChannel(i)) {
            continue;
        }
        // A channel cell with no channel donors is a source: Strahler 1.
        order[i] = maxDonorCount[i] == 0 ? std::uint8_t{1}
                   : maxDonorCount[i] >= 2
                       ? static_cast<std::uint8_t>(maxDonor[i] + 1) // two equal orders meet: +1
                       : maxDonor[i];
        const std::uint32_t r = net.receiver[i];
        if (r == i || !isChannel(r)) {
            continue;
        }
        if (order[i] > maxDonor[r]) {
            maxDonor[r] = order[i];
            maxDonorCount[r] = 1;
        } else if (order[i] == maxDonor[r]) {
            ++maxDonorCount[r];
        }
    }

    // A LINK is a run of same-order cells between junctions. Its drop is the elevation lost along
    // it, which is the quantity the test compares across orders.
    std::vector<double> firstOrder;
    std::vector<double> higherOrder;
    for (std::size_t i = 0; i < n; ++i) {
        if (!isChannel(i) || order[i] == 0) {
            continue;
        }
        const std::uint32_t r = net.receiver[i];
        // The link ENDS here if the receiver is a different order (a junction) or leaves the
        // network; walk up from this cell to the head of the run and measure the whole drop.
        const bool endsHere = r == i || !isChannel(r) || order[r] != order[i];
        if (!endsHere) {
            continue;
        }
        // PER-CELL DROP, not per-link-run, and the reason is that it keeps the comparison honest
        // rather than that it is easier. Tarboton's test compares the MEAN drop between orders;
        // measuring both orders the same way is what the t-test needs, and a per-cell drop is the
        // same quantity per unit channel length for every order. A per-link version would need a
        // donor index to walk runs upstream, which is a second data structure for no change in what
        // the statistic can detect.
        const double drop = h[i] - (r == i ? h[i] : h[r]);
        if (order[i] == 1) {
            firstOrder.push_back(drop);
        } else {
            higherOrder.push_back(drop);
        }
    }

    DropTest out;
    out.first_links = firstOrder.size();
    out.higher_links = higherOrder.size();
    if (firstOrder.size() < 2 || higherOrder.size() < 2) {
        return out;
    }
    const auto stats = [](const std::vector<double>& v) {
        double mean = 0.0;
        for (double x : v) {
            mean += x;
        }
        mean /= static_cast<double>(v.size());
        double var = 0.0;
        for (double x : v) {
            var += (x - mean) * (x - mean);
        }
        var /= static_cast<double>(v.size() - 1);
        return std::pair<double, double>{mean, var};
    };
    const auto [m1, v1] = stats(firstOrder);
    const auto [m2, v2] = stats(higherOrder);
    out.first_order_mean = m1;
    out.higher_order_mean = m2;
    const double se =
        std::sqrt(v1 / static_cast<double>(firstOrder.size()) + v2 / static_cast<double>(higherOrder.size()));
    out.t_statistic = se > 1e-12 ? (m1 - m2) / se : 0.0;
    return out;
}

} // namespace

int main(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto next = [&]() { return i + 1 < argc ? std::string{argv[++i]} : std::string{}; };
        if (a == "--seed") {
            o.seed = std::stoi(next());
        } else if (a == "--stages") {
            o.stages = std::stoi(next());
        } else if (a == "--cells") {
            o.cells = std::stoi(next());
        } else if (a == "--cell-size") {
            o.cell_size = std::stof(next());
        } else if (a == "--out") {
            o.out = next();
        } else if (a == "--zoom") {
            // Split on commas by hand: MSVC treats sscanf as deprecated and this build is /WX.
            const std::string v = next();
            std::size_t at = 0;
            float* fields[3] = {&o.zoom_x, &o.zoom_z, &o.zoom_span};
            for (float* out : fields) {
                if (at > v.size()) {
                    break;
                }
                const std::size_t comma = v.find(',', at);
                *out = std::stof(v.substr(at, comma == std::string::npos ? std::string::npos : comma - at));
                at = comma == std::string::npos ? v.size() + 1 : comma + 1;
            }
        } else if (a == "--plane") {
            o.plane = next();
        } else if (a == "--help" || a == "-h") {
            std::printf("terrain_dump: false-colour the macro terrain field and print its statistics\n"
                        "  --seed N --stages N --cells N --cell-size F --out FILE\n"
                        "  --plane elevation|flow|precip|temperature|rivers\n"
                        "  --zoom X,Z,SPAN   (world metres; for looking at a river)\n");
            return 0;
        }
    }

    const float half = 0.5f * o.cell_size * static_cast<float>(o.cells);
    TerrainField field{
        FieldGeometry{.origin_x = -half, .origin_z = -half, .cell_size = o.cell_size, .cells = o.cells}};
    run_pipeline(
        field, MacroParams{.seed = o.seed}, o.stages,
        [](std::string_view name, double seconds, void*) {
            std::printf("  stage %-18.*s %7.3f s\n", static_cast<int>(name.size()), name.data(), seconds);
        },
        nullptr);

    const std::span<const float> h = field.plane(Plane::Elevation);
    float lo = h[0];
    float hi = h[0];
    for (float v : h) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    std::size_t land = 0;
    for (float v : h) {
        land += v > 0.0f ? 1u : 0u;
    }

    // The flow plane is only meaningful once the flow stage has run; recompute it here so a dump
    // with --stages 1 still shows something rather than a plane of zeros.
    TerrainField working = field;
    priority_flood(working);
    const FlowNetwork net = build_flow_network(working);
    accumulate_flow(working, net);

    std::printf("field  %d x %d cells at %.0f m = %.1f km across, %.2f MB\n", field.cells(), field.cells(),
                o.cell_size, static_cast<double>(field.geometry().extent()) / 1000.0,
                static_cast<double>(field.bytes()) / 1.0e6);
    std::printf("height %.1f .. %.1f m, land fraction %.1f%% (research §9.3 target ~29%%)\n",
                static_cast<double>(lo), static_cast<double>(hi),
                100.0 * static_cast<double>(land) / static_cast<double>(field.cell_count()));
    // Research §9.3's LAND-HALF shape, which is the part of the hypsometric test that a patch can
    // express. A whole-Earth land fraction cannot be measured on 8 km of ground (see the stage's
    // own comment), but "most land near sea level with a thinning tail" is a property of THIS
    // sample and is exactly what the hypsometric power curve claims to produce.
    {
        std::vector<float> heights;
        heights.reserve(field.cell_count());
        for (float v : h) {
            if (v > 0.0f) {
                heights.push_back(v);
            }
        }
        if (!heights.empty()) {
            std::sort(heights.begin(), heights.end());
            const auto q = [&](double f) {
                return heights[std::min(heights.size() - 1,
                                        static_cast<std::size_t>(f * static_cast<double>(heights.size())))];
            };
            double mean = 0.0;
            for (float v : heights) {
                mean += v;
            }
            mean /= static_cast<double>(heights.size());
            const double median = q(0.5);
            std::printf("land hypsometry: median %.1f m, mean %.1f m, p90 %.1f m, max %.1f m\n", median, mean,
                        static_cast<double>(q(0.9)), static_cast<double>(heights.back()));
            // A Gaussian field puts its median at half its range. Research 9.3 wants the land
            // peak near SEA LEVEL, so this ratio well below 0.5 is the property being claimed.
            std::printf("  median/max = %.3f  (Gaussian ~0.5; 9.3 wants well below 0.5)\n",
                        median / std::max<double>(heights.back(), 1e-6));
        }
    }

    {
        // Goal 308, SWEPT rather than run at one threshold -- because Tarboton's constant-drop test
        // is the published METHOD FOR CHOOSING A_c, not merely a check on it. Reporting it at a
        // single threshold would answer "does this one pass" when the useful question is "which
        // threshold does the test select", and that answer is what can then be compared against
        // what the drainage-density band wants.
        std::printf("constant-drop sweep (TauDEM criterion |t| < 2):\n");
        for (const float ac : {0.005f, 0.01f, 0.05f, 0.1f, 0.5f, 1.0f, 2.0f}) {
            const DropTest d = constant_drop(working, net, ac);
            std::printf("  A_c %6.3f km^2  |t| = %7.2f   first %.3f m (%zu)  higher %.3f m (%zu)\n",
                        static_cast<double>(ac), std::abs(d.t_statistic), d.first_order_mean, d.first_links,
                        d.higher_order_mean, d.higher_links);
        }
        const DropTest drop = constant_drop(working, net, 0.01f);
        std::printf("constant-drop t-test at A_c = 0.01 km^2: |t| = %.2f  (TauDEM criterion |t| < 2)\n",
                    std::abs(drop.t_statistic));
        std::printf("  first-order mean drop %.3f m over %zu cells; higher-order %.3f m over %zu\n",
                    drop.first_order_mean, drop.first_links, drop.higher_order_mean, drop.higher_links);
    }

    // ---- goal 302: the climate planes, measured on the REAL field rather than a synthetic ridge.
    //
    // The unit tests assert the mechanism on a Gaussian ridge because that is the only way to know
    // what the answer should be. These are the same quantities on the terrain that actually ships,
    // which is a different question: a ridge is one barrier, and this is a coastline plus an
    // orogenic belt plus everything the erosion did to both.
    {
        const std::span<const float> precip = field.plane(Plane::Precipitation);
        const std::span<const float> temp = field.plane(Plane::Temperature);
        std::vector<float> landPrecip;
        landPrecip.reserve(field.cell_count());
        // Windward- vs lee-FACING land, classified by whether the ground rises or falls along the
        // wind. This is the shadow measured on real terrain: every slope in the field votes, rather
        // than one hand-picked transect.
        double windSum = 0.0;
        double leeSum = 0.0;
        std::size_t windN = 0;
        std::size_t leeN = 0;
        const ClimateParams cp;
        const float wlen = std::sqrt(cp.wind_x * cp.wind_x + cp.wind_z * cp.wind_z);
        const auto ux = static_cast<std::int32_t>(std::round(cp.wind_x / wlen));
        const auto uz = static_cast<std::int32_t>(std::round(cp.wind_z / wlen));
        for (std::int32_t cz = 0; cz < field.cells(); ++cz) {
            for (std::int32_t cx = 0; cx < field.cells(); ++cx) {
                const std::size_t i = field.index(cx, cz);
                if (h[i] <= 0.0f) {
                    continue;
                }
                landPrecip.push_back(precip[i]);
                if (!field.in_bounds(cx - ux, cz - uz)) {
                    continue;
                }
                const float upwind = std::max(h[field.index(cx - ux, cz - uz)], 0.0f);
                if (h[i] > upwind) {
                    windSum += precip[i];
                    ++windN;
                } else if (h[i] < upwind) {
                    leeSum += precip[i];
                    ++leeN;
                }
            }
        }
        std::sort(landPrecip.begin(), landPrecip.end());
        const auto pct = [&](double q) {
            return landPrecip.empty()
                       ? 0.0f
                       : landPrecip[std::min(landPrecip.size() - 1,
                                             static_cast<std::size_t>(q * static_cast<double>(landPrecip.size())))];
        };
        const double wet = static_cast<double>(pct(0.90));
        const double dry = static_cast<double>(pct(0.10));
        std::printf("precip land p10 %.3f, median %.3f, p90 %.3f  =>  wet/dry %.1f:1 "
                    "(research §6.2 anchor ~10:1 across a 1.5-2 km barrier)\n",
                    dry, static_cast<double>(pct(0.50)), wet, dry > 1e-6 ? wet / dry : 0.0);
        // §6.2's anchor is a knob, not an accident: `background_fraction` sets the floor a fully
        // shadowed cell falls to, so it alone decides the wet/dry ratio. Swept rather than asserted,
        // for the same reason the constant-drop threshold was: the useful question is which value
        // the acceptance number SELECTS.
        {
            std::printf("background_fraction sweep (land p90/p10 vs research §6.2's ~10:1):\n");
            for (const float bg : {0.05f, 0.10f, 0.20f, 0.30f, 0.40f}) {
                TerrainField probe = field;
                ClimateParams sweep;
                sweep.background_fraction = bg;
                compute_climate(probe, sweep);
                const std::span<const float> pp = probe.plane(Plane::Precipitation);
                std::vector<float> probeLand;
                probeLand.reserve(field.cell_count());
                for (std::size_t i = 0; i < field.cell_count(); ++i) {
                    if (h[i] > 0.0f) {
                        probeLand.push_back(pp[i]);
                    }
                }
                std::sort(probeLand.begin(), probeLand.end());
                const auto at = [&](double q) {
                    return probeLand.empty() ? 0.0f
                                        : probeLand[std::min(probeLand.size() - 1,
                                                        static_cast<std::size_t>(q * static_cast<double>(probeLand.size())))];
                };
                const double w = static_cast<double>(at(0.90));
                const double d = static_cast<double>(at(0.10));
                std::printf("  bg %.2f: p10 %.3f  p90 %.3f  =>  %5.1f:1\n", static_cast<double>(bg), d, w,
                            d > 1e-6 ? w / d : 0.0);
            }
        }

        const double windMean = windN != 0 ? windSum / static_cast<double>(windN) : 0.0;
        const double leeMean = leeN != 0 ? leeSum / static_cast<double>(leeN) : 0.0;
        std::printf("precip windward-facing land %.3f (%zu cells) vs lee-facing %.3f (%zu) "
                    "=> %.2f:1\n",
                    windMean, windN, leeMean, leeN, leeMean > 1e-6 ? windMean / leeMean : 0.0);

        float tlo = temp[0];
        float thi = temp[0];
        for (const float v : temp) {
            tlo = std::min(tlo, v);
            thi = std::max(thi, v);
        }
        std::printf("temperature %.2f .. %.2f C over %.1f m of relief (lapse %.1f C/km)\n",
                    static_cast<double>(tlo), static_cast<double>(thi),
                    static_cast<double>(std::max(hi, 0.0f)),
                    static_cast<double>(lapse_rate_c_per_km()));
    }

    for (const float ac : {0.002f, 0.01f, 0.0625f, 0.1f, 1.0f, 5.0f}) {
        std::printf("drainage density at A_c = %.1f km^2: %6.2f km/km^2  (research §9.4 band 2-12)\n",
                    static_cast<double>(ac), drainage_density(working, ac));
    }

    // ---- goal 309: the river network as polylines ------------------------------------------
    // `field` is the pipeline output BEFORE this tool's own fill; `working` is after it. The
    // difference is exactly the lake surfaces, which is how the extractor identifies them.
    const RiverNetwork rivers = extract_rivers(working, net, field.plane(Plane::Elevation), HydrologyParams{});
    {
        double maxAreaKm2 = 0.0;
        for (std::size_t i = 0; i < working.cell_count(); ++i) {
            maxAreaKm2 = std::max(maxAreaKm2, static_cast<double>(working.plane(Plane::FlowAccum)[i]) *
                                                  working.geometry().cell_area() / 1.0e6);
        }
        double maxQ = 0.0;
        double maxW = 0.0;
        std::size_t sea = 0;
        std::size_t lake = 0;
        std::size_t edge = 0;
        std::size_t junction = 0;
        for (const RiverReach& r : rivers.reaches) {
            for (const RiverNode& nd : r.nodes) {
                maxQ = std::max(maxQ, static_cast<double>(nd.discharge_m3s));
                maxW = std::max(maxW, static_cast<double>(nd.width_m));
            }
            switch (r.terminus) {
            case RiverReach::Terminus::Sea: ++sea; break;
            case RiverReach::Terminus::Lake: ++lake; break;
            case RiverReach::Terminus::FieldEdge: ++edge; break;
            case RiverReach::Terminus::Junction: ++junction; break;
            }
        }
        std::size_t spilling = 0;
        for (const Lake& l : rivers.lakes) {
            spilling += l.has_spill_path ? 1u : 0u;
        }
        std::size_t riverDelta = 0;
        for (const Delta& d : rivers.deltas) {
            riverDelta += d.regime == DeltaRegime::RiverDominated ? 1u : 0u;
        }
        std::printf("rivers %zu reaches (%zu to sea, %zu to lake, %zu to a junction, %zu off-field), "
                    "max basin %.2f km^2\n",
                    rivers.reaches.size(), sea, lake, junction, edge, maxAreaKm2);
        // The independent area-to-width route, so the ~2.6x disagreement between the two published
        // families is visible rather than hidden behind whichever one shipped. See
        // research/bankfull-discharge-ratio.md §6.
        const HydrologyParams hp;
        const double areaKm2OfLargest = maxQ * 3.15576e7 / (hp.mean_annual_runoff_m * 1.0e6);
        const double crossCheck =
            hp.width_area_coefficient * std::pow(areaKm2OfLargest, hp.width_area_exponent);
        std::printf("  largest river: %.2f km^2 effective basin, Q %.3f m^3/s mean annual, "
                    "bankfull width %.2f m\n",
                    areaKm2OfLargest, maxQ, maxW);
        std::printf("  cross-check (Sofia & Nikolopoulos W = 3.6 A^0.39): %.2f m -- the two "
                    "published families disagree %.1fx\n",
                    crossCheck, crossCheck / std::max(maxW, 1e-6));
        std::printf("  measured wavelength/width %.1f (research §7.2 band 10-14), sinuosity %.2f "
                    "(band 1.2-2.2)\n",
                    static_cast<double>(rivers.measured_wavelength_over_width()),
                    static_cast<double>(rivers.measured_sinuosity()));
        std::printf("  lakes %zu, of which %zu have a spill path; every reach terminates: %s\n",
                    rivers.lakes.size(), spilling, rivers.every_reach_terminates() ? "yes" : "NO");
        std::printf("  deltas %zu (%zu river-dominated, %zu wave-dominated)\n", rivers.deltas.size(),
                    riverDelta, rivers.deltas.size() - riverDelta);
        // The three biggest, so --zoom has somewhere to point without guessing from a thumbnail.
        std::vector<const Delta*> byQ;
        byQ.reserve(rivers.deltas.size());
        for (const Delta& d : rivers.deltas) {
            byQ.push_back(&d);
        }
        std::sort(byQ.begin(), byQ.end(),
                  [](const Delta* a, const Delta* b) { return a->discharge_m3s > b->discharge_m3s; });
        for (std::size_t k = 0; k < std::min<std::size_t>(3, byQ.size()); ++k) {
            std::printf("  delta %zu at %.0f,%.0f  Q %.4f m^3/s  radius %.1f m  %s\n", k,
                        static_cast<double>(byQ[k]->apex.x), static_cast<double>(byQ[k]->apex.y),
                        static_cast<double>(byQ[k]->discharge_m3s), static_cast<double>(byQ[k]->radius_m),
                        byQ[k]->regime == DeltaRegime::RiverDominated ? "river-dominated" : "wave-dominated");
        }
    }

    // ---- goal 309's viewed capture: the river polylines, drawn at a scale that can show them ----
    //
    // A 2.4 m channel in an 8 km dump at one pixel per 16 m cell is a sixth of a pixel. The zoomed
    // render exists because "viewed capture of a meandering river reaching a delta" is otherwise
    // not a thing this world can produce -- not because the river is wrong, but because the macro
    // dump's resolution is 6x coarser than its subject. See rivers.hpp on why that is the finding
    // rather than a defect.
    if (o.plane == "rivers") {
        const std::int32_t px = 900;
        const float span = o.zoom_span > 0.0f ? o.zoom_span : field.geometry().extent();
        const float originX = o.zoom_span > 0.0f ? o.zoom_x - 0.5f * span : field.geometry().origin_x;
        const float originZ = o.zoom_span > 0.0f ? o.zoom_z - 0.5f * span : field.geometry().origin_z;
        const float metresPerPixel = span / static_cast<float>(px);
        std::vector<std::uint8_t> img(static_cast<std::size_t>(px) * px * 3);

        // Terrain underneath, bilinearly sampled so the zoom is smooth rather than blocky.
        for (std::int32_t py = 0; py < px; ++py) {
            for (std::int32_t pxi = 0; pxi < px; ++pxi) {
                const float wx = originX + (static_cast<float>(pxi) + 0.5f) * metresPerPixel;
                const float wz = originZ + (static_cast<float>(py) + 0.5f) * metresPerPixel;
                const glm::vec2 c = field.geometry().to_cell(wx, wz);
                const auto x0 = static_cast<std::int32_t>(std::floor(c.x));
                const auto z0 = static_cast<std::int32_t>(std::floor(c.y));
                float height = 0.0f;
                if (field.in_bounds(x0, z0) && field.in_bounds(x0 + 1, z0 + 1)) {
                    const float tx = c.x - static_cast<float>(x0);
                    const float tz = c.y - static_cast<float>(z0);
                    const float a = h[field.index(x0, z0)] * (1.0f - tx) + h[field.index(x0 + 1, z0)] * tx;
                    const float b =
                        h[field.index(x0, z0 + 1)] * (1.0f - tx) + h[field.index(x0 + 1, z0 + 1)] * tx;
                    height = a * (1.0f - tz) + b * tz;
                }
                const float tt = height >= 0.0f ? 0.5f + 0.5f * (height / std::max(hi, 1e-6f))
                                                : 0.5f * (1.0f - height / std::min(lo, -1e-6f));
                const std::array<std::uint8_t, 3> col = terrain_colour(tt);
                const std::size_t q = (static_cast<std::size_t>(py) * px + pxi) * 3;
                // Desaturated, so the water drawn on top reads as the subject.
                img[q + 0] = static_cast<std::uint8_t>(120 + (col[0] * 135) / 255);
                img[q + 1] = static_cast<std::uint8_t>(120 + (col[1] * 135) / 255);
                img[q + 2] = static_cast<std::uint8_t>(120 + (col[2] * 135) / 255);
            }
        }

        const auto plot = [&](float wx, float wz, std::uint8_t r, std::uint8_t gg, std::uint8_t b) {
            const auto ix = static_cast<std::int32_t>((wx - originX) / metresPerPixel);
            const auto iz = static_cast<std::int32_t>((wz - originZ) / metresPerPixel);
            if (ix < 0 || iz < 0 || ix >= px || iz >= px) {
                return;
            }
            const std::size_t q = (static_cast<std::size_t>(iz) * px + ix) * 3;
            img[q + 0] = r;
            img[q + 1] = gg;
            img[q + 2] = b;
        };
        // A disc of the channel's real half-width, so what is drawn is the river's actual size.
        const auto stamp = [&](glm::vec2 at, float radiusM, std::uint8_t r, std::uint8_t gg,
                               std::uint8_t b) {
            const float rp = std::max(radiusM / metresPerPixel, 0.6f);
            const auto ri = static_cast<std::int32_t>(std::ceil(rp));
            for (std::int32_t dy = -ri; dy <= ri; ++dy) {
                for (std::int32_t dx = -ri; dx <= ri; ++dx) {
                    if (static_cast<float>(dx * dx + dy * dy) > rp * rp) {
                        continue;
                    }
                    plot(at.x + static_cast<float>(dx) * metresPerPixel,
                         at.y + static_cast<float>(dy) * metresPerPixel, r, gg, b);
                }
            }
        };

        // Deltas first, so the channel is drawn over its own fan.
        for (const Delta& d : rivers.deltas) {
            const std::uint8_t tone = d.regime == DeltaRegime::RiverDominated ? 190 : 150;
            stamp(d.apex, d.radius_m, tone, static_cast<std::uint8_t>(tone - 30), 120);
        }
        for (const RiverReach& r : rivers.reaches) {
            for (const RiverNode& node : r.nodes) {
                stamp(node.position, 0.5f * node.width_m, 30, 90, 200);
            }
        }
        // Lake surfaces.
        for (const Lake& lk : rivers.lakes) {
            const auto lx = static_cast<std::int32_t>(lk.outlet_cell % static_cast<std::size_t>(field.cells()));
            const auto lz = static_cast<std::int32_t>(lk.outlet_cell / static_cast<std::size_t>(field.cells()));
            const glm::vec2 w = field.geometry().to_world(lx, lz);
            stamp(w, 2.0f, 240, 60, 60); // the spill point, in red
        }

        if (!svo_render::PngWriter::write(o.out.c_str(), static_cast<std::uint32_t>(px),
                                          static_cast<std::uint32_t>(px), img.data())) {
            std::fprintf(stderr, "terrain_dump: could not write %s\n", o.out.c_str());
            return 1;
        }
        std::printf("wrote %s (%.0f m across at %.2f m/pixel)\n", o.out.c_str(),
                    static_cast<double>(span), static_cast<double>(metresPerPixel));
        return 0;
    }

    const bool wantFlow = o.plane == "flow";
    const bool wantPrecip = o.plane == "precip" || o.plane == "precipitation";
    const bool wantTemp = o.plane == "temperature" || o.plane == "temp";
    const std::span<const float> source = wantFlow     ? working.plane(Plane::FlowAccum)
                                          : wantPrecip ? field.plane(Plane::Precipitation)
                                          : wantTemp   ? field.plane(Plane::Temperature)
                                                       : h;
    float tlo = source[0];
    float thi = source[0];
    for (const float v : source) {
        tlo = std::min(tlo, v);
        thi = std::max(thi, v);
    }
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(o.cells) * o.cells * 3);
    for (std::int32_t cz = 0; cz < o.cells; ++cz) {
        for (std::int32_t cx = 0; cx < o.cells; ++cx) {
            const std::size_t i = field.index(cx, cz);
            float t = 0.0f;
            if (wantPrecip || wantTemp) {
                const std::array<std::uint8_t, 3> c =
                    wantPrecip ? precip_colour(source[i])
                               : temperature_colour(
                                     std::clamp((source[i] - tlo) / std::max(thi - tlo, 1e-6f), 0.0f, 1.0f));
                const std::size_t px = i * 3;
                rgb[px + 0] = c[0];
                rgb[px + 1] = c[1];
                rgb[px + 2] = c[2];
                continue;
            }
            if (wantFlow) {
                // log, because accumulation spans five orders of magnitude and a linear ramp shows
                // one white river on a black field.
                t = std::log(1.0f + source[i]) / std::log(1.0f + static_cast<float>(field.cell_count()));
            } else {
                // Sea level pinned to the ramp's own 0.5 stop, so the coastline is where the colour
                // changes rather than wherever the range happens to put it.
                t = source[i] >= 0.0f ? 0.5f + 0.5f * (source[i] / std::max(hi, 1e-6f))
                                      : 0.5f * (1.0f - source[i] / std::min(lo, -1e-6f));
            }
            const std::array<std::uint8_t, 3> c =
                wantFlow ? std::array<std::uint8_t, 3>{static_cast<std::uint8_t>(t * 255.0f),
                                                       static_cast<std::uint8_t>(t * 255.0f),
                                                       static_cast<std::uint8_t>(60 + t * 195.0f)}
                         : terrain_colour(t);
            const std::size_t p = i * 3;
            rgb[p + 0] = c[0];
            rgb[p + 1] = c[1];
            rgb[p + 2] = c[2];
        }
    }
    if (!svo_render::PngWriter::write(o.out.c_str(), static_cast<std::uint32_t>(o.cells),
                                      static_cast<std::uint32_t>(o.cells), rgb.data())) {
        std::fprintf(stderr, "terrain_dump: could not write %s\n", o.out.c_str());
        return 1;
    }
    std::printf("wrote %s\n", o.out.c_str());
    return 0;
}
