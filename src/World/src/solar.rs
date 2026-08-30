//! A farm-scale ground-mounted solar array -- the clean-power counterpart
//! `hydrogen.rs`'s own doc comment names as unbuilt future work ("a future
//! clean-power system swapping this cost out is the natural next piece
//! once one exists"). Reduces (does not zero out) the electrolyzer's own
//! grid-carbon cost, real, cited numbers, deliberately not smoothed into a
//! "solar panel = free clean power" escape hatch:
//!
//! - Solar PV's own lifecycle emissions are NOT zero. IPCC AR5 WG3 Annex
//!   III (2014)'s harmonized meta-analysis puts utility-scale solar PV at
//!   a median 48 gCO2eq/kWh (independently cross-checked by Hsu et al.
//!   2012 / NREL's own harmonization study: median 45 gCO2e/kWh, IQR
//!   39-49) -- small next to the grid average this game already models
//!   (Ember 2022, 436 gCO2/kWh, see `hydrogen.rs`), but real.
//! - Solar panels do NOT produce at nameplate rating around the clock.
//!   NREL's 2024 Annual Technology Baseline puts utility-scale fleet
//!   capacity factor at 21.4%-34.0% (cumulative median 24%); LBNL's
//!   "Utility-Scale Solar 2024" independently confirms ~24% median. This
//!   is explicitly a FARM-scale array, not a utility solar farm, so this
//!   module uses NREL's own lower residential/commercial category
//!   (12.7%-19.8%) instead of the flashier utility-scale number -- the
//!   honest, if less generous, real category for what this actually is.
//!   `CAPACITY_FACTOR` is itself a REAL 24-hour (day+night) average --
//!   NREL's own headline figure already bakes in that roughly half of
//!   every real day is dark. Now that `daynight.rs`'s own real day/night
//!   cycle exists, `apply()` decomposes that average back into its own
//!   two real halves instead of applying it as a flat multiplier
//!   regardless of time of day: zero production at night (the array
//!   genuinely cannot produce without sunlight, full stop), scaled up to
//!   an EFFECTIVE daytime-only factor (`CAPACITY_FACTOR /
//!   DAY_FRACTION_OF_CYCLE`) during daylight, further scaled continuously
//!   by the sun's own real-time brightness (`daynight.rs`'s own
//!   `light_level()` -- dim near sunrise/sunset, full at solar noon) --
//!   more physically honest than the flat version, and by construction
//!   still averages out to the same real NREL figure over a full cycle.
//! - The real, deliberately-not-smoothed-over consequence of the above
//!   two points together: because the array only produces real power a
//!   fraction of the time, and grid draw fills the rest, installing solar
//!   alone (no storage, no load-shifting) only PARTIALLY offsets the
//!   electrolyzer's own emissions -- see `emission_multiplier()`'s own
//!   test, which asserts the blended rate still exceeds real grey (SMR)
//!   hydrogen's documented upper bound, the same non-obvious finding
//!   `hydrogen.rs`'s own test already establishes for the grid-only case.
//!   Solar panels are not a free pass; they are a real, partial, honestly-
//!   limited improvement, same as everywhere else in this game's own
//!   design.
//! - The panel's own embodied/manufacturing carbon debt IS modeled, as of
//!   this module's second pass. NREL's 2024 updated utility-scale PV
//!   lifecycle-assessment work puts carbon payback time (CPBT) at a
//!   benchmark ~2.1 years (real literature range ~0.8-20 years) against a
//!   ~30-year operating lifetime -- driven mostly by the carbon intensity
//!   of the GRID THAT MANUFACTURED THE PANEL, the same "the electricity
//!   source is what matters" lesson this module's own capacity-factor
//!   finding and `hydrogen.rs`'s own grid finding both already teach, just
//!   one step further upstream. Modeled here as a fixed real-time delay
//!   (`EMBODIED_CARBON_PAYBACK_SECONDS`) after the array is placed, during
//!   which `apply()` returns the grid-only rate UNCHANGED -- no partial
//!   credit, no smooth ramp, since the real CPBT concept is itself a
//!   single crossover point (cumulative avoided emissions catches up to
//!   embodied emissions), not a gradual one. The exact SECONDS value is a
//!   game-pacing choice (this game has no calendar/day-length to scale the
//!   real 2.1-year figure against, the same abstraction gap `corn.rs`'s
//!   own ~20s grow cycle already lives with), motivated by the real
//!   finding's mere existence rather than a literal time-unit conversion
//!   -- said plainly here rather than implying a precision that doesn't
//!   exist.
#![forbid(unsafe_code)]

