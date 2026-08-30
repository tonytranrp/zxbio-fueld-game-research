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
    // Not read yet -- same "no HUD element exists to show it to the player
    // yet" situation as CarbonBudget's own fields; see that struct's doc
    // comment. A HUD showing both together (fuel produced vs. carbon
    // spent) is more useful to build once than either alone right now.
    #[allow(dead_code)]
    pub(crate) fn liters(&self) -> f32 {
        self.liters
    }
}

// Deliberately game-scale abstractions, not literal unit conversions from
// real bushel-per-acre ethanol yield figures (those operate at a much
// larger scale than "one plant"). The ratio between them is what matters:
// EMISSION_PER_HARVEST is set larger than crop.rs's own
// SEQUESTRATION-on-maturity payout for the same plant, so a full
// grow-then-process cycle is net-emitting overall -- matching real
// first-generation corn ethanol's own well-documented lifecycle-emissions
// controversy (it is NOT carbon-neutral end to end, despite the naive
// intuition that "it's a plant, growing it undoes burning it"). Surfacing
// that real, non-obvious tension instead of smoothing it over is exactly
// the kind of fact-as-mechanic this project's whole design approach is
// built around.
const FUEL_PER_HARVEST: f32 = 0.3;
const EMISSION_PER_HARVEST: f32 = 3.0;
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
    mut timers: Query<(Entity, &mut HarvestTimer)>,
) {
    let dt = dt.0;

    for (entity, crop) in &newly_mature {
        if crop.is_mature() {
            commands.entity(entity).insert(HarvestTimer(0.0));
        }
    }

    for (entity, mut timer) in &mut timers {
        timer.0 += dt;
        if timer.0 >= HARVEST_DELAY_SECONDS {
            stockpile.liters += FUEL_PER_HARVEST;
            carbon.add_emission(EMISSION_PER_HARVEST);
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
    fn harvest_emission_exceeds_a_single_plants_own_sequestration() {
        // The real, non-obvious tension this mechanic exists to surface:
        // growing then processing one plant should be net-emitting, not
        // net-neutral or net-negative -- see this module's own doc comment
        // on why. crop.rs's FIELD_SEQUESTRATION constant is private to
        // that module (by design -- it's field-layout data, not a public
        // API), so this compares against the same literal value the corn
        // field is actually spawned with in session.rs, documented there.
        const CORN_FIELD_SEQUESTRATION: f32 = 2.0;
        assert!(
            EMISSION_PER_HARVEST > CORN_FIELD_SEQUESTRATION,
            "a full grow-then-harvest cycle should be net-emitting overall, matching real first-generation biofuel's own lifecycle-emissions tradeoff"
        );
    }
}
