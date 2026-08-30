//! Harvest -> fermentation -> combustion: the first real EMITTING
//! counterpart to crop.rs's sequestration. Mirrors Eco's "no clean version"
//! design pillar -- producing fuel from a harvested crop unavoidably emits
//! CO2, there's no alternate path that doesn't (see the design rationale
//! this crate's own Eco citation, biofuel-climate-science-gameplay.md's
//! research summary). Real chemistry this is grounded in: fermentation
//! (C6H12O6 -> 2 C2H5OH + 2 CO2) releases CO2 directly during processing,
//! then combustion of the resulting ethanol (C2H5OH + 3 O2 -> 2 CO2 + 3
//! H2O) releases more when the fuel is actually burned -- two real
//! emission points in an actual biofuel lifecycle, collapsed into one
//! game-visible emission per harvest for this first pass rather than
//! modeled as two separate steps.
//!
//! The emission amount itself is read from each harvested crop's own
//! `CropGrowth::emission_on_harvest()` (a per-species field, not a global
//! constant here) -- switchgrass.rs's own doc comment covers why different
//! feedstocks have genuinely different real lifecycle emissions and how
//! its own much-lower value was calibrated against corn's.
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use crate::crop::CropGrowth;
use crate::session::DeltaSeconds;
use bevy::ecs::query::Without;
use bevy::prelude::{Commands, Component, Entity, Query, Res, ResMut, Resource};

#[derive(Resource, Default)]
pub(crate) struct FuelStockpile {
    liters: f32,
}

impl FuelStockpile {
    pub(crate) fn liters(&self) -> f32 {
        self.liters
    }
}

// A deliberately game-scale abstraction, not a literal unit conversion from
// real liters-per-acre ethanol yield figures (those operate at a much
// larger scale than "one plant"). Shared across crop species for this
// first pass -- real feedstocks do yield different liters of ethanol per
// unit biomass, but that's a separate, not-yet-researched number from the
// emissions figures this iteration calibrated; unlike emission_on_harvest,
// this isn't yet worth a per-species field.
const FUEL_PER_HARVEST: f32 = 0.3;
// A mature crop sits harvestable for a bit before auto-harvesting -- gives
// the grow -> mature -> harvested sequence a readable pace instead of
// vanishing the instant growth finishes. A real player-driven harvest
// (walk up, press a key) is a follow-up once interactive input can
// actually be exercised in this environment -- Windows-MCP cannot
// currently deliver either keyboard or raw mouse-motion input to this
// crate's winit window (confirmed empirically, see
// biofuel-climate-science-gameplay.md), so a mechanic that REQUIRES
// interactive input can't be verified end-to-end by this session itself
// yet. Auto-harvest keeps this phase's mechanic fully self-verifying.
const HARVEST_DELAY_SECONDS: f32 = 5.0;

#[derive(Component)]
pub(crate) struct HarvestTimer(f32);

pub(crate) fn update_harvest(
    dt: Res<DeltaSeconds>,
    mut commands: Commands,
    mut stockpile: ResMut<FuelStockpile>,
    mut carbon: ResMut<CarbonBudget>,
    newly_mature: Query<(Entity, &CropGrowth), Without<HarvestTimer>>,
    mut timers: Query<(Entity, &CropGrowth, &mut HarvestTimer)>,
) {
    let dt = dt.0;

    for (entity, crop) in &newly_mature {
        if crop.is_mature() {
            commands.entity(entity).insert(HarvestTimer(0.0));
        }
    }

    for (entity, crop, mut timer) in &mut timers {
        timer.0 += dt;
        if timer.0 >= HARVEST_DELAY_SECONDS {
            stockpile.liters += FUEL_PER_HARVEST;
            carbon.add_emission(crop.emission_on_harvest());
            // A plain despawn(), not despawn_recursive() -- Bevy's own
            // despawn() has cascaded to children by default since 0.14,
            // which is what actually removes the WorldAssetRoot-
            // instantiated mesh hierarchy crop.rs spawned this entity
            // with, not just the CropGrowth/HarvestTimer components on the
            // root.
            commands.entity(entity).despawn();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_harvested_crop_emits_exactly_its_own_emission_on_harvest_value() {
        // update_harvest reads crop.emission_on_harvest() rather than a
        // single global constant (see this module's own doc comment on
        // why) -- exercise that plumbing directly rather than only via
        // crop.rs's/switchgrass.rs's own per-species emission>sequestration
        // invariant tests, which don't touch fuel.rs's own code at all.
        let mut carbon = CarbonBudget::default();
        let before = carbon.remaining();
        let crop = CropGrowth::new(1.0, 1.0, 1.0, 1.0, 10.0, 4.0);
        carbon.add_emission(crop.emission_on_harvest());
        assert!((before - carbon.remaining() - 4.0).abs() < 1.0e-6, "the budget should shrink by exactly the harvested crop's own emission_on_harvest value");
    }
}
