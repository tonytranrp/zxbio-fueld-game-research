#pragma once

// Prompt 006 goal 302: orographic climate. Research Part 7 §10.2 stage 2; Part 5 §6.2 supplies the
// magnitude anchors, and Part 7 §"orographic precipitation" supplies the baseline model.
//
// This is the classical LINEAR UPSLOPE model -- the research's own named baseline,
// S = C_w * U.grad(h) + S_inf, "condensation proportional to wind-speed-times-terrain-slope,
// background rate added" -- extended with the two things that baseline lacks and that this goal's
// acceptance case explicitly asks for: DEPLETION (a parcel that has rained is dry, which is what
// makes a lee dry rather than merely un-enhanced) and DRIFT (condensate falls some distance
// downwind, which is the spillover). It is NOT Smith-Barstad's linear theory; climate.cpp argues
// that choice at length and goal 302a records the upgrade.
//
// Every parameter below is derived from a research anchor divided by this world's miniaturization,
// and each says which. That is the honest form here: an 8 km field with ~112 m of relief is a
// MINIATURE of the landscapes the anchors were measured on, and a parameter copied across without
// the division produces a physically-scaled non-effect (measured: 1.06:1).

#include "world/generation/field/terrain_field.hpp"

namespace world::generation::field {

struct ClimateParams {
    /// Prevailing wind, as a direction the air travels TOWARD. Normalised internally.
    float wind_x = 1.0f;
    float wind_z = 0.3f;

    /// Sea-level temperature, °C. The latitude term over 8 km is 0.07 degrees of arc, so latitude
    /// enters as this base rather than as a gradient across the field.
    float sea_level_temperature_c = 14.0f;

    /// The ascent that wrings 1 - 1/e of a parcel's moisture out of it, in metres.
    ///
    /// PHYSICALLY this is the water-vapour scale height, ~2 km: lift a column by that and it has
    /// essentially rained itself out, which is why §6.2's 1:10 anchor is quoted for a 1.5-2 km
    /// barrier. THIS WORLD'S orogen is ~112 m of relief, so at the physical value a barrier here
    /// removes 1 - exp(-112/2000) = 5.4% of the column and there is no rain shadow at all.
    ///
    /// 133 m = 2000 m / 15, the vertical miniaturization implied by comparing this world's relief
    /// to the anchor's 1.5-2 km barrier. Stated, not hidden: the climate treats a 112 m ridge as if
    /// it were a 1.7 km one, because that is what this world's mountains stand in for.
    float wring_out_height_m = 133.0f;

    /// The downwind distance over which suspended condensate falls out -- the spillover length.
    ///
    /// Roe & Baker put physical drift at 5-25 km. What must be preserved across the scale gap is
    /// not that length but the DIMENSIONLESS RATIO drift / barrier-half-width, because that ratio
    /// is what decides how far past a crest the rain reaches RELATIVE TO THE RANGE -- which is the
    /// shape of the spillover. The Olympic barrier is ~25 km half-width, so the ratio is 0.2-1.0.
    /// This world's orogenic crest is 320 m half-width (`macro_pipeline.cpp`), giving 64-320 m.
    ///
    /// A first draft used 1200 m, derived instead by miniaturizing the FIELD SPAN (60 km of Olympic
    /// transect against this 8 km field, 7.5x). That was wrong and the ridge test caught it: this
    /// world's field is 7.5x smaller than the anchor's transect but its mountains are 70x narrower,
    /// so scaling by the field span put the spillover three barrier-widths deep and the lee half of
    /// a ridge came out WETTER than the windward half.
    float drift_length_m = 250.0f;

    /// The share of precipitation that is NOT orographically forced -- synoptic rain that falls
    /// regardless of terrain. A fully shadowed cell falls to exactly this, so this parameter alone
    /// sets the field's wet/dry ratio, which is the one magnitude §6.2 states: "lee:windward ratio
    /// ~1:10 across a 1.5-2 km barrier".
    ///
    /// 0.20 is what that anchor SELECTS, measured rather than reasoned: swept over the shipped
    /// field (`terrain_dump` prints the sweep), the land p90/p10 ratio reads 21.5 / 16.1 / 10.3 /
    /// 7.2 / 5.3 at background fractions 0.05 / 0.10 / 0.20 / 0.30 / 0.40, and holds across three
    /// seeds at 0.20 (10.3, 8.6, 9.8).
    ///
    /// A first draft set 0.10 by reasoning that a cell receiving the FIELD-MEAN orographic rain
    /// would then be 10x a fully shadowed one. That was arithmetic about the wrong cell: the
    /// research's ratio is quoted against a windward PEAK, and the windward peak here is 3.7x the
    /// field mean, so 0.10 delivered 32:1 rather than 10:1. Sweeping was what found it.
    float background_fraction = 0.20f;

    /// e-folding distance over which a parcel crossing water recovers toward saturation. Without it
    /// a long ocean fetch arrives at the coast as dry as the lee of the last range it crossed.
    float ocean_recharge_length_m = 4000.0f;
};

/// Fills `Plane::Temperature` and `Plane::Precipitation` from `Plane::Elevation`.
///
/// Precipitation is DIMENSIONLESS and normalised so the field mean is exactly 1.0 -- it is a
/// multiplier on whatever absolute rainfall a consumer wants to assume. That is deliberate: the
/// biome classifier and the stream-power rain field both want the PATTERN, and normalising removes
/// a units fudge factor that would otherwise have to be tuned to make the pattern visible.
void compute_climate(TerrainField& out, const ClimateParams& p = {});

/// The environmental lapse rate the temperature plane uses, °C/km -- exposed so goal 302's Check
/// can assert it numerically rather than trusting the comment.
[[nodiscard]] float lapse_rate_c_per_km() noexcept;

} // namespace world::generation::field
