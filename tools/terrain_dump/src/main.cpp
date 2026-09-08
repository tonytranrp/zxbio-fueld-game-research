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
#include <vector>

#include "png_writer.hpp"
#include "world/generation/field/fluvial.hpp"
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
        } else if (a == "--plane") {
            o.plane = next();
        } else if (a == "--help" || a == "-h") {
            std::printf("terrain_dump: false-colour the macro terrain field and print its statistics\n"
                        "  --seed N --stages N --cells N --cell-size F --plane elevation|flow --out FILE\n");
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
            std::printf("land hypsometry: median %.1f m, mean %.1f m, p90 %.1f m, max %.1f m\n",
                        median, mean, static_cast<double>(q(0.9)), static_cast<double>(heights.back()));
            // A Gaussian field puts its median at half its range. Research 9.3 wants the land
            // peak near SEA LEVEL, so this ratio well below 0.5 is the property being claimed.
            std::printf("  median/max = %.3f  (Gaussian ~0.5; 9.3 wants well below 0.5)\n",
                        median / std::max<double>(heights.back(), 1e-6));
        }
    }

    for (const float ac : {0.002f, 0.0625f, 0.1f, 1.0f, 5.0f}) {
        std::printf("drainage density at A_c = %.1f km^2: %6.2f km/km^2  (research §9.4 band 2-12)\n",
                    static_cast<double>(ac), drainage_density(working, ac));
    }

    const bool wantFlow = o.plane == "flow";
    const std::span<const float> source = wantFlow ? working.plane(Plane::FlowAccum) : h;
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(o.cells) * o.cells * 3);
    for (std::int32_t cz = 0; cz < o.cells; ++cz) {
        for (std::int32_t cx = 0; cx < o.cells; ++cx) {
            const std::size_t i = field.index(cx, cz);
            float t = 0.0f;
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
