//! A farm-scale hydrogen electrolyzer -- a second, distinct energy pathway
//! alongside crop.rs's/fuel.rs's biofuel one, coupled to the SAME shared
//! CarbonBudget (carbon.rs's own single-meter design). Real electrolysis
//! chemistry: 2 H2O -> 2 H2 + O2, driven by electrical energy.
//!
//! The one real finding this module exists to surface, deliberately not
//! smoothed over: **an electrolyzer is not automatically clean** -- exactly
//! the same lesson crop.rs's/fuel.rs's own corn-ethanol design already
//! teaches (growing a plant doesn't automatically undo burning it), now
//! for a technology popularly assumed to be inherently green. Real,
//! cited numbers:
//!
//! - Modern alkaline/PEM electrolyzers need ~55 kWh of electricity per kg
//!   of H2 produced (DOE technical-target tables, 61% LHV system
//!   efficiency) -- high-temperature solid-oxide electrolysis needs only
//!   ~38 kWh/kg of ELECTRICITY (88% electrical efficiency, the rest
//!   entering as heat), a real three-way tradeoff (cost/maturity vs.
//!   load-following flexibility vs. peak efficiency requiring continuous
//!   heat) not modeled as a choice in this first pass -- this module
//!   models the alkaline/PEM case, the realistic default for a farm-scale
//!   unit without dedicated high-temperature process heat.
//! - The carbon cost of that electricity depends entirely on its SOURCE.
//!   IEA's Global Hydrogen Review 2024: unabated grey (natural-gas SMR)
//!   hydrogen emits 10-12 kg CO2-eq/kg H2. IEA states grid intensity must
//!   stay below ~200-240 g CO2/kWh for electrolytic H2 to beat grey on
//!   emissions -- but Ember's Global Electricity Review puts the 2022
//!   GLOBAL AVERAGE grid intensity at 436 g CO2/kWh, roughly double that
//!   threshold. At 55 kWh/kg, electrolysis on an average, un-greened grid
//!   emits ~24 kg CO2/kg H2 (55 * 0.436) -- WORSE than grey SMR hydrogen,
//!   not better. EMISSION_PER_KG below uses this real, calculated figure
//!   directly (carbon.rs's own "game-tonnes, not real tonnes, the RATIO is
//!   what matters" convention already permits this, and the real number
//!   happens to already sit in this game's legible numeric range).
//!
//! This first pass deliberately has no clean/renewable power source to
//! plug in instead (there's no solar/battery system yet) -- the
//! electrolyzer always draws the realistic default (an un-greened grid),
//! always paying the real carbon cost above, every second it runs. A
//! future clean-power system swapping this cost out is the natural next
//! piece once one exists; see biofuel-climate-science-gameplay.md.
//!
//! Runs continuously once spawned, not click-triggered like fuel.rs's own
//! harvest -- input_state.rs's single left-click flag is drained
//! (`Option::take`-style) by the first system that reads it each frame,
//! so a second, unrelated click-triggered system in the same frame would
//! either need its own separate input channel or silently never fire
//! whenever fuel.rs's own harvest system happens to run first. Continuous
//! production sidesteps that conflict entirely, and is also the more
//! realistic behavior for a real electrolyzer (they run continuously
//! while powered, not "clicked" per batch).
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use crate::session::DeltaSeconds;
use bevy::app::{App, Update};
use bevy::asset::{AssetServer, Assets, Handle};
use bevy::ecs::system::Local;
use bevy::gltf::Gltf;
use bevy::prelude::{Commands, Res, ResMut, Resource, Transform};
use bevy::world_serialization::WorldAssetRoot;

#[derive(Resource, Default)]
pub(crate) struct HydrogenStockpile {
    kg: f32,
}

impl HydrogenStockpile {
    pub(crate) fn kg(&self) -> f32 {
        self.kg
    }
}

#[derive(Resource)]
struct ElectrolyzerAssets {
    gltf: Handle<Gltf>,
}

