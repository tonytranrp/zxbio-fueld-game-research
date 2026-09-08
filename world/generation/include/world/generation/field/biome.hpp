#pragma once

// Prompt 006 goal 316: the biome field. Research Part 7 §6.1 (Whittaker placement), §6.4 (ecotone
// sharpness), Part 6 §1 (stem density by biome) and §8.1 (the acceptance band).
//
// THE WHITTAKER DIAGRAM'S TEMPERATURE AXIS DOES NOT EXIST ON THIS WORLD, and that is stated here
// rather than discovered by whoever reads the biome map and wonders why it looks like a rainfall
// map. §6.1 classifies biomes on mean annual precipitation against mean annual temperature. Goal
// 302 measured this world's temperature range: **13.50 to 14.00 °C -- half a degree**, because
// 112 m of relief at 6.5 °C/km cannot produce more and 8 km of latitude is 0.07 degrees of arc.
//
// So one of Whittaker's two axes is a constant here. The classifier below uses it anyway -- the
// elevation belt it produces is real, just narrow -- but the discriminating axis is precipitation,
// and the biome map is honestly a moisture map with a thin alpine belt on top. Raising the world's
// relief is what would restore the second axis; it is an open goal, not a defect in this stage.
//
// What §6.4 adds and this stage implements: ecotone sharpness is a function of CAUSE. Climate
// gradients grade; feedback-maintained boundaries (fire/grass, treeline) are abrupt -- the
// forest-savanna wall averages 10 m. So the classifier blends smoothly on moisture and snaps at
// the two boundaries the research names as feedback-maintained.

#include <cstdint>
#include <span>
#include <string_view>

#include "world/generation/field/terrain_field.hpp"

namespace world::generation::field {

enum class Biome : std::uint8_t {
    Ocean,
    Beach,
    Wetland,
    Grassland,
    Shrubland,
    TemperateForest,
    BorealForest,
    Alpine,
    Desert,
    Count,
};

/// Per-biome stem density, with the research band it came from.
///
/// `stems_per_hectare` is the value this generator targets; `band_lo`/`band_hi` are the measured
/// range the research reports, kept beside it so a target can be argued with. Research Part 6 §1:
///
///   * temperate 200-1,000, and §8.1's acceptance band is **400-700** -- Spies & Franklin's
///     Pacific-Northwest Douglas-fir gives young 758-1,154, mature 373-548, old-growth 394-511
///   * boreal 500-2,000 at the mature-stand level, dense young regeneration 5,000-10,000+
///   * high-productivity mature stands converge on **500-800 everywhere irrespective of latitude**
///     (Madrigal-Gonzalez 2023, 3,000+ plots) -- which is why the forest targets sit there
///   * shrubland/chaparral is effectively 0 LARGE trees per hectare; its woody stems are shrubs
///   * the dry end: crown density spans ~10-100 crowns/ha from <200 mm to 1,400 mm MAP
struct BiomeDefinition {
    Biome biome;
    std::string_view name;
    float stems_per_hectare;
    float band_lo;
    float band_hi;
    std::string_view citation;
};

[[nodiscard]] std::span<const BiomeDefinition> biome_table() noexcept;
[[nodiscard]] const BiomeDefinition& biome_def(Biome b) noexcept;
[[nodiscard]] float stems_per_hectare(Biome b) noexcept;

struct BiomeParams {
    float sea_level = 0.0f;
    /// Beach band, metres above sea level. Matches `world/materials`' own `TerrainBands` rather
    /// than introducing a second coastline rule.
    float beach_top_m = 2.0f;
    /// Above this the alpine belt begins, metres. §6.1's "altitude belts" -- and on this world it
    /// is the only part of the temperature axis that discriminates anything.
    float alpine_base_m = 60.0f;
    /// Dimensionless precipitation (the plane is normalised to a field mean of 1.0) below which
    /// land is desert and above which it is forest. The two thresholds bracket grass and shrub.
    float desert_precip = 0.45f;
    float shrub_precip = 0.75f;
    float forest_precip = 1.05f;
    /// A cell whose upstream contributing area exceeds this AND which is nearly flat is wetland.
    float wetland_accum_cells = 400.0f;
    float wetland_max_slope = 0.02f;
    /// §6.4: climate boundaries grade, feedback-maintained ones snap. This is the width of the
    /// graded blend in units of the precipitation threshold, and the snap boundaries ignore it.
    float ecotone_softness = 0.12f;
    std::uint32_t seed = 1337;
};

/// Fills `Plane::Biome` from elevation, precipitation, temperature and flow accumulation.
///
/// Requires `accumulate_flow` to have run if wetlands are to be found; without it every cell reads
/// zero contributing area and the wetland class is simply empty, which is a visible absence rather
/// than a wrong answer.
void classify_biomes(TerrainField& out, const BiomeParams& p = {});

/// Mean stems per hectare over LAND, weighted by the biome areas actually produced -- what
/// acceptance test 10 measures.
[[nodiscard]] float mean_land_stem_density(const TerrainField& field, float seaLevel = 0.0f);

} // namespace world::generation::field
