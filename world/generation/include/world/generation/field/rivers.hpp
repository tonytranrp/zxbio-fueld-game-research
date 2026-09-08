#pragma once

// Prompt 006 goal 309: the fluvial post-passes -- meanders, base level, lakes and deltas.
// Research Part 2 §7 (meandering), §8 (longitudinal profile and base level), §9 (deltas), and
// Part 7 §11(7) which lists these as post-passes on the CENTRELINES rather than on the heightfield.
//
// WHY THIS IS A POLYLINE MODULE AND NOT A HEIGHTFIELD STAGE. Measured on the shipped field before a
// line of it was written: the largest river here is a few metres wide against a 16 m macro cell.
// A channel is a SUB-CELL feature of the macro field, and a meander wavelength of 10-14 W is a
// handful of cells. Nothing about a river on this world can be expressed by moving macro heights
// around -- the research says as much ("route on the macro-grid then smooth/meander the centreline
// polyline"), and the alternative would be a heightfield operation whose own output resolution
// cannot represent its own subject. So the network is extracted as polylines carrying width and
// discharge, meandered in continuous space, and handed to the detail layer to render at voxel
// resolution.
//
// The numbers the polylines carry are the research's, and each says which:
//   * discharge from PRECIPITATION-weighted accumulation, so goal 302's orographic field actually
//     decides which basins carry water (Part 2 §11)
//   * bankfull width W = a Q^0.5 with a in 3.0-4.0 (Part 2 §1.3's anchor check: Bray 3.83, Nixon
//     2.99, Hey & Thorne 2.3-4.33; the research's verdict is "use 3.0-4.0, expect ±50% scatter")
//   * meander wavelength lambda = 10-14 W (Part 2 §7.2; composite L_m = 8.36 W^1.05 ~ 10.2 W)
//   * sinuosity 1.2-2.2, radius of curvature 2-3 W (Part 2 §7.2/§7.3, Sacramento bends)

#include <cstdint>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "world/generation/field/fluvial.hpp"
#include "world/generation/field/terrain_field.hpp"

namespace world::generation::field {

struct HydrologyParams {
    float sea_level = 0.0f;

    /// Drainage area at which a channel begins, km². Matches the fluvial pass's own threshold --
    /// goal 307 selected 0.01 km² from the drainage-density band, and a river network extracted at
    /// a different threshold than the one that was incised would describe channels the terrain
    /// does not have.
    float channel_threshold_km2 = 0.01f;

    /// Depth of runoff per year at a precipitation multiplier of 1.0, metres.
    ///
    /// The precipitation plane is dimensionless (mean 1), so this is where absolute water enters
    /// the pipeline. 0.48 m/yr = 1200 mm of rain at a runoff coefficient of 0.4, which is an
    /// ordinary humid-temperate maritime figure for the 14 °C coast the climate stage describes.
    /// It is an ASSUMPTION, not a research anchor -- the research gives no rainfall for this world
    /// because this world is not a place. It is a single multiplier on every discharge, so it
    /// scales width as its square root and moves nothing else.
    float mean_annual_runoff_m = 0.48f;

    /// Bankfull discharge straight from contributing area: Q_bf = a * A_km2^b.
    ///
    /// **Petit & Pauquet (1997)**, ~40 gauging stations on Belgian Ardennes gravel-bed rivers,
    /// catchments **4 to 2,700 km²**, r = 0.989: `Q_b = 0.087 A^1.044` (m³/s, km²). Written up with
    /// its caveats in `research/bankfull-discharge-ratio.md`.
    ///
    /// This replaced a `bankfull_multiple` that converted mean annual flow to bankfull. That ratio
    /// turned out **not to be a published statistic at all** -- the literature relates bankfull
    /// discharge to AREA or to recurrence interval, essentially never to mean annual flow -- so the
    /// conversion was removed rather than guessed. The relation implies k = 5.8-6.9 over 1-50 km²,
    /// which is where the guessed 10 would have sat: at the top of the defensible band.
    ///
    /// The size range is the reason to prefer this over anything else found: it is calibrated ON
    /// catchments of this world's size, in this world's climate, where the regime equations are
    /// not (see `width_coefficient`).
    float bankfull_area_coefficient = 0.087f;
    float bankfull_area_exponent = 1.044f;

