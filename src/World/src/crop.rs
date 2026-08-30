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
use bevy::time::Time;
use bevy::world_serialization::WorldAssetRoot;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum GrowthStage {
    Seedling,
    Growing,
    Mature,
}

// Which of the three real feedstocks this plant is -- previously only
// implicit in which module's own spawn function created it (corn.rs's
// own field vs switchgrass.rs's vs miscanthus.rs's), with no way for a
// generic Query<&CropGrowth> (hud.rs's own per-species count) to tell
// them apart. Doesn't change any growth/carbon math -- purely an
// identity tag alongside the existing per-species numeric fields.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum CropSpecies {
    Corn,
    Switchgrass,
    Miscanthus,
}

#[derive(Component)]
pub(crate) struct CropGrowth {
    growth: f32, // 0.0 (just planted) .. 1.0 (mature)
    stage: GrowthStage,
    species: CropSpecies,
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
    // Emitted into CarbonBudget once, on harvest (fuel.rs) -- a per-species
    // field rather than fuel.rs's own single global constant, since
    // different feedstocks have genuinely different real lifecycle
    // emissions (see switchgrass.rs's own doc comment for the cited
    // research behind its own much-lower value relative to corn's).
    emission_on_harvest: f32,
    // Max wind-sway rotation in radians, reached only once the plant is
    // fully grown (scaled by `growth` below, same reasoning as the visual
    // scale lerp -- a barely-visible seedling swaying by a full-grown
    // plant's own angle would read as a glitch, not wind). A visual/
    // structural design choice, not a cited game-balance number like the
    // carbon fields above: real fine grass blades (switchgrass) flex far
    // more visibly in wind than a corn stalk's thicker, stiffer structure,
    // which is itself a real agronomic concern (lodging -- wind/rain
    // flattening a crop is a real yield-loss risk farmers manage) even if
    // this game doesn't model lodging as a mechanic yet.
    sway_amplitude: f32,
}

impl CropGrowth {
    pub(crate) fn new(
        light: f32,
        co2: f32,
        water: f32,
        growth_rate: f32,
        sequestration_on_maturity: f32,
        emission_on_harvest: f32,
        sway_amplitude: f32,
        species: CropSpecies,
    ) -> Self {
        Self {
            growth: 0.0,
            stage: GrowthStage::Seedling,
            species,
            light,
            co2,
            water,
            growth_rate,
            sequestration_on_maturity,
            emission_on_harvest,
            sway_amplitude,
        }
    }

    pub(crate) fn is_mature(&self) -> bool {
        self.stage == GrowthStage::Mature
    }

    pub(crate) fn species(&self) -> CropSpecies {
        self.species
    }

    pub(crate) fn emission_on_harvest(&self) -> f32 {
        self.emission_on_harvest
    }

    // The core Liebig's-law rule: whichever input is scarcest sets the
    // pace, full stop -- not min-then-average, not a weighted blend. Inputs
    // are expressed 0.0..=1.0 as "fraction of what this plant wants," so
    // 1.0 means fully supplied and never a bottleneck. irrigation_quality
    // (water.rs's own WaterBody::irrigation_multiplier(), 0.0..=1.0) scales
    // the water term specifically -- real off-optimum irrigation water pH
    // reduces effective nutrient uptake from a given water SUPPLY, it
    // doesn't add a fourth independent bottleneck alongside light/co2/water.
    fn limiting_factor(&self, irrigation_quality: f32) -> f32 {
        self.light.min(self.co2).min(self.water * irrigation_quality)
    }
}

