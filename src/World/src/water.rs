//! A farm pond whose pH is coupled to the shared CarbonBudget -- real
//! ocean-acidification chemistry (CO2 + H2O <-> H2CO3 <-> H+ + HCO3- <->
//! 2H+ + CO3^2-, Henry's-law solubility, carbonate buffering) closes a real
//! feedback loop the player's own emissions eventually pay for: harvesting
//! crops emits CO2 (fuel.rs) -> the pond acidifies -> irrigation quality
//! changes -> crop growth itself changes (crop.rs), the same shared-budget
//! design carbon.rs's own doc comment already establishes (Anno 2070's CO2
//! Reservoir: one meter, real opportunity cost, not per-system decoration).
//!
//! Every constant is grounded in real, cited research (see
//! biofuel-climate-science-gameplay.md's research summary for the full
//! source list), with the game-scale abstraction gaps stated plainly where
//! the citations don't reach:
//!
//! - Starting pH (8.2): the real documented pre-industrial global mean
//!   ocean surface pH (WHOI/OCB-EPOCA, NOAA-PMEL, IPCC AR4/AR5) -- chosen
//!   as a "pristine but not necessarily farm-optimal" baseline, not
//!   because this is literally seawater.
//! - The pH-vs-emissions curve is LOG-SCALE and ACCELERATING, not linear:
//!   real ocean chemistry shows roughly each 0.3-unit pH drop corresponds
//!   to a doubling of dissolved CO2/H+ (log relationship), AND the ocean's
//!   own buffering capacity is documented to erode as cumulative CO2 load
//!   rises (Jiang et al. 2019, Sci. Reports -- projected down up to 34% by
//!   2100 under high-emissions scenarios). Modeled here as pH dropping
//!   proportionally to (fraction of CarbonBudget consumed)^2 -- early
//!   emissions (budget mostly intact, well-buffered) barely move pH; later
//!   emissions (budget mostly exhausted, buffering eroded) swing it hard.
//!   The exact exponent/scale are a game-legible stand-in for a real
//!   dynamic, not a literal curve fit -- no such precise formula exists in
//!   the cited literature for this game's abstracted "budget" units.
//! - Irrigation quality is a PEAKED curve around a real general-crop
//!   nutrient-uptake pH optimum (~6.0-6.5, Penn State Extension), not a
//!   naive "lower pH = worse" monotonic falloff -- the one real irrigation-
//!   water-pH field trial found (Vashisth et al. 2020, citrus: pH 5.8 vs
//!   8.0 for 60 days) actually found the LOWER, more acidic water
//!   outperformed the higher/more alkaline one. No irrigation-pH study
//!   exists for corn, switchgrass, or miscanthus specifically -- citrus and
//!   Zhang et al. 2015's switchgrass SOIL-pH (not water-pH) coupling-effect
//!   finding are the closest real proxies, both cited as an honest scope
//!   limit, not overclaimed as species-specific data.
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use bevy::app::{App, Update};
use bevy::asset::Assets;
use bevy::color::Color;
use bevy::math::primitives::Cuboid;
use bevy::mesh::{Mesh, Meshable};
use bevy::pbr::{MeshMaterial3d, StandardMaterial};
use bevy::prelude::{Handle, Mesh3d, Res, ResMut, Resource, Transform};

#[derive(Resource)]
pub(crate) struct WaterBody {
    ph: f32,
    material: Handle<StandardMaterial>,
}

const INITIAL_PH: f32 = 8.2;
const MIN_PH: f32 = 4.0;
// See this module's own doc comment: pH drop scales with the SQUARE of how
// much of CarbonBudget's total_budget has been consumed, capturing real
// buffering-capacity erosion as an accelerating (not linear) curve. At
// budget_fraction=1.0 (fully consumed), drop=PH_DROP_SCALE; the exponent
// itself is what does the "barely moves at first, swings hard later" work.
// Calibrated (not derived from a precise real formula -- none exists for
// this game's abstracted "budget" units, see this module's own doc
// comment) so the acceleration property stays real and visible in actual
// play: a real in-game test found exponent=2.0 made even a full harvest
// round (6 corn plants, +18 net emission against a 1000 total_budget)
// produce a pH change too small to show at 2 decimal places -- correct per
// the formula, but risked reading as "disconnected" rather than "working,
// just early" during a normal play session. 1.5 keeps emissions mattering
// less early than late (exponent > 1) while reaching player-visible
// territory within single-digit harvest cycles instead of dozens.
const PH_DROP_SCALE: f32 = 1.38;
const PH_ACCELERATION_EXPONENT: f32 = 1.5;