    /// W = a Q^b. Research Part 2 §1.3: a spans 2.3-4.3 by region, "use 3.0-4.0 as the plausible
    /// band"; b is 0.5 across the family (Bray's 0.53, the global fit's 0.512).
    ///
    /// **EXTRAPOLATED, KNOWINGLY.** NEH Part 654 Ch. 9's own data ranges put Nixon (1959) at
    /// 19.8-510 m³/s and Hey & Thorne at 3.9-425, and the chapter says the generalized width
    /// predictors should not be applied below 17 m³/s. This world's largest river is ~0.5 m³/s
    /// bankfull -- roughly two orders of magnitude below calibration. An independent area-to-width
    /// route (Sofia & Nikolopoulos 2020, W = 3.6 A^0.39) disagrees with this one by ~2.6x, which is
    /// the honest uncertainty on any width here; `terrain_dump` prints both so the gap is visible.
    /// W goes as the square root of Q, so even a 2x discharge error is only 1.41x in width.
    float width_coefficient = 3.5f;
    float width_exponent = 0.5f;

    /// The independent cross-check route, reported alongside but not used to build geometry:
    /// W = alpha * A_km2^beta, Sofia & Nikolopoulos (2020), alpha = 3.6 ± 2.3, beta = 0.39 ± 0.21.
    float width_area_coefficient = 3.6f;
    float width_area_exponent = 0.39f;

    /// Passes of centreline smoothing before the meander is applied.
    ///
    /// Research Part 7 §11(7) says to "route on the macro-grid then SMOOTH/meander the centreline",
    /// and skipping the smooth is measurable: D8 gives 45-degree staircase steps one cell (16 m)
    /// long, while this world's meander wavelength is ~29 m. The centreline's own zig-zag is at the
    /// same scale as the meander being applied to it, and the produced wavelength read 17.2 W for a
    /// requested 12 until the staircase was smoothed out of the way.
    int centreline_smoothing_passes = 4;

    /// lambda / W. Research Part 2 §7.2 band is 10-14; the composite fit is 10.2 and the Braudrick
    /// flume stabilised at 14.
    ///
    /// **10, not 12, because the research's band is on the MEASURED wavelength and the construction
    /// parameter is not the same number.** At 12 the produced geometry measures 14.6 -- a systematic
    /// ~21% overshoot, because the centreline is smoothed before the meander is integrated along it
    /// and smoothing shortens the valley length that the measurement divides by. The parameter is
    /// therefore set so the MEASUREMENT lands mid-band, which is what the band is a statement about.
    float meander_wavelength_ratio = 10.0f;

    /// Target sinuosity (channel length / valley length). Research Part 2 §7.3: Sacramento bends
    /// average ~1.4, cutoff-prone ~2.0, working band 1.2-2.2.
    float meander_sinuosity = 1.4f;

    /// Per-bend jitter in the meander phase, as a fraction of a wavelength. Part 7 §11(7) asks for
    /// "migration as a time-jittered displacement" -- a river that has been migrating does not have
    /// bends of identical phase, and a pure periodic curve reads as a decoration.
    float meander_jitter = 0.25f;

    std::uint32_t seed = 1337;
};

struct RiverNode {
    glm::vec2 position{}; ///< world x/z in metres, AFTER meandering
    float discharge_m3s = 0.0f;
    float width_m = 0.0f;
    float elevation_m = 0.0f; ///< monotonically non-increasing downstream (see the base-level pass)
    /// Signed perpendicular distance from the straight valley centreline this node meandered away
    /// from. Kept because it is the only way to MEASURE the produced wavelength: counting sign
    /// changes of the offset against the reach's end-to-end chord fails on a curving reach (the
    /// offset drifts monotonically and the crossings are missed), and measuring the parameter
    /// instead of the geometry would not be a measurement at all.
    float lateral_offset_m = 0.0f;
};

/// A maximal run of channel cells of one Strahler order, between junctions.
struct RiverReach {
    enum class Terminus : std::uint8_t {
        Sea,       ///< reaches sea level
        Lake,      ///< runs into a closed depression's water surface
        FieldEdge, ///< leaves the field; a legitimate terminus for a patch of a larger world
        Junction,  ///< flows into a higher-order reach -- NOT a terminus, the water continues
    };