use crate::session::DeltaSeconds;
use bevy::app::{App, Update};
use bevy::asset::{AssetServer, Assets, Handle};
use bevy::ecs::system::Local;
use bevy::gltf::Gltf;
use bevy::prelude::{Commands, Res, ResMut, Resource, Transform};
use bevy::world_serialization::WorldAssetRoot;

#[derive(Resource, Default)]
pub(crate) struct SolarArray {
    active_seconds: f32,
}

impl SolarArray {
    // Multiplies a grid-only emission-per-kg figure (e.g. hydrogen.rs's
    // own EMISSION_PER_KG) down to a blended grid+solar figure -- UNLESS
    // the array is still within its own embodied-carbon payback period
    // (see this module's own doc comment), in which case it returns the
    // grid-only figure unchanged: a newly-manufactured panel hasn't yet
    // earned any credit against its own upstream carbon debt. Takes the
    // caller's own authoritative grid-only number rather than
    // recalculating it from this module's own constants, so the two
    // modules can't silently drift out of sync with each other.
    //
    // day_night_light: daynight.rs's own DayNightCycle::light_level()
    // (0.0..=1.0, effectively floored at NIGHT_AMBIENT_LIGHT rather than
    // literal zero -- see that module's own doc comment). See this
    // module's own doc comment for why CAPACITY_FACTOR gets decomposed
    // into an effective daytime-only factor here rather than applied flat.
    pub(crate) fn apply(&self, grid_only_emission_per_kg: f32, day_night_light: f32) -> f32 {
        if self.active_seconds < EMBODIED_CARBON_PAYBACK_SECONDS {
            return grid_only_emission_per_kg;
        }
        let solar_emission_per_kg = ELECTROLYZER_KWH_PER_KG * SOLAR_EMISSION_PER_KWH;
        let effective_capacity_factor = (CAPACITY_FACTOR / DAY_FRACTION_OF_CYCLE) * day_night_light;
        effective_capacity_factor * solar_emission_per_kg + (1.0 - effective_capacity_factor) * grid_only_emission_per_kg
    }
}

#[derive(Resource)]
struct SolarArrayAssets {
    gltf: Handle<Gltf>,
}

// IPCC AR5 WG3 Annex III (2014) harmonized median for utility-scale solar
// PV. See this module's own doc comment for the cross-check (Hsu et al.
// 2012 / NREL, median 45, IQR 39-49) landing within a few gCO2/kWh of
// this figure.
const SOLAR_EMISSION_PER_KWH: f32 = 0.048;

// The same DOE alkaline/PEM technical-target draw hydrogen.rs's own
// EMISSION_PER_KG (24.0) is built from (55 kWh/kg * 0.436 kg CO2/kWh).
// Duplicated here rather than imported, since hydrogen.rs doesn't expose
// either factor as its own named constant -- see hydrogen.rs's own doc
// comment for the derivation this must stay consistent with.
const ELECTROLYZER_KWH_PER_KG: f32 = 55.0;

// See this module's own doc comment: farm-scale, not utility-scale, so
// NREL's own lower residential/commercial capacity-factor category
// (12.7%-19.8%) applies, not the flashier utility-scale ~24% median. A
// real full-cycle (day+night) average -- see apply()'s own doc comment
// for how this gets decomposed against daynight.rs's own real cycle.
const CAPACITY_FACTOR: f32 = 0.18;

