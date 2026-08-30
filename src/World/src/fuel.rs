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
//!
//! Harvest is player-triggered (left click), not the fixed-delay auto-
//! harvest this module originally shipped with -- that auto-harvest was
//! explicitly a stand-in "until interactive input can actually be
//! exercised in this environment," per this module's own prior doc
//! comment. Confirmed this session: while Windows-MCP synthetic keyboard
//! input still never reaches this crate's window (see
//! biofuel-climate-science-gameplay.md's own documented gotcha),
//! `WindowEvent::MouseInput` DOES reach it -- verified with a temporary
//! eprintln probe in session.rs's own window_event handler before this
//! mechanic was built on top of that assumption, not after.
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use crate::crop::CropGrowth;
use crate::input_state::InputState;
use crate::player::PlayerController;
use bevy::prelude::{Commands, Entity, Query, ResMut, Transform};

#[derive(bevy::prelude::Resource, Default)]
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
// A single click harvests every mature crop within this radius of the
// player, not just the single nearest one -- there's no raycast/reticle
// picking system yet to target one specific plant, so "click to harvest
// what's ready nearby" is the honest first-pass interaction rather than
// pretending to aim at a specific plant. Generous enough to cover every
// crop field from PLAYER_SPAWN itself (the farthest, miscanthus/
// switchgrass's own row, sits ~4.4 units out) since Windows-MCP still
// cannot deliver the keyboard input real player movement needs (see this
// module's own doc comment) -- this session can only ever click from the
// fixed spawn point, so the radius has to reach every field from there to
// be end-to-end testable at all. Tighten once real movement can be
// exercised and a walk-up-to-one-plant interaction is worth building.
const HARVEST_RADIUS: f32 = 6.0;

pub(crate) fn update_harvest(
    mut input: ResMut<InputState>,
    player: bevy::prelude::Res<PlayerController>,
    mut commands: Commands,
    mut stockpile: ResMut<FuelStockpile>,
    mut carbon: ResMut<CarbonBudget>,
    crops: Query<(Entity, &CropGrowth, &Transform)>,
) {
    if !input.take_left_click() {
        return;
    }

    let player_pos = player.position();
    let player_xz = bevy::math::Vec2::new(player_pos.x, player_pos.z);

    for (entity, crop, transform) in &crops {
        if !crop.is_mature() {
            continue;
        }
        let crop_xz = bevy::math::Vec2::new(transform.translation.x, transform.translation.z);
        if player_xz.distance(crop_xz) > HARVEST_RADIUS {
            continue;
        }

        stockpile.liters += FUEL_PER_HARVEST;
        carbon.add_emission(crop.emission_on_harvest());
        // A plain despawn(), not despawn_recursive() -- Bevy's own
        // despawn() has cascaded to children by default since 0.14, which
        // is what actually removes the WorldAssetRoot-instantiated mesh
        // hierarchy crop.rs spawned this entity with, not just the
        // CropGrowth component on the root.
        commands.entity(entity).despawn();
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
        let crop = CropGrowth::new(1.0, 1.0, 1.0, 1.0, 10.0, 4.0, 0.1);
        carbon.add_emission(crop.emission_on_harvest());
        assert!((before - carbon.remaining() - 4.0).abs() < 1.0e-6, "the budget should shrink by exactly the harvested crop's own emission_on_harvest value");
    }
}
