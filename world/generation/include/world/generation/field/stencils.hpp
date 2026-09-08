#pragma once

// Prompt 006 Group AM-C goals 310-314: the stencil passes.
//
// Research Part 7 §10.1 draws the line these live on: some of this is meant to be a "convincing
// costume" rather than the real thing, and "a stencil that passes the cross-section test is a
// success, not a compromise". Each function below is a stamp validated by a statistic, not a
// simulation.
//
// EVERY ONE OF THEM IS TESTED ON A SYNTHETIC FIELD THAT HAS ITS SUBJECT, and then measured on the
// shipped world. That split is not ceremony: this world is a gentle coastal plain with 112 m of
// relief and a 0.50 °C temperature range, so some of these stencils have NO SUBJECT in it. A
// stencil tested only against the shipped world would be reported as broken when it is merely
// unemployed, and the honest form is a working stencil plus a measured negative.

#include <cstdint>
#include <vector>

#include "world/generation/field/biome.hpp"
#include "world/generation/field/fluvial.hpp"
#include "world/generation/field/terrain_field.hpp"

namespace world::generation::field {

// ---------------------------------------------------------------------------------------- 310

struct GlacialParams {
    /// Elevation above which ice forms, metres. Research Part 3 §3's glacial buzzsaw ties peak
    /// heights to the snowline; §10.1 says to select basins above the CLIMATE FIELD's snowline,
    /// which is what `snowline_from_climate` computes.
    float snowline_m = 1200.0f;
    /// Parabolic exponent for the carved cross-section: y = a x^b. Research §9.5's glacial band is
    /// b = 1.5-2.0 (Graf's Beartooth), Svensson's Lapporten 2.0-2.2.
    float cross_section_b = 1.8f;
    /// Trough width per unit sqrt(contributing area) -- the ice-flux proxy §10.1 names.
    float width_per_sqrt_area = 0.6f;
    /// Overdeepening below the local base level at confluences, metres.
    float overdeepen_m = 15.0f;
    float sea_level = 0.0f;
};

struct GlacialResult {
    std::size_t basins = 0; ///< basins found above the snowline
    std::size_t carved_cells = 0;
    float highest_m = 0.0f; ///< the field's own maximum, so a negative result states why
};

/// The elevation at which the temperature plane crosses freezing. Returns +infinity when the world
/// never freezes -- which is this world's answer, and the reason goal 310 reports a negative.
[[nodiscard]] float snowline_from_climate(const TerrainField& field, float freezingC = 0.0f);

/// Carves parabolic troughs along the flow lines of basins above the snowline.
[[nodiscard]] GlacialResult carve_glacial_valleys(TerrainField& out, const FlowNetwork& net,
                                                  const GlacialParams& p = {});

// ---------------------------------------------------------------------------------------- 311

struct CoastalParams {
    float sea_level = 0.0f;
    /// A coast steeper than this retreats as a CLIFF with a shore platform; gentler coasts
    /// accumulate a beach. Research Part 3 §6-§7's two depositional/erosional regimes, split on
    /// the one quantity the field has.
    float cliff_slope = 0.35f;
    /// Shore platform: the near-horizontal bench cut at wave base, extending seaward.
    float platform_width_m = 40.0f;
    float platform_depth_m = 1.5f;
    /// Beach: the wedge deposited on a gentle coast, landward of the waterline.
    float beach_width_m = 30.0f;
};

struct CoastalResult {
    std::size_t coast_cells = 0;
    std::size_t cliffs = 0;
    std::size_t beaches = 0;
};

/// Cuts shore platforms below cliffed coasts and lays beach wedges on gentle ones.
///
/// The MATERIAL is not chosen here. `world/materials`' `TerrainBands::beach_band` already owns
/// "what is a beach made of", and goal 311's Check requires that to stay true -- this stage moves
/// geometry so that the existing band lands where a beach should be, and nothing else.
[[nodiscard]] CoastalResult carve_coastline(TerrainField& out, const CoastalParams& p = {});

// ---------------------------------------------------------------------------------------- 312

struct KarstParams {
    /// Dolines are cut only where the lithology plane says carbonate.
    float carbonate_lithology = 2.0f;
    /// Research Part 4 §8. Doline diameters are log-normal over a wide range; these bracket the
    /// band the generator targets, in metres.
    float diameter_min_m = 20.0f;
    float diameter_max_m = 120.0f;
    /// Dolines per square kilometre. The research's karst-plateau densities run to the tens per
    /// km²; this is the target and `KarstResult` reports what was achieved.
    float density_per_km2 = 25.0f;
    float depth_over_diameter = 0.12f;
    std::uint32_t seed = 1337;
};

struct KarstResult {
    std::size_t dolines = 0;
    float density_per_km2 = 0.0f;
    float mean_diameter_m = 0.0f;
    /// Dolines are DRAINAGE SINKS, which is exactly what acceptance test 9 forbids. True when the
    /// stage re-ran the fill afterwards, which is the answer goal 312's Check demands be stated.
    bool refilled = false;
};

[[nodiscard]] KarstResult stamp_karst(TerrainField& out, const KarstParams& p = {});

// ---------------------------------------------------------------------------------------- 314

/// Research Part 5 §2's dune-morphology phase diagram: the two axes are SAND SUPPLY and WIND
/// DIRECTIONAL VARIABILITY, and the morphology is selected by both rather than one type everywhere.
enum class DuneType : std::uint8_t {
    None,
    Barchan,         ///< low supply, unidirectional wind: crescents with horns downwind
    TransverseRidge, ///< high supply, unidirectional: ridges across the wind
    Linear,          ///< bidirectional wind regime: seif ridges along the resultant
    Star,            ///< multidirectional wind: radiating arms, the highest dunes
};

struct DuneParams {
    /// Wind directional variability, 0 = one direction all year, 1 = fully multidirectional. This
    /// is the phase diagram's x-axis and there is no wind-rose model here to derive it from, so it
    /// is supplied and varied spatially by noise.
    float directional_variability = 0.25f;
    /// Sand supply proxy. Deserts downwind of a long dry fetch have more.
    float supply = 0.5f;
    /// Research Part 5 §3. Dune wavelength and height bands for the chosen morphology, metres.
    float wavelength_min_m = 50.0f;
    float wavelength_max_m = 500.0f;
    float height_over_wavelength = 0.06f;
    std::uint32_t seed = 1337;
};

struct DuneResult {
    std::size_t desert_cells = 0;
    std::size_t dune_cells = 0;
    float mean_wavelength_m = 0.0f;
    float mean_height_m = 0.0f;
    std::size_t by_type[5] = {};
};

/// Chooses a morphology per cell from the phase diagram and stamps the corresponding dune field
/// wherever the biome plane says desert.
[[nodiscard]] DuneResult stamp_dunes(TerrainField& out, const DuneParams& p = {});

/// The phase diagram itself, exposed so a test can assert it selects rather than collapsing to one
/// answer. Research Part 5 §2.
[[nodiscard]] DuneType dune_morphology(float supply, float directionalVariability) noexcept;

} // namespace world::generation::field