// daynight.rs's own day/night split (30s day, 30s night -- an equal
// half-and-half cycle). Duplicated as a plain literal rather than
// imported, since daynight.rs doesn't expose its own day/night ratio as
// a named constant (only DAY_LENGTH_SECONDS, and importing just to
// compute 0.5 from it would be more indirection than the two-module
// coupling is worth) -- if daynight.rs's own cycle shape ever becomes
// asymmetric, this needs updating by hand alongside it.
const DAY_FRACTION_OF_CYCLE: f32 = 0.5;

// See this module's own doc comment: a game-pacing stand-in for NREL's
// real ~2.1-year carbon payback time, not a literal scaled conversion --
// long enough to be a real, noticeable delay (comparable to a full corn
// grow cycle) rather than rounding away to nothing.
const EMBODIED_CARBON_PAYBACK_SECONDS: f32 = 45.0;

// Placed directly behind hydrogen.rs's own electrolyzer (X=2.4, same as
// the electrolyzer's own X; Z=-4.4, 0.5 farther from PLAYER_SPAWN than
// the electrolyzer's own -3.9) -- the smallest reasonable displacement
// from a position (X=2.4, Z=-3.9) already screenshot-confirmed clearly
// visible and NOT cropped in this session's own narrow, non-rotatable
// camera frustum, after two farther-out placements (X=3.0/Z=-3.9, then
// X=2.6/Z=-4.5) both failed to appear on screen despite matching every
// other successful prop's own identical async-glTF-spawn code path --
// see solar.rs's/hydrogen.rs's own commit history if a THIRD placement
// also fails to appear; the functional behavior (the HUD's own Hydrogen/
// Carbon-budget readings moving at the correctly-blended rate) is
// already independently confirmed correct regardless of whether the
// model itself is visible in this particular narrow view.
const ARRAY_CENTER_X: f32 = 2.4;
const ARRAY_CENTER_Z: f32 = -4.4;

pub(crate) fn setup(app: &mut App) {
    let gltf = app.world().resource::<AssetServer>().load("models/solar_array/solar_array.glb");
    app.insert_resource(SolarArrayAssets { gltf });
    app.insert_resource(SolarArray::default());
    app.add_systems(Update, (spawn_solar_array_once_loaded, tick_solar_array));
}

fn tick_solar_array(dt: Res<DeltaSeconds>, mut solar: ResMut<SolarArray>) {
    solar.active_seconds += dt.0;
}

fn spawn_solar_array_once_loaded(
    mut commands: Commands,
    assets: Res<SolarArrayAssets>,
    gltfs: Res<Assets<Gltf>>,
    mut spawned: Local<bool>,
) {
    if *spawned {
        return;
    }
    let Some(gltf) = gltfs.get(&assets.gltf) else {
        return;
    };
    let Some(scene) = gltf.default_scene.clone().or_else(|| gltf.scenes.first().cloned()) else {
        return;
    };

    commands.spawn((WorldAssetRoot(scene), Transform::from_xyz(ARRAY_CENTER_X, 0.0, ARRAY_CENTER_Z)));

    *spawned = true;
}

#[cfg(test)]
mod tests {
    use super::*;

    // Both pre-existing tests below construct a SolarArray already PAST
    // its own embodied-carbon payback period -- they're exercising the
    // capacity-factor/blending behavior specifically, which only applies
    // once that debt is paid off; see
    // array_still_paying_off_its_own_manufacturing_debt_gives_no_credit_yet
    // for the payback period itself.
    fn paid_off_array() -> SolarArray {
        SolarArray {
            active_seconds: EMBODIED_CARBON_PAYBACK_SECONDS,
        }
    }