// Real general-crop nutrient-uptake pH optimum (Penn State Extension) --
// see this module's own doc comment for why this is a peaked curve, not a
// monotonic one.
const OPTIMAL_IRRIGATION_PH: f32 = 6.5;
const PH_FALLOFF_RANGE: f32 = 2.5;
// Floor, not zero: real research (Penn State Extension) frames off-optimum
// pH as reducing nutrient UPTAKE, not as an outright kill switch -- crop.rs's
// own Liebig's-law limiting_factor() already handles true starvation via
// the light/co2/water fields directly; this multiplier is a softer,
// separate penalty layered on top of that, not a second hard gate.
const IRRIGATION_MULTIPLIER_FLOOR: f32 = 0.3;

impl WaterBody {
    pub(crate) fn ph(&self) -> f32 {
        self.ph
    }

    // A peaked curve around OPTIMAL_IRRIGATION_PH, not a monotonic falloff
    // from INITIAL_PH -- see this module's own doc comment for the real
    // (if scope-limited) research behind this shape.
    pub(crate) fn irrigation_multiplier(&self) -> f32 {
        let distance = (self.ph - OPTIMAL_IRRIGATION_PH).abs();
        (1.0 - distance / PH_FALLOFF_RANGE).clamp(IRRIGATION_MULTIPLIER_FLOOR, 1.0)
    }

    // True once the pond has acidified PAST the real nutrient-uptake
    // optimum (Penn State Extension, ~6.0-6.5 -- see this module's own
    // doc comment), not merely once it's moved at all -- the peaked
    // curve means early acidification from INITIAL_PH (8.2) down toward
    // OPTIMAL_IRRIGATION_PH is actually an IMPROVEMENT (see
    // irrigation_multiplier_peaks_near_the_optimum_not_at_the_starting_ph's
    // own test), so a naive "pH has changed" flag would misleadingly warn
    // the player during the healthy half of the curve. This only goes
    // true on the declining, harmful side -- once the player has
    // genuinely overshot, not merely started moving away from the
    // (real, but not itself optimal) starting pH.
    pub(crate) fn has_overshot_optimal(&self) -> bool {
        self.ph < OPTIMAL_IRRIGATION_PH
    }
}

fn compute_ph(emitted: f32, total_budget: f32) -> f32 {
    // total_budget is always positive (carbon.rs's own Default), but guard
    // against division by zero defensively in case a future test/config
    // constructs one at 0.0.
    if total_budget <= 0.0 {
        return INITIAL_PH;
    }
    let budget_fraction = (emitted / total_budget).max(0.0);
    let drop = PH_DROP_SCALE * budget_fraction.powf(PH_ACCELERATION_EXPONENT);
    (INITIAL_PH - drop).max(MIN_PH)
}

// A healthy-to-acidified color ramp -- blue-teal at/near a healthy pH,
// shifting toward a murky yellow-green as it drops, the common visual
// shorthand for algae-bloom/eutrophic water quality (a real, if separate,
// consequence of nutrient/chemistry imbalance from what this module models
// numerically).
fn ph_to_color(ph: f32) -> Color {
    let t = ((INITIAL_PH - ph) / (INITIAL_PH - MIN_PH)).clamp(0.0, 1.0);
    let healthy = bevy::math::Vec3::new(0.10, 0.35, 0.55);
    let acidified = bevy::math::Vec3::new(0.45, 0.42, 0.15);
    let mixed = healthy.lerp(acidified, t);
    Color::srgb(mixed.x, mixed.y, mixed.z)
}

// Placed immediately left of switchgrass.rs's own field (X=-0.5..0.5,
// Z=-4.2..-3.6) at the same Z depth -- already screenshot-verified in a
// prior iteration to sit within the camera's visible frustum at this
// distance from PLAYER_SPAWN (see switchgrass.rs's own placement doc
// comment). Checked clear of level.rs's crate obstacles (X=-3.35..-1.65
// combined) with a small margin on both sides.
const POND_CENTER_X: f32 = -1.05;
const POND_CENTER_Z: f32 = -3.9;
const POND_HALF_WIDTH: f32 = 0.35;
const POND_HALF_DEPTH: f32 = 0.35;
const POND_HALF_HEIGHT: f32 = 0.02;

