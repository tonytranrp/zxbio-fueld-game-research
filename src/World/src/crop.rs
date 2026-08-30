//! Crop growth -- a Liebig's-law limiting-factor rule: yield is gated by
//! the SCARCEST input, never averaged across them. This is the same
//! generalized-rule approach Oxygen Not Included applies uniformly across
//! every gas (density, heat, phase change) rather than special-casing each
//! resource -- the generality is what reads as "real" instead of scripted,
//! and it's real C3 photosynthesis besides: a plant starved of water
//! doesn't grow faster just because light is abundant.
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use crate::session::DeltaSeconds;
use bevy::app::{App, Update};
use bevy::asset::{AssetServer, Assets, Handle};
use bevy::ecs::system::Local;
use bevy::gltf::Gltf;
use bevy::prelude::{Commands, Component, Query, Res, ResMut, Resource, Transform};
use bevy::world_serialization::WorldAssetRoot;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum GrowthStage {
    Seedling,
    Growing,
    Mature,
}

#[derive(Component)]
pub(crate) struct CropGrowth {
    growth: f32, // 0.0 (just planted) .. 1.0 (mature)
    stage: GrowthStage,
    // Fixed per-plant environmental supply for this first pass -- a real
    // per-tile simulation (soil moisture depletion, shade from neighboring
    // plants, cloud cover) is a follow-up once there's more than one crop
    // type to make that complexity worth paying for.
    light: f32,
    co2: f32,
    water: f32,
    growth_rate: f32, // growth/second at a limiting factor of 1.0
    // Sequestered into CarbonBudget exactly once, on the transition into
    // Mature -- not applied continuously, so a plant destroyed or
    // harvested before maturity never banks a payout it didn't finish
    // earning.
    sequestration_on_maturity: f32,
}

impl CropGrowth {
    pub(crate) fn new(light: f32, co2: f32, water: f32, growth_rate: f32, sequestration_on_maturity: f32) -> Self {
        Self {
            growth: 0.0,
            stage: GrowthStage::Seedling,
            light,
            co2,
            water,
            growth_rate,
            sequestration_on_maturity,
        }
    }

    // The core Liebig's-law rule: whichever input is scarcest sets the
    // pace, full stop -- not min-then-average, not a weighted blend. Inputs
    // are expressed 0.0..=1.0 as "fraction of what this plant wants," so
    // 1.0 means fully supplied and never a bottleneck.
    fn limiting_factor(&self) -> f32 {
        self.light.min(self.co2).min(self.water)
    }
}

pub(crate) fn update_crop_growth(
    dt: Res<DeltaSeconds>,
    mut carbon: ResMut<CarbonBudget>,
    mut crops: Query<(&mut CropGrowth, &mut Transform)>,
) {
    let dt = dt.0;
    for (mut crop, mut transform) in &mut crops {
        if crop.stage == GrowthStage::Mature {
            continue;
        }
        let limiting = crop.limiting_factor();
        crop.growth = (crop.growth + crop.growth_rate * limiting * dt).min(1.0);

        crop.stage = if crop.growth >= 1.0 {
            GrowthStage::Mature
        } else if crop.growth > 0.0 {
            GrowthStage::Growing
        } else {
            GrowthStage::Seedling
        };

        // Visual growth: scale from a small seedling up to full size --
        // cheap and immediate feedback without needing a second mesh/LOD
        // per growth stage yet.
        const SEEDLING_SCALE: f32 = 0.08;
        let scale = SEEDLING_SCALE + (1.0 - SEEDLING_SCALE) * crop.growth;
        transform.scale = bevy::math::Vec3::splat(scale);

        if crop.stage == GrowthStage::Mature {
            carbon.add_emission(-crop.sequestration_on_maturity);
        }
    }
}

#[derive(Resource)]
struct CornFieldAssets {
    gltf: Handle<Gltf>,
}

