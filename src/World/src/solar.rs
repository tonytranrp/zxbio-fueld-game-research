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
//! - Explicitly NOT modeled in this first pass (a real, citable, but
//!   separate cost this module doesn't attempt to fold in numerically):
//!   the panel's own embodied/manufacturing carbon debt. NREL's 2024
//!   updated utility-scale PV lifecycle-assessment work puts carbon
//!   payback time (CPBT) at a benchmark ~2.1 years (real literature range
//!   ~0.8-20 years) against a ~30-year operating lifetime -- driven
//!   mostly by the carbon intensity of the GRID THAT MANUFACTURED THE
//!   PANEL, the same "the electricity source is what matters" lesson this
//!   module's own capacity-factor finding and `hydrogen.rs`'s own grid
//!   finding both already teach, just one step further upstream. A future
//!   pass modeling a manufacturing-debt period before the array's offset
//!   "kicks in" would be the honest way to fold this in; this pass keeps
//!   the array's benefit active from the moment it's placed, and says so
//!   plainly here rather than implying completeness.
#![forbid(unsafe_code)]

use bevy::app::{App, Update};
use bevy::asset::{AssetServer, Assets, Handle};
use bevy::ecs::system::Local;
use bevy::gltf::Gltf;
use bevy::prelude::{Commands, Res, Resource, Transform};
use bevy::world_serialization::WorldAssetRoot;

#[derive(Resource, Default)]
pub(crate) struct SolarArray;

impl SolarArray {
    // Multiplies a grid-only emission-per-kg figure (e.g. hydrogen.rs's
    // own EMISSION_PER_KG) down to a blended grid+solar figure. Takes the
    // caller's own authoritative grid-only number rather than
    // recalculating it from this module's own constants, so the two
    // modules can't silently drift out of sync with each other.
    pub(crate) fn apply(&self, grid_only_emission_per_kg: f32) -> f32 {
        let solar_emission_per_kg = ELECTROLYZER_KWH_PER_KG * SOLAR_EMISSION_PER_KWH;
        CAPACITY_FACTOR * solar_emission_per_kg + (1.0 - CAPACITY_FACTOR) * grid_only_emission_per_kg
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
// (12.7%-19.8%) applies, not the flashier utility-scale ~24% median.
const CAPACITY_FACTOR: f32 = 0.18;

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
    app.insert_resource(SolarArray);
    app.add_systems(Update, spawn_solar_array_once_loaded);
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

    #[test]
    fn solar_meaningfully_reduces_emissions_but_not_to_zero() {
        // Real capacity-factor intermittency means the array is a
        // genuine, non-trivial improvement -- not decoration that rounds
        // to nothing.
        let array = SolarArray;
        let grid_only = 24.0;
        let blended = array.apply(grid_only);

        assert!(blended < grid_only, "a real, if partial, capacity-factor-limited offset should reduce emissions below the grid-only baseline");
        assert!(blended > 0.0, "solar PV's own real lifecycle emissions are not zero -- see this module's own IPCC AR5 citation");
    }

    #[test]
    fn solar_alone_still_does_not_beat_real_grey_hydrogen() {
        // The real, deliberately-not-smoothed-over finding this module
        // exists to surface: because grid draw still fills in the
        // ~80%-plus of the time an unstored solar array isn't producing,
        // installing solar alone is not enough to make grid-adjacent
        // electrolysis actually clean -- the same honest standard
        // hydrogen.rs's own grid-only test already holds itself to.
        const GREY_HYDROGEN_UPPER_BOUND_KG_CO2_PER_KG: f32 = 12.0;
        let array = SolarArray;
        let blended = array.apply(24.0);

        assert!(
            blended > GREY_HYDROGEN_UPPER_BOUND_KG_CO2_PER_KG,
            "a single unstored farm-scale solar array should meaningfully help but still not fully close the gap to real grey (natural-gas SMR) hydrogen's own documented range -- intermittency is a real, not cosmetic, limitation"
        );
    }
}