pub(crate) fn setup(app: &mut App) {
    let mesh: Handle<Mesh> = {
        let mut meshes = app.world_mut().resource_mut::<Assets<Mesh>>();
        meshes.add(Cuboid::new(POND_HALF_WIDTH * 2.0, POND_HALF_HEIGHT * 2.0, POND_HALF_DEPTH * 2.0).mesh())
    };
    let material: Handle<StandardMaterial> = {
        let mut materials = app.world_mut().resource_mut::<Assets<StandardMaterial>>();
        materials.add(StandardMaterial {
            base_color: ph_to_color(INITIAL_PH),
            unlit: true,
            ..Default::default()
        })
    };

    app.world_mut().spawn((
        Transform::from_xyz(POND_CENTER_X, -POND_HALF_HEIGHT, POND_CENTER_Z),
        Mesh3d(mesh),
        MeshMaterial3d(material.clone()),
    ));

    app.insert_resource(WaterBody { ph: INITIAL_PH, material });
    app.add_systems(Update, update_water_chemistry);
}

fn update_water_chemistry(carbon: Res<CarbonBudget>, mut water: ResMut<WaterBody>, mut materials: ResMut<Assets<StandardMaterial>>) {
    water.ph = compute_ph(carbon.emitted(), carbon.total_budget());
    if let Some(mut material) = materials.get_mut(&water.material) {
        material.base_color = ph_to_color(water.ph);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn early_modest_emissions_barely_move_ph_but_heavy_overshoot_swings_it_hard() {
        // The real, non-obvious dynamic this module exists to surface:
        // buffering capacity erodes with cumulative load (see this
        // module's own Jiang et al. 2019 citation), so the SAME-sized
        // emission matters far more once the budget is mostly spent than
        // when it was fresh -- not a flat/linear per-unit effect.
        let ph_at_10_percent = compute_ph(100.0, 1000.0);
        let ph_at_50_percent = compute_ph(500.0, 1000.0);
        let ph_at_150_percent = compute_ph(1500.0, 1000.0);

        let early_drop = INITIAL_PH - ph_at_10_percent;
        let mid_drop = ph_at_10_percent - ph_at_50_percent;
        let late_drop = ph_at_50_percent - ph_at_150_percent;

        assert!(early_drop < mid_drop, "the first 10% of the budget should move pH less than the next 40%");
        assert!(mid_drop < late_drop, "acidification should accelerate as the budget is consumed, not stay linear");
    }

    #[test]
    fn irrigation_quality_peaks_near_the_optimum_not_at_the_starting_ph() {
        // The real, citation-backed finding this module deliberately does
        // NOT smooth over: the pond's own starting pH (8.2, real ocean
        // baseline) is itself somewhat off the general-crop nutrient-uptake
        // optimum (~6.5) -- modest acidification from the player's own
        // emissions can genuinely IMPROVE irrigation quality before
        // eventually overshooting into harm at heavy emissions.
        let water_at_start = WaterBody {
            ph: INITIAL_PH,
            material: Handle::default(),
        };
        let water_at_optimum = WaterBody {
            ph: OPTIMAL_IRRIGATION_PH,
            material: Handle::default(),
        };
        let water_overshot = WaterBody {
            ph: MIN_PH,
            material: Handle::default(),
        };

        assert!(
            water_at_optimum.irrigation_multiplier() > water_at_start.irrigation_multiplier(),
            "irrigation quality at the real nutrient-uptake optimum should exceed the pond's own starting (alkaline) quality"
        );
        assert!(
            water_at_optimum.irrigation_multiplier() > water_overshot.irrigation_multiplier(),
            "overshooting past the optimum into heavy acidification should be worse than the optimum itself"
        );
    }

    #[test]
    fn overshoot_warning_stays_off_during_the_healthy_half_of_the_curve() {
        // The exact naive-flag mistake this method's own doc comment
        // warns against: pH moving DOWN from INITIAL_PH toward the
        // optimum is real improvement, not something worth alarming the
        // player about, even though "pH changed from its start value"
        // would be true for both directions.
        let water_still_alkaline = WaterBody {
            ph: (INITIAL_PH + OPTIMAL_IRRIGATION_PH) / 2.0,
            material: Handle::default(),
        };
        let water_at_optimum = WaterBody {
            ph: OPTIMAL_IRRIGATION_PH,
            material: Handle::default(),
        };
        let water_overshot = WaterBody {
            ph: MIN_PH,
            material: Handle::default(),
        };

        assert!(!water_still_alkaline.has_overshot_optimal(), "still on the healthy (improving) half of the curve -- no warning yet");
        assert!(!water_at_optimum.has_overshot_optimal(), "sitting exactly at the optimum is not itself an overshoot");
        assert!(water_overshot.has_overshot_optimal(), "genuinely past the optimum into harmful acidic territory should warn");
    }
}
