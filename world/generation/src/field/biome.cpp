// Prompt 006 goal 316. See biome.hpp on why one of Whittaker's two axes does not exist here.

#include <algorithm>
#include <array>
#include <cmath>

#include "world/generation/field/biome.hpp"

namespace world::generation::field {
namespace {

using Def = BiomeDefinition;

// Research Part 6 §1 and §8.1. Every target sits inside the band beside it; where the research
// gives a wide range the target is the mature-stand convergence value (500-800 irrespective of
// latitude, Madrigal-Gonzalez 2023), because a generated world is populated with mature stands --
// there is no successional clock in it.
constexpr std::array<Def, static_cast<std::size_t>(Biome::Count)> kTable{{
    {Biome::Ocean, "ocean", 0.0f, 0.0f, 0.0f, "no stems below sea level"},
    {Biome::Beach, "beach", 0.0f, 0.0f, 0.0f, "Part 6 §1 -- bare sand carries no trees"},
    {Biome::Wetland, "wetland", 120.0f, 50.0f, 300.0f,
     "Part 6 §1 -- waterlogged ground is sparsely treed; below the 180 stems/ha threshold"},
    {Biome::Grassland, "grassland", 25.0f, 10.0f, 100.0f,
     "Part 6 §1 -- crown density ~10-100/ha across the rainfall gradient (Cramer 2017)"},
    {Biome::Shrubland, "shrubland", 60.0f, 0.0f, 200.0f,
     "Part 6 §1 -- mature chaparral carries ~0 LARGE trees/ha; these are the scattered ones"},
    {Biome::TemperateForest, "temperate forest", 550.0f, 400.0f, 700.0f,
     "Part 7 §9.10 / Part 6 §8.1 -- 400-700 [82][83]; Spies & Franklin mature 373-548"},
    {Biome::BorealForest, "boreal forest", 900.0f, 500.0f, 2000.0f,
     "Part 6 §1 -- boreal 500-2,000 at the mature-stand level (Crowther 2015)"},
    {Biome::Alpine, "alpine", 15.0f, 0.0f, 80.0f,
     "Part 6 §1 -- above the treeline belt; scattered krummholz only"},
    {Biome::Desert, "desert", 3.0f, 0.0f, 20.0f,
     "Part 6 §1 -- <200 mm MAP bin, the dry end of the crown-density gradient"},
}};

/// Deterministic per-cell hash, so the graded ecotones dither rather than banding. Same discipline
/// as every other stage: no global RNG, same seed same world.
[[nodiscard]] float hash01(std::int32_t cx, std::int32_t cz, std::uint32_t seed) {
    std::uint32_t h = seed ^ (static_cast<std::uint32_t>(cx) * 0x9e3779b9u) ^
                      (static_cast<std::uint32_t>(cz) * 0x85ebca6bu);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h) / 4294967296.0f;
}

} // namespace

std::span<const BiomeDefinition> biome_table() noexcept {
    return kTable;
}

const BiomeDefinition& biome_def(Biome b) noexcept {
    const auto i = static_cast<std::size_t>(b);
    return kTable[i < kTable.size() ? i : 0];
}

float stems_per_hectare(Biome b) noexcept {
    return biome_def(b).stems_per_hectare;
}

void classify_biomes(TerrainField& out, const BiomeParams& p) {
    const std::span<const float> h = out.plane(Plane::Elevation);
    const std::span<const float> precip = out.plane(Plane::Precipitation);
    const std::span<const float> acc = out.plane(Plane::FlowAccum);
    const std::span<float> biome = out.plane(Plane::Biome);
    const float cell = out.geometry().cell_size;

    for (std::int32_t cz = 0; cz < out.cells(); ++cz) {
        for (std::int32_t cx = 0; cx < out.cells(); ++cx) {
            const std::size_t i = out.index(cx, cz);
            const float elevation = h[i];

            if (elevation <= p.sea_level) {
                biome[i] = static_cast<float>(Biome::Ocean);
                continue;
            }
            if (elevation <= p.sea_level + p.beach_top_m) {
                biome[i] = static_cast<float>(Biome::Beach);
                continue;
            }

            // Local slope, for the wetland test. Central difference where possible; the field edge
            // falls back to flat, which can only over-report wetland on a one-cell border.
            float slope = 0.0f;
            if (cx > 0 && cz > 0 && cx + 1 < out.cells() && cz + 1 < out.cells()) {
                const float dx = (h[out.index(cx + 1, cz)] - h[out.index(cx - 1, cz)]) / (2.0f * cell);
                const float dz = (h[out.index(cx, cz + 1)] - h[out.index(cx, cz - 1)]) / (2.0f * cell);
                slope = std::sqrt(dx * dx + dz * dz);
            }

            // WETLAND IS A FEEDBACK BOUNDARY, not a climate one: ground is either waterlogged or it
            // is not, so this snaps rather than grading. §6.4's rule that edge sharpness is a
            // function of CAUSE.
            if (acc[i] >= p.wetland_accum_cells && slope <= p.wetland_max_slope) {
                biome[i] = static_cast<float>(Biome::Wetland);
                continue;
            }

            // ALPINE is the altitude belt -- the only surviving trace of Whittaker's temperature
            // axis on this world (see the header). Also a feedback boundary: the treeline is sharp.
            if (elevation >= p.alpine_base_m) {
                biome[i] = static_cast<float>(Biome::Alpine);
                continue;
            }

            // The moisture axis, GRADED. §6.4: climate boundaries blend, so a cell within
            // `ecotone_softness` of a threshold is assigned probabilistically by a deterministic
            // hash -- which dithers the boundary into a mixed band instead of drawing a contour
            // line. The width of that band is the ecotone.
            const float m = precip[i] + p.ecotone_softness * (hash01(cx, cz, p.seed) - 0.5f) * 2.0f;
            Biome chosen = Biome::Grassland;
            if (m < p.desert_precip) {
                chosen = Biome::Desert;
            } else if (m < p.shrub_precip) {
                chosen = Biome::Grassland;
            } else if (m < p.forest_precip) {
                chosen = Biome::Shrubland;
            } else {
                // BOREAL vs TEMPERATE would be the temperature axis's job. With half a degree of
                // range across the world there is nothing to split on, so this world grows
                // temperate forest and the boreal class exists for a world with real relief.
                chosen = Biome::TemperateForest;
            }
            biome[i] = static_cast<float>(chosen);
        }
    }
}

float mean_land_stem_density(const TerrainField& field, float seaLevel) {
    const std::span<const float> h = field.plane(Plane::Elevation);
    const std::span<const float> biome = field.plane(Plane::Biome);
    double sum = 0.0;
    std::size_t land = 0;
    for (std::size_t i = 0; i < field.cell_count(); ++i) {
        if (h[i] <= seaLevel) {
            continue;
        }
        ++land;
        const auto b = static_cast<std::uint8_t>(std::lround(biome[i]));
        sum += static_cast<double>(stems_per_hectare(
            b < static_cast<std::uint8_t>(Biome::Count) ? static_cast<Biome>(b) : Biome::Grassland));
    }
    return land == 0 ? 0.0f : static_cast<float>(sum / static_cast<double>(land));
}

} // namespace world::generation::field