    std::vector<RiverNode> nodes; ///< upstream to downstream
    std::uint8_t strahler = 0;
    Terminus terminus = Terminus::Sea;
    std::int32_t lake = -1; ///< index into RiverNetwork::lakes when terminus == Lake
    std::int32_t downstream = -1; ///< index of the reach this one joins, when terminus == Junction
    /// Straight-line valley length and the meandered channel length, so sinuosity is a fact about
    /// the produced geometry rather than a parameter that was asked for.
    float valley_length_m = 0.0f;
    float channel_length_m = 0.0f;
};

/// A closed depression that the fill raised: standing water with an outlet.
struct Lake {
    std::size_t outlet_cell = 0; ///< the spill point -- the lowest cell on the lake's rim
    float surface_m = 0.0f;
    std::size_t cell_count = 0;
    /// True when a downstream walk from the outlet reaches the sea or the field edge. Acceptance
    /// test 9's remaining half: a lake with no spill path is an internal basin wearing a hat.
    bool has_spill_path = false;
};

/// Research Part 2 §9.1, the Galloway triangle. Only two of the three vertices are reachable here
/// and rivers.cpp says why.
enum class DeltaRegime : std::uint8_t {
    RiverDominated, ///< elongate/birdfoot: supply outruns redistribution
    WaveDominated,  ///< cuspate/smoothed: alongshore energy planes the mouth off
};

struct Delta {
    glm::vec2 apex{};
    float radius_m = 0.0f;
    float discharge_m3s = 0.0f;
    DeltaRegime regime = DeltaRegime::RiverDominated;
    /// Sediment supply over wave energy: the axis the regime is chosen on, kept so the choice is
    /// inspectable rather than just a label.
    float supply_over_wave = 0.0f;
};

struct RiverNetwork {
    std::vector<RiverReach> reaches;
    std::vector<Lake> lakes;
    std::vector<Delta> deltas;

    /// Mean meander wavelength over channel width, measured over the largest reaches -- goal 309's
    /// Check. `topFraction` selects that population by discharge.
    [[nodiscard]] float measured_wavelength_over_width(float topFraction = 0.1f) const;
    /// Mean sinuosity over the same population.
    [[nodiscard]] float measured_sinuosity(float topFraction = 0.1f) const;
    /// Every reach terminates at sea level, a lake, or the field edge, and every lake spills.
    [[nodiscard]] bool every_reach_terminates() const;
};

/// Extracts the river network from an already-filled, already-accumulated field.
///
/// `filled` must have had `priority_flood` and `accumulate_flow` run on it (the pipeline's
/// `fill_depressions` and `flow` stages), because the reaches follow `net.receiver` and the
/// discharge reads `Plane::FlowAccum`.
///
/// `unfilledElevation` is the SAME field before the fill, and it is what identifies lakes: a lake
/// is exactly the set of cells the fill had to raise. A first version inferred lakes from the
/// fill's epsilon slope instead -- "a cell whose receiver is barely lower" -- and found 173 of them
/// on a field with a handful, because a diffused plain is also barely sloping. Comparing against
/// the pre-fill surface is not a heuristic; it is the definition. Pass an empty span to skip lake
/// detection, in which case reaches into a depression report `FieldEdge`.
[[nodiscard]] RiverNetwork extract_rivers(const TerrainField& filled, const FlowNetwork& net,
                                          std::span<const float> unfilledElevation,
                                          const HydrologyParams& p = {});

} // namespace world::generation::field