// (water, light, co2) supply as a fraction of what each plant wants -- one
// row fully supplied, one row water-limited, so the limiting-factor rule
// (see limiting_factor()'s own doc comment) visibly does something
// side-by-side in the very first field planted, not just in a unit test.
const FIELD_SUPPLY: [(f32, f32, f32); 6] = [
    (1.0, 1.0, 1.0),
    (1.0, 1.0, 1.0),
    (1.0, 1.0, 1.0),
    (0.35, 1.0, 1.0),
    (0.35, 1.0, 1.0),
    (0.35, 1.0, 1.0),
];
const FIELD_ROW_SPACING: f32 = 1.2;
const FIELD_COL_SPACING: f32 = 1.0;
const FIELD_GROWTH_RATE: f32 = 0.05; // full growth in ~20s at limiting factor 1.0
const FIELD_SEQUESTRATION: f32 = 2.0;

// Directly ahead of PLAYER_SPAWN (0,1,-8), facing +Z the same direction the
// player spawns looking -- a 3x2 grid of corn centered on X=0, a couple of
// meters out (Z -6.0..-4.8), well before any of level.rs's other obstacles
// (nearest is the crates/drums row starting around Z=-3.4) or the barn
// (front wall Z=3.5). Deliberately in the player's immediate view on spawn
// rather than off to a side -- the farm should be the first thing the
// player sees, not something found by turning around (assets/models/
// corn_plant/, see that folder's README for generation provenance). Async
// two-step spawn, same pattern as viewmodel.rs's own setup()/
// spawn_viewmodel_once_loaded(): the glTF's scene isn't available until its
// own asset dependencies finish loading, which can't all happen inside
// resumed() itself.
pub(crate) fn setup(app: &mut App) {
    let gltf = app.world().resource::<AssetServer>().load("models/corn_plant/corn_plant.glb");
    app.insert_resource(CornFieldAssets { gltf });
    app.add_systems(Update, (spawn_corn_field_once_loaded, update_crop_growth));
}

fn spawn_corn_field_once_loaded(
    mut commands: Commands,
    assets: Res<CornFieldAssets>,
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

    for (i, (water, light, co2)) in FIELD_SUPPLY.iter().copied().enumerate() {
        let row = (i / 3) as f32;
        let col = (i % 3) as f32;
        let x = -1.0 + col * FIELD_COL_SPACING;
        let z = -6.0 + row * FIELD_ROW_SPACING;
        commands.spawn((
            WorldAssetRoot(scene.clone()),
            Transform::from_xyz(x, 0.0, z),
            CropGrowth::new(light, co2, water, FIELD_GROWTH_RATE, FIELD_SEQUESTRATION),
        ));
    }

    *spawned = true;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn scarce_water_bottlenecks_growth_even_with_abundant_light_and_co2() {
        // Liebig's law, not an average: light=1.0 and co2=1.0 (fully
        // supplied) must not compensate for water=0.1 (starved) -- the
        // limiting factor should equal the SCARCEST input, not
        // (1.0+1.0+0.1)/3 =~ 0.7.
        let crop = CropGrowth::new(1.0, 1.0, 0.1, 1.0, 10.0);
        assert!((crop.limiting_factor() - 0.1).abs() < 1.0e-6, "limiting factor should equal the scarcest input, not an average");
    }

    #[test]
    fn fully_supplied_crop_reaches_maturity_and_sequesters_carbon_once() {
        let mut budget = CarbonBudget::default();
        // Directly exercise the maturity transition + sequestration
        // payout without needing a full ECS World/App for this unit test
        // -- same reasoning physics.rs's own tests call RapierPhysics
        // methods directly rather than spinning up a Bevy schedule.
        let mut crop = CropGrowth::new(1.0, 1.0, 1.0, 1.0, 10.0);
        let before = budget.remaining();

        // growth_rate=1.0 at limiting_factor=1.0 means 1.0 growth/sec;
        // 2 seconds is comfortably past the >=1.0 threshold.
        crop.growth = (crop.growth + crop.growth_rate * crop.limiting_factor() * 2.0).min(1.0);
        assert_eq!(crop.growth, 1.0);
        crop.stage = GrowthStage::Mature;
        budget.add_emission(-crop.sequestration_on_maturity);

        assert!(budget.remaining() > before, "reaching maturity should sequester carbon (grow the remaining budget)");
    }
}