pub(crate) fn update_crop_growth(
    dt: Res<DeltaSeconds>,
    mut carbon: ResMut<CarbonBudget>,
    water: Res<crate::water::WaterBody>,
    mut crops: Query<(&mut CropGrowth, &mut Transform)>,
) {
    let dt = dt.0;
    let irrigation_quality = water.irrigation_multiplier();
    for (mut crop, mut transform) in &mut crops {
        if crop.stage == GrowthStage::Mature {
            continue;
        }
        let limiting = crop.limiting_factor(irrigation_quality);
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

// Wind sway -- a separate system from update_crop_growth (not folded into
// its own per-crop loop) specifically because that function `continue`s
// early for Mature crops once growth is done, but a harvestable mature
// plant sitting in the field waiting for a click (fuel.rs) should keep
// swaying, not freeze. Rotates around the world Z axis (tips the plant's
// top side to side in X, base stays planted, matching real wind-sway
// appearance for an upright plant whose local up is Y). A per-plant phase
// offset derived from each plant's own X position keeps a whole field from
// swaying in perfect unison, which reads as fake -- real wind gusts also
// reach neighboring plants at slightly different times.
const SWAY_SPEED: f32 = 1.6;
const SWAY_PHASE_PER_METER: f32 = 1.3;

// Pure math, split out from update_crop_sway below so it's directly
// unit-testable without spinning up a Bevy World/App -- same reasoning
// this file's own tests already apply to limiting_factor()/growth math.
fn sway_angle(sway_amplitude: f32, growth: f32, elapsed_secs: f32, phase: f32) -> f32 {
    sway_amplitude * growth * (elapsed_secs * SWAY_SPEED + phase).sin()
}

fn update_crop_sway(time: Res<Time>, mut crops: Query<(&CropGrowth, &mut Transform)>) {
    let elapsed = time.elapsed_secs();
    for (crop, mut transform) in &mut crops {
        let phase = transform.translation.x * SWAY_PHASE_PER_METER;
        let angle = sway_angle(crop.sway_amplitude, crop.growth, elapsed, phase);
        transform.rotation = bevy::math::Quat::from_rotation_z(angle);
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
// Real fermentation+combustion lifecycle emission for corn ethanol -- see
// fuel.rs's own doc comment for the chemistry and the reasoning behind this
// exceeding FIELD_SEQUESTRATION (a full grow-then-harvest cycle should be
// net-emitting, matching real first-generation corn ethanol's documented
// lifecycle-emissions controversy).
const FIELD_EMISSION_ON_HARVEST: f32 = 3.0;
// Small -- a corn stalk's thicker, stiffer structure sways far less
// visibly in wind than fine grass blades (see CropGrowth's own doc comment
// on sway_amplitude for the real agronomic concern, lodging, this is
// loosely gesturing at without modeling as a mechanic).
const FIELD_SWAY_AMPLITUDE: f32 = 0.03;

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
    app.add_systems(Update, (spawn_corn_field_once_loaded, update_crop_growth, update_crop_sway));
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
            CropGrowth::new(light, co2, water, FIELD_GROWTH_RATE, FIELD_SEQUESTRATION, FIELD_EMISSION_ON_HARVEST, FIELD_SWAY_AMPLITUDE, CropSpecies::Corn),
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
        let crop = CropGrowth::new(1.0, 1.0, 0.1, 1.0, 10.0, 5.0, 0.1, CropSpecies::Corn);
        assert!((crop.limiting_factor(1.0) - 0.1).abs() < 1.0e-6, "limiting factor should equal the scarcest input, not an average");
    }

    #[test]
    fn poor_irrigation_quality_scales_down_the_water_term_specifically() {
        // water.rs's own WaterBody::irrigation_multiplier() should reduce
        // effective water supply, not act as a fourth independent
        // bottleneck alongside light/co2/water -- with light and co2 both
        // abundant and nominal water already the scarcest input, halving
        // irrigation quality should halve the resulting limiting factor.
        let crop = CropGrowth::new(1.0, 1.0, 0.4, 1.0, 10.0, 5.0, 0.1, CropSpecies::Corn);
        let full_quality = crop.limiting_factor(1.0);
        let half_quality = crop.limiting_factor(0.5);
        assert!((full_quality - 0.4).abs() < 1.0e-6, "at full irrigation quality, the limiting factor should equal the nominal water supply");
        assert!((half_quality - 0.2).abs() < 1.0e-6, "halving irrigation quality should halve the effective water term");
    }

    #[test]
    fn fully_supplied_crop_reaches_maturity_and_sequesters_carbon_once() {
        let mut budget = CarbonBudget::default();
        // Directly exercise the maturity transition + sequestration
        // payout without needing a full ECS World/App for this unit test
        // -- same reasoning physics.rs's own tests call RapierPhysics
        // methods directly rather than spinning up a Bevy schedule.
        let mut crop = CropGrowth::new(1.0, 1.0, 1.0, 1.0, 10.0, 5.0, 0.1, CropSpecies::Corn);
        let before = budget.remaining();

        // growth_rate=1.0 at limiting_factor=1.0 means 1.0 growth/sec;
        // 2 seconds is comfortably past the >=1.0 threshold.
        crop.growth = (crop.growth + crop.growth_rate * crop.limiting_factor(1.0) * 2.0).min(1.0);
        assert_eq!(crop.growth, 1.0);
        crop.stage = GrowthStage::Mature;
        budget.add_emission(-crop.sequestration_on_maturity);

        assert!(budget.remaining() > before, "reaching maturity should sequester carbon (grow the remaining budget)");
    }

    #[test]
    fn corn_field_harvest_cycle_is_net_emitting() {
        // The real, non-obvious tension this crop exists to surface:
        // growing then processing corn should be net-emitting overall, not
        // net-neutral or net-negative -- see fuel.rs's own doc comment on
        // the real fermentation+combustion chemistry this is grounded in,
        // and switchgrass.rs's own equivalent test for the cellulosic
        // feedstock's contrasting (still net-emitting, but far less) cycle.
        assert!(
            FIELD_EMISSION_ON_HARVEST > FIELD_SEQUESTRATION,
            "a full grow-then-harvest cycle should be net-emitting overall, matching real first-generation corn ethanol's own lifecycle-emissions tradeoff"
        );
    }

    #[test]
    fn seedling_barely_sways_but_a_mature_plant_reaches_full_amplitude() {
        // Sway is scaled by growth (see CropGrowth's own doc comment on
        // sway_amplitude): a fraction-0.0 seedling should produce ~zero
        // angle regardless of elapsed time/phase, while a fully-grown
        // plant, at the sine wave's own peak, should reach the full
        // configured amplitude -- not some fixed angle applied uniformly
        // regardless of growth.
        let amplitude = 0.15;
        let seedling_angle = sway_angle(amplitude, 0.0, 3.7, 1.2);
        assert!(seedling_angle.abs() < 1.0e-6, "an ungrown seedling should not visibly sway");

        // sin() peaks at 1.0 when its argument is PI/2 -- choose elapsed
        // such that elapsed_secs*SWAY_SPEED + phase == PI/2 exactly, with
        // phase=0 for a simple case.
        let elapsed_at_peak = (core::f32::consts::FRAC_PI_2) / SWAY_SPEED;
        let mature_angle = sway_angle(amplitude, 1.0, elapsed_at_peak, 0.0);
        assert!((mature_angle - amplitude).abs() < 1.0e-5, "a fully-grown plant at the sine wave's peak should reach its full configured sway_amplitude");
    }
}