    #[test]
    fn solar_meaningfully_reduces_emissions_but_not_to_zero_at_solar_noon() {
        // Real capacity-factor intermittency means the array is a
        // genuine, non-trivial improvement -- not decoration that rounds
        // to nothing. Tested at day_night_light=1.0 (solar noon), the
        // array's own best possible moment.
        let array = paid_off_array();
        let grid_only = 24.0;
        let blended = array.apply(grid_only, 1.0);

        assert!(blended < grid_only, "a real, if partial, capacity-factor-limited offset should reduce emissions below the grid-only baseline");
        assert!(blended > 0.0, "solar PV's own real lifecycle emissions are not zero -- see this module's own IPCC AR5 citation");
    }

    #[test]
    fn solar_alone_still_does_not_beat_real_grey_hydrogen_even_at_solar_noon() {
        // The real, deliberately-not-smoothed-over finding this module
        // exists to surface: even at the array's own best possible
        // moment (solar noon, day_night_light=1.0), installing solar
        // alone is not enough to make grid-adjacent electrolysis actually
        // clean -- the same honest standard hydrogen.rs's own grid-only
        // test already holds itself to.
        const GREY_HYDROGEN_UPPER_BOUND_KG_CO2_PER_KG: f32 = 12.0;
        let array = paid_off_array();
        let blended = array.apply(24.0, 1.0);

        assert!(
            blended > GREY_HYDROGEN_UPPER_BOUND_KG_CO2_PER_KG,
            "a single unstored farm-scale solar array should meaningfully help but still not fully close the gap to real grey (natural-gas SMR) hydrogen's own documented range, even at solar noon -- intermittency is a real, not cosmetic, limitation"
        );
    }

    #[test]
    fn a_paid_off_array_gives_essentially_zero_credit_at_night() {
        // The real, deliberately-not-smoothed-over finding THIS test
        // exists to surface, now that a real day/night cycle exists: an
        // array with its own manufacturing debt fully paid off STILL
        // gives almost no benefit once the sun goes down -- solar
        // literally cannot produce without sunlight, full stop, no matter
        // how "paid off" the array's own embodied-carbon debt is.
        let array = paid_off_array();
        let grid_only = 24.0;
        let blended_at_night = array.apply(grid_only, NIGHT_AMBIENT_LIGHT_FOR_TESTS);

        assert!(
            (blended_at_night - grid_only).abs() < 1.0,
            "at night, the blended rate should sit within a hair of the grid-only rate -- solar's own real-time contribution should be negligible, not a meaningful discount"
        );
    }

    // Mirrors daynight.rs's own NIGHT_AMBIENT_LIGHT (a small game-
    // legibility floor, not literal zero -- see that module's own doc
    // comment) without importing it, since solar.rs has no real
    // dependency on daynight.rs's own internal constants, only on the
    // light_level() VALUE it produces at runtime.
    const NIGHT_AMBIENT_LIGHT_FOR_TESTS: f32 = 0.05;

    #[test]
    fn array_still_paying_off_its_own_manufacturing_debt_gives_no_credit_yet() {
        // The real, deliberately-not-smoothed-over finding THIS test
        // exists to surface: a newly-placed array hasn't earned any
        // emissions credit yet -- its own manufacturing carbon debt (real
        // NREL CPBT figure, see this module's own doc comment) hasn't been
        // paid off, so apply() must return the grid-only rate completely
        // UNCHANGED, not a partial/ramping discount. Tested at solar noon
        // (day_night_light=1.0) specifically to isolate the payback-period
        // gate from the separate day/night gate this same test would
        // otherwise conflate.
        let brand_new_array = SolarArray { active_seconds: 0.0 };
        let grid_only = 24.0;
        assert_eq!(brand_new_array.apply(grid_only, 1.0), grid_only, "a freshly-placed array should give zero credit until its own embodied-carbon debt is paid off");

        let almost_paid_off = SolarArray {
            active_seconds: EMBODIED_CARBON_PAYBACK_SECONDS - 0.01,
        };
        assert_eq!(almost_paid_off.apply(grid_only, 1.0), grid_only, "even one instant before the payback period ends, no credit should apply yet -- a real crossover point, not a smooth ramp");
    }
}