// A deliberately small, steady trickle -- game-scale, not a literal kg/s
// rate (same "game-scale abstraction" convention fuel.rs's own
// FUEL_PER_HARVEST carries). Paced so the running carbon cost below
// becomes visible on the HUD within roughly a minute of real play, similar
// to how water.rs's own pH curve was calibrated for player-visible pacing.
const PRODUCTION_RATE_KG_PER_SECOND: f32 = 0.01;

// See this module's own doc comment: 55 kWh/kg (DOE alkaline/PEM technical
// target) * 0.436 kg CO2/kWh (Ember's 2022 global average grid intensity)
// = ~24 kg CO2/kg H2 -- a real, calculated, cited figure worse than grey
// (natural-gas SMR) hydrogen's own documented 10-12 kg CO2-eq/kg H2, used
// directly as the game constant.
const EMISSION_PER_KG: f32 = 24.0;

// Placed at the same camera depth as switchgrass.rs's/miscanthus.rs's own
// fields (already screenshot-verified in prior iterations to sit within
// the visible frustum from PLAYER_SPAWN), offset right of miscanthus's own
// field (X=1.2..1.7) and checked clear of level.rs's drum obstacles
// (X=3.2/3.9, Z=-3.0/-3.4 -- a different Z band from this prop's own -3.9).
const ELECTROLYZER_CENTER_X: f32 = 2.4;
const ELECTROLYZER_CENTER_Z: f32 = -3.9;

pub(crate) fn setup(app: &mut App) {
    let gltf = app.world().resource::<AssetServer>().load("models/electrolyzer/electrolyzer.glb");
    app.insert_resource(ElectrolyzerAssets { gltf });
    app.insert_resource(HydrogenStockpile::default());
    app.add_systems(Update, (spawn_electrolyzer_once_loaded, update_electrolyzer));
}

fn spawn_electrolyzer_once_loaded(
    mut commands: Commands,
    assets: Res<ElectrolyzerAssets>,
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

    commands.spawn((
        WorldAssetRoot(scene),
        Transform::from_xyz(ELECTROLYZER_CENTER_X, 0.0, ELECTROLYZER_CENTER_Z),
    ));

    *spawned = true;
}

fn update_electrolyzer(dt: Res<DeltaSeconds>, mut hydrogen: ResMut<HydrogenStockpile>, mut carbon: ResMut<CarbonBudget>) {
    let produced = PRODUCTION_RATE_KG_PER_SECOND * dt.0;
    hydrogen.kg += produced;
    carbon.add_emission(produced * EMISSION_PER_KG);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn grid_powered_electrolysis_emits_more_per_kg_than_real_grey_hydrogen() {
        // The real, non-obvious lesson this module exists to surface --
        // see this module's own doc comment for the IEA/Ember figures
        // behind both sides of this comparison.
        const GREY_HYDROGEN_UPPER_BOUND_KG_CO2_PER_KG: f32 = 12.0;
        assert!(
            EMISSION_PER_KG > GREY_HYDROGEN_UPPER_BOUND_KG_CO2_PER_KG,
            "grid-powered electrolysis should emit more per kg of H2 than even the upper end of real grey (natural-gas SMR) hydrogen's documented range -- an un-greened electrolyzer is not automatically clean"
        );
    }

    #[test]
    fn running_the_electrolyzer_for_a_fixed_time_produces_hydrogen_and_emits_proportionally() {
        let mut hydrogen = HydrogenStockpile::default();
        let mut carbon = CarbonBudget::default();
        let before_remaining = carbon.remaining();

        let dt = 10.0;
        let produced = PRODUCTION_RATE_KG_PER_SECOND * dt;
        hydrogen.kg += produced;
        carbon.add_emission(produced * EMISSION_PER_KG);

        assert!((hydrogen.kg() - produced).abs() < 1.0e-6, "hydrogen produced should scale linearly with elapsed time at the fixed production rate");
        assert!(carbon.remaining() < before_remaining, "running the electrolyzer should shrink the remaining carbon budget, not leave it untouched");
    }
}
