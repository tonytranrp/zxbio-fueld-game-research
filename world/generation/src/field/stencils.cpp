// Prompt 006 Group AM-C goals 310-314. See stencils.hpp for the "convincing costume" framing.

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include "world/generation/field/stencils.hpp"

namespace world::generation::field {
namespace {

constexpr float kTwoPi = 6.28318530718f;

[[nodiscard]] float hash01(std::int32_t a, std::int32_t b, std::uint32_t seed) {
    std::uint32_t h =
        seed ^ (static_cast<std::uint32_t>(a) * 0x9e3779b9u) ^ (static_cast<std::uint32_t>(b) * 0x85ebca6bu);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h) / 4294967296.0f;
}

/// Slope magnitude at a cell, metres per metre.
[[nodiscard]] float slope_at(const TerrainField& f, std::span<const float> h, std::int32_t cx,
                             std::int32_t cz) {
    if (cx <= 0 || cz <= 0 || cx + 1 >= f.cells() || cz + 1 >= f.cells()) {
        return 0.0f;
    }
    const float d = 2.0f * f.geometry().cell_size;
    const float dx = (h[f.index(cx + 1, cz)] - h[f.index(cx - 1, cz)]) / d;
    const float dz = (h[f.index(cx, cz + 1)] - h[f.index(cx, cz - 1)]) / d;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

// ---------------------------------------------------------------------------------------- 310

float snowline_from_climate(const TerrainField& field, float freezingC) {
    const std::span<const float> h = field.plane(Plane::Elevation);
    const std::span<const float> t = field.plane(Plane::Temperature);
    // The elevation at which temperature crosses freezing, found by fitting the lapse relation the
    // climate stage actually applied rather than assuming it: T = T0 - lapse * z, so the crossing
    // is z = (T0 - freezing) / lapse. Two samples at different elevations determine both.
    std::size_t low = field.cell_count();
    std::size_t high = field.cell_count();
    for (std::size_t i = 0; i < field.cell_count(); ++i) {
        if (h[i] <= 0.0f) {
            continue;
        }
        if (high == field.cell_count() || h[i] > h[high]) {
            high = i;
        }
        if (low == field.cell_count() || h[i] < h[low]) {
            low = i;
        }
    }
    if (high == field.cell_count() || h[high] - h[low] < 1e-3f) {
        return std::numeric_limits<float>::infinity();
    }
    const float lapse = (t[low] - t[high]) / (h[high] - h[low]); // °C per metre
    if (lapse <= 1e-9f) {
        return std::numeric_limits<float>::infinity();
    }
    return h[low] + (t[low] - freezingC) / lapse;
}

GlacialResult carve_glacial_valleys(TerrainField& out, const FlowNetwork& net, const GlacialParams& p) {
    GlacialResult result;
    const std::span<float> h = out.plane(Plane::Elevation);
    const std::span<const float> acc = out.plane(Plane::FlowAccum);
    const float cell = out.geometry().cell_size;
    for (const float v : h) {
        result.highest_m = std::max(result.highest_m, v);
    }

    // Every cell above the snowline seeds ice. Its trough width scales with the ice-flux proxy
    // (sqrt of contributing area) per §10.1, and the cross-section is the power law §9.5 measures.
    std::vector<float> carved(h.begin(), h.end());
    for (std::int32_t cz = 0; cz < out.cells(); ++cz) {
        for (std::int32_t cx = 0; cx < out.cells(); ++cx) {
            const std::size_t i = out.index(cx, cz);
            if (h[i] < p.snowline_m) {
                continue;
            }
            ++result.basins;
            const float halfWidth = p.width_per_sqrt_area * std::sqrt(std::max(acc[i], 1.0f)) * cell;
            const auto reach = static_cast<std::int32_t>(std::ceil(halfWidth / cell));
            if (reach < 1) {
                continue;
            }
            // The floor: overdeepened below the local surface, which is what makes a U rather than
            // a widened V. Confluences carry more ice, so the overdeepening scales with the flux
            // proxy too -- §10.1's "overdeepen below base level at confluences".
            const float floorZ =
                h[i] - p.overdeepen_m * std::min(1.0f, std::sqrt(std::max(acc[i], 1.0f)) / 40.0f);
            for (std::int32_t dz = -reach; dz <= reach; ++dz) {
                for (std::int32_t dx = -reach; dx <= reach; ++dx) {
                    if (!out.in_bounds(cx + dx, cz + dz)) {
                        continue;
                    }
                    const float r = std::sqrt(static_cast<float>(dx * dx + dz * dz)) * cell;
                    if (r > halfWidth) {
                        continue;
                    }
                    // y = a x^b, normalised so the wall meets the untouched surface at halfWidth.
                    const float u = r / std::max(halfWidth, 1e-3f);
                    const float profile = std::pow(u, p.cross_section_b);
                    const std::size_t j = out.index(cx + dx, cz + dz);
                    const float target = floorZ + (h[j] - floorZ) * profile;
                    if (target < carved[j]) {
                        carved[j] = target;
                    }
                }
            }
        }
    }
    for (std::size_t i = 0; i < out.cell_count(); ++i) {
        if (carved[i] < h[i] - 1e-4f) {
            ++result.carved_cells;
        }
        h[i] = std::max(carved[i], p.sea_level - 200.0f);
    }
    (void)net;
    return result;
}

// ---------------------------------------------------------------------------------------- 311

CoastalResult carve_coastline(TerrainField& out, const CoastalParams& p) {
    CoastalResult result;
    const std::span<float> h = out.plane(Plane::Elevation);
    const std::vector<float> original(h.begin(), h.end());
    const float cell = out.geometry().cell_size;
    const auto platformCells = static_cast<std::int32_t>(std::ceil(p.platform_width_m / cell));
    const auto beachCells = static_cast<std::int32_t>(std::ceil(p.beach_width_m / cell));

    for (std::int32_t cz = 1; cz + 1 < out.cells(); ++cz) {
        for (std::int32_t cx = 1; cx + 1 < out.cells(); ++cx) {
            const std::size_t i = out.index(cx, cz);
            if (original[i] <= p.sea_level) {
                continue;
            }
            // A coast cell is land with at least one submarine neighbour.
            bool coast = false;
            for (std::int32_t dz = -1; dz <= 1 && !coast; ++dz) {
                for (std::int32_t dx = -1; dx <= 1 && !coast; ++dx) {
                    coast = original[out.index(cx + dx, cz + dz)] <= p.sea_level;
                }
            }
            if (!coast) {
                continue;
            }
            ++result.coast_cells;

            const float s = slope_at(out, original, cx, cz);
            if (s >= p.cliff_slope) {
                // CLIFF: cut a near-horizontal shore platform seaward at wave base. The platform is
                // the diagnostic feature -- a cliff without one is just a steep slope.
                ++result.cliffs;
                for (std::int32_t dz = -platformCells; dz <= platformCells; ++dz) {
                    for (std::int32_t dx = -platformCells; dx <= platformCells; ++dx) {
                        if (!out.in_bounds(cx + dx, cz + dz)) {
                            continue;
                        }
                        const std::size_t j = out.index(cx + dx, cz + dz);
                        if (original[j] > p.sea_level) {
                            continue; // landward of the waterline: the cliff face stays
                        }
                        const float r = std::sqrt(static_cast<float>(dx * dx + dz * dz)) * cell;
                        if (r > p.platform_width_m) {
                            continue;
                        }
                        // Flat bench just below the water, grading into the natural seabed at the
                        // platform's outer edge.
                        const float t = r / p.platform_width_m;
                        const float bench = p.sea_level - p.platform_depth_m;
                        h[j] = std::max(h[j], bench * (1.0f - t) + original[j] * t);
                    }
                }
            } else {
                // GENTLE COAST: deposit a beach wedge landward, so the material band that already
                // exists in world/materials lands on a real depositional form rather than on
                // whatever the erosion happened to leave.
                ++result.beaches;
                for (std::int32_t dz = -beachCells; dz <= beachCells; ++dz) {
                    for (std::int32_t dx = -beachCells; dx <= beachCells; ++dx) {
                        if (!out.in_bounds(cx + dx, cz + dz)) {
                            continue;
                        }
                        const std::size_t j = out.index(cx + dx, cz + dz);
                        if (original[j] <= p.sea_level) {
                            continue;
                        }
                        const float r = std::sqrt(static_cast<float>(dx * dx + dz * dz)) * cell;
                        if (r > p.beach_width_m) {
                            continue;
                        }
                        // Pull the profile toward a low, near-flat wedge: a beach is a smoothing,
                        // not a pile.
                        const float t = r / p.beach_width_m;
                        const float wedge = p.sea_level + 1.2f + 0.8f * t;
                        h[j] = h[j] * t + std::min(h[j], wedge) * (1.0f - t);
                    }
                }
            }
        }
    }
    return result;
}

// ---------------------------------------------------------------------------------------- 312

KarstResult stamp_karst(TerrainField& out, const KarstParams& p) {
    KarstResult result;
    const std::span<float> h = out.plane(Plane::Elevation);
    const std::span<const float> lithology = out.plane(Plane::Lithology);
    const float cell = out.geometry().cell_size;
    const float areaKm2 = static_cast<float>(out.cell_count()) * cell * cell / 1.0e6f;

    // Candidate centres on a jittered grid whose spacing gives the target density. A jittered grid
    // rather than rejection sampling because it is O(n) and deterministic, and because real doline
    // fields are clustered but not Poisson -- §8's "clustered noise".
    const float spacing = std::sqrt(1.0e6f / std::max(p.density_per_km2, 0.01f));
    const auto step = std::max(1, static_cast<std::int32_t>(spacing / cell));
    double diameterSum = 0.0;

    for (std::int32_t gz = 0; gz < out.cells(); gz += step) {
        for (std::int32_t gx = 0; gx < out.cells(); gx += step) {
            const float jx = hash01(gx, gz, p.seed);
            const float jz = hash01(gx, gz, p.seed + 7u);
            const auto cx =
                static_cast<std::int32_t>(static_cast<float>(gx) + (jx - 0.5f) * static_cast<float>(step));
            const auto cz =
                static_cast<std::int32_t>(static_cast<float>(gz) + (jz - 0.5f) * static_cast<float>(step));
            if (!out.in_bounds(cx, cz)) {
                continue;
            }
            const std::size_t i = out.index(cx, cz);
            // ON A CARBONATE MASK ONLY. Dolines are dissolution features; putting them on granite
            // would be the "one texture everywhere" failure §10.1 warns about.
            if (std::abs(lithology[i] - p.carbonate_lithology) > 0.5f || h[i] <= 0.0f) {
                continue;
            }
            // Log-normal-ish diameter across the band, per §8's size distribution.
            const float u = hash01(cx, cz, p.seed + 31u);
            const float diameter = p.diameter_min_m * std::pow(p.diameter_max_m / p.diameter_min_m, u * u);
            const float radius = 0.5f * diameter;
            const float depth = diameter * p.depth_over_diameter;
            const auto reach = static_cast<std::int32_t>(std::ceil(radius / cell));
            for (std::int32_t dz = -reach; dz <= reach; ++dz) {
                for (std::int32_t dx = -reach; dx <= reach; ++dx) {
                    if (!out.in_bounds(cx + dx, cz + dz)) {
                        continue;
                    }
                    const float r = std::sqrt(static_cast<float>(dx * dx + dz * dz)) * cell;
                    if (r > radius) {
                        continue;
                    }
                    // A doline is a smooth closed bowl, which is why it is a drainage sink.
                    const float t = r / std::max(radius, 1e-3f);
                    h[out.index(cx + dx, cz + dz)] -= depth * (1.0f - t * t);
                }
            }
            ++result.dolines;
            diameterSum += diameter;
        }
    }
    result.density_per_km2 = static_cast<float>(result.dolines) / std::max(areaKm2, 1e-6f);
    result.mean_diameter_m =
        result.dolines == 0 ? 0.0f : static_cast<float>(diameterSum / static_cast<double>(result.dolines));

    // GOAL 312'S CHECK DEMANDS THIS BE STATED: dolines are drainage sinks, and acceptance test 9
    // forbids internal basins. The answer taken here is to RE-RUN THE FILL, not to exempt them.
    //
    // Re-running is the choice because a locally exempted sink would have to be carried through
    // every downstream consumer -- the flow router, the river extractor, the coherence metric --
    // as a special case that means "this basin is allowed". Filling them means a doline holds
    // water: it becomes a shallow closed depression brimming to its rim, which is what a doline
    // with a blocked throat actually is, and it costs one O(n) pass.
    if (result.dolines > 0) {
        priority_flood(out);
        result.refilled = true;
    }
    return result;
}

// ---------------------------------------------------------------------------------------- 314

DuneType dune_morphology(float supply, float directionalVariability) noexcept {
    // Research Part 5 §2's phase diagram, read as a 2x2 with the star field in the
    // high-variability corner:
    //
    //                    low supply            high supply
    //   unidirectional   barchan               transverse ridge
    //   bidirectional    linear (seif)         linear (seif)
    //   multidirectional star                  star
    if (directionalVariability > 0.66f) {
        return DuneType::Star;
    }
    if (directionalVariability > 0.33f) {
        return DuneType::Linear;
    }
    return supply > 0.5f ? DuneType::TransverseRidge : DuneType::Barchan;
}

DuneResult stamp_dunes(TerrainField& out, const DuneParams& p) {
    DuneResult result;
    const std::span<float> h = out.plane(Plane::Elevation);
    const std::span<const float> biome = out.plane(Plane::Biome);
    const float cell = out.geometry().cell_size;
    double wavelengthSum = 0.0;
    double heightSum = 0.0;

    for (std::int32_t cz = 0; cz < out.cells(); ++cz) {
        for (std::int32_t cx = 0; cx < out.cells(); ++cx) {
            const std::size_t i = out.index(cx, cz);
            if (static_cast<std::uint8_t>(std::lround(biome[i])) !=
                static_cast<std::uint8_t>(Biome::Desert)) {
                continue;
            }
            ++result.desert_cells;

            // Both phase-diagram axes vary spatially, so the morphology map varies with them --
            // goal 314's Check asks to see two different dune types in two different places, and a
            // constant on either axis would give one type everywhere.
            const float vary =
                std::clamp(p.directional_variability + 0.5f * (hash01(cx / 24, cz / 24, p.seed + 3u) - 0.4f),
                           0.0f, 1.0f);
            const float supply =
                std::clamp(p.supply + 0.6f * (hash01(cx / 31, cz / 31, p.seed + 5u) - 0.4f), 0.0f, 1.0f);
            const DuneType type = dune_morphology(supply, vary);
            ++result.by_type[static_cast<std::size_t>(type)];

            // Wavelength grows with supply across the research's band; height follows the
            // height/wavelength ratio §3 reports.
            const float wavelength = p.wavelength_min_m + (p.wavelength_max_m - p.wavelength_min_m) * supply;
            const float height = wavelength * p.height_over_wavelength;
            const float wx = static_cast<float>(cx) * cell;
            const float wz = static_cast<float>(cz) * cell;

            float profile = 0.0f;
            switch (type) {
            case DuneType::TransverseRidge:
                // Ridges ACROSS the wind: one sinusoid along the wind direction.
                profile = std::sin(kTwoPi * wx / wavelength);
                break;
            case DuneType::Linear:
                // Seifs ALONG the resultant: the ridge axis is rotated 45 degrees here, which is
                // what makes them visibly different from transverse ridges in a dump.
                profile = std::sin(kTwoPi * (wx + wz) * 0.7071f / wavelength);
                break;
            case DuneType::Star:
                // Radiating arms: two crossed systems, which produces isolated peaks rather than
                // ridges. Star dunes are the tallest, so the amplitude is boosted.
                profile = 1.4f * std::sin(kTwoPi * wx / wavelength) * std::sin(kTwoPi * wz / wavelength);
                break;
            case DuneType::Barchan:
            default: {
                // Crescents: isolated, so the field is mostly flat with occasional dunes. The
                // asymmetric horn shape comes from clamping the negative half away.
                const float s =
                    std::sin(kTwoPi * wx / wavelength) * std::sin(kTwoPi * wz / (wavelength * 1.7f));
                profile = std::max(s, 0.0f) * 1.2f - 0.15f;
                break;
            }
            }
            h[i] += height * profile;
            ++result.dune_cells;
            wavelengthSum += wavelength;
            heightSum += height;
        }
    }
    if (result.dune_cells != 0) {
        result.mean_wavelength_m = static_cast<float>(wavelengthSum / static_cast<double>(result.dune_cells));
        result.mean_height_m = static_cast<float>(heightSum / static_cast<double>(result.dune_cells));
    }
    return result;
}

} // namespace world::generation::field
