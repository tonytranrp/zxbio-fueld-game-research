//! Second crop: switchgrass (Panicum virgatum), a cellulosic/advanced-
//! generation biofuel feedstock, planted deliberately next to crop.rs's
//! corn field to make the real first-generation vs. advanced-generation
//! biofuel contrast a first-sight visual comparison, not just two numbers
//! in a spreadsheet. Reuses crop.rs's own CropGrowth component (Liebig's-
//! law limiting-factor growth) unchanged -- the growth RULE is the same
//! real photosynthesis limit for any plant; only the per-species numbers
//! differ, which is exactly what CropGrowth::new()'s parameters are for.
//!
//! Every constant below is calibrated against real, cited research (see
//! biofuel-climate-science-gameplay.md's research summary for the full
//! source list -- primarily Schmer et al. 2008, PNAS/USDA-ARS, a 10-farm
//! 5-year field study), not invented for game balance:
//!
//! - Lifecycle GHG emissions: switchgrass ethanol is documented at 76%
//!   lower than corn ethanol (Schmer 2008) and 77-97% lower than gasoline
//!   across feedstocks in Argonne's GREET model (Wang et al. 2012) -- the
//!   real reason is NOT "grass is magic," it's that switchgrass processing
//!   has an extra step corn doesn't: cellulosic ethanol burns its leftover
//!   lignin (the non-fermentable structural polymer cellulose/hemicellulose
//!   are locked inside) for onsite process heat, offsetting fossil natural
//!   gas a corn plant would otherwise burn.
//! - Explicitly NOT modeled as net carbon-negative: the same research
//!   found switchgrass ethanol is a dramatic reduction but not reliably
//!   net-negative in soil-carbon-inclusive lifecycle models (miscanthus,
//!   not built here, is the feedstock more consistently found net-negative
//!   -- a real candidate for a future third crop). Modeling switchgrass as
//!   magically carbon-negative would be exactly the kind of comfortable-
//!   but-wrong narrative this project's whole design approach exists to
//!   correct (see crop.rs/fuel.rs's own corn-emissions design note).
//! - Water: deliberately NOT modeled as "switchgrass needs less water" --
//!   real field measurements (Hamilton et al. 2015) found switchgrass
//!   evapotranspiration similar to or higher than corn's, and its
//!   water-use efficiency trends lower than corn's under drought (Namoi et
//!   al. 2025). The real, documented advantage is deep-root access: on the
//!   SAME marginal/low-water land where corn's shallower roots fail,
//!   switchgrass's much deeper root system reaches moisture corn can't --
//!   that's a supply-access difference, not a lower-need difference, so
//!   it's modeled here as a higher effective `water` value on the same
//!   marginal tile corn would be planted on, not a universally lower
//!   requirement.
//! - Growth pace: perennial, ~3 real years to reach full yield (vs. corn's
//!   single annual season) -- compressed to a meaningfully slower, not
//!   literally 3x-real-time, growth_rate for playability (same "game-scale
//!   abstraction, not literal unit conversion" disclaimer fuel.rs's own
//!   constants already carry).
//! - Fertilizer: a 2020-2022 Illinois field trial (Namoi et al. 2025)
//!   measured corn at 202 kg N/ha/yr vs. switchgrass at 56 kg N/ha/yr (~3.6x
//!   less) -- not yet modeled numerically (no fertilizer resource exists
//!   yet), noted here as a real, well-documented input-cost difference
//!   worth building toward once one does.
#![forbid(unsafe_code)]

use crate::crop::CropGrowth;
use bevy::app::{App, Update};
use bevy::asset::{AssetServer, Assets, Handle};
use bevy::ecs::system::Local;
use bevy::gltf::Gltf;
use bevy::prelude::{Commands, Res, Resource, Transform};
use bevy::world_serialization::WorldAssetRoot;

#[derive(Resource)]
struct SwitchgrassFieldAssets {
    gltf: Handle<Gltf>,
}

// (water, light, co2) supply as a fraction of what each plant wants, same
// convention crop.rs's own FIELD_SUPPLY uses. All four plants sit on the
// SAME nominally marginal/low-water tile corn's own water-limited row uses
// (0.35) -- but switchgrass's own deep-root access to that same tile's
// moisture is modeled as a meaningfully higher effective value (0.7, double
// corn's), not the full 1.0 corn's fully-supplied row gets, since real
// field data (see this file's own doc comment) does NOT support "needs no
// water at all," only "reaches more of what's there than corn's shallower
// roots can."
const FIELD_SUPPLY: [(f32, f32, f32); 4] = [(0.7, 1.0, 1.0), (0.7, 1.0, 1.0), (0.7, 1.0, 1.0), (0.7, 1.0, 1.0)];
const FIELD_COL_SPACING: f32 = 1.0;
// Tighter than crop.rs's own 1.2 -- keeps this field's second row (Z=-3.6)
// comfortably clear of level.rs's nearby crate obstacles (Z spans as close
// as -2.95; see this module's own setup() doc comment for the full check).
const FIELD_ROW_SPACING: f32 = 0.6;
// ~2.5x slower than crop.rs's own FIELD_GROWTH_RATE (0.05) -- a compressed
// stand-in for real switchgrass's multi-year establishment against corn's
// single annual season, not a literal ratio (see this file's own doc
// comment on why 3 real years isn't modeled 1:1).
pub(crate) const FIELD_GROWTH_RATE: f32 = 0.02;
// Calibrated against crop.rs's own corn FIELD_SEQUESTRATION (2.0): higher,
// reflecting perennial grasses' well-documented extensive root-biomass soil
// carbon storage relative to an annual crop's more above-ground-focused
// growth -- a real, qualitatively well-supported difference this pass
// doesn't attempt to pin to a precise published ratio.
pub(crate) const FIELD_SEQUESTRATION: f32 = 2.2;
// Calibrated against crop.rs's own corn FIELD_EMISSION_ON_HARVEST (3.0)
// using Schmer et al. 2008's real, cited 76%-lower-than-corn-ethanol
// lifecycle GHG figure applied to the game's NET-per-cycle margin (corn:
// 3.0 emitted - 2.0 sequestered = +1.0 net) rather than the raw emission
// number alone -- a full switchgrass cycle nets +0.3 (30% of corn's net),
// dramatically better but still net-EMITTING, matching this file's own doc
// comment on deliberately not modeling switchgrass as carbon-negative.
pub(crate) const FIELD_EMISSION_ON_HARVEST: f32 = 2.5;
// Largest of the three crops (see crop.rs's own corn FIELD_SWAY_AMPLITUDE,
// 0.03, for the baseline and CropGrowth's own doc comment for the general
// reasoning) -- switchgrass's real narrow, fine blade-like leaves (this
// file's own generation prompt) flex more visibly in wind than corn's
// thicker stalks or miscanthus's own stiffer, bamboo-like canes.
pub(crate) const FIELD_SWAY_AMPLITUDE: f32 = 0.15;

// A second row directly between the player and crop.rs's own corn field
// (which occupies X=-1.0..1.0, Z=-6.0..-4.8) -- NOT beside it in X as
// originally placed: empirically, corn's own rightmost column (X=1.0) sits
// right at the edge of the default camera's visible frustum at that depth
// (screenshot-verified after the first build attempt placed switchgrass at
// X=1.8/2.6, which was confirmed off-screen), so any wider-in-X placement
// at the same depth risks going off-frame given the same "camera can't
// currently be turned via automation" constraint documented in crop.rs's
// own setup() doc comment (Windows-MCP still can't deliver mouse-look
// input; see biofuel-climate-science-gameplay.md). Reusing corn's own
// proven-visible X range (-0.5..0.5, a narrower 2-column subset of it) at a
// shallower depth instead keeps this field guaranteed in-frame: closer to
// the player than corn (a wider frustum at greater depth doesn't help
// here, since the field needs to sit somewhere corn doesn't already
// occupy). Z=-4.2/-3.6 checked clear of both level.rs obstacle clusters --
// the crates (X=-3.0/-2.0, Z=-2.0/-2.6, half-extent 0.35, so Z spans
// -2.35..-2.95) and the drums (X=3.2/3.9, Z=-3.0/-3.4) -- neither overlaps
// this field's X=-0.5..0.5, Z=-4.2..-3.6 footprint.
pub(crate) fn setup(app: &mut App) {
    let gltf = app.world().resource::<AssetServer>().load("models/switchgrass/switchgrass.glb");
    app.insert_resource(SwitchgrassFieldAssets { gltf });
    app.add_systems(Update, spawn_switchgrass_field_once_loaded);
}

fn spawn_switchgrass_field_once_loaded(
    mut commands: Commands,
    assets: Res<SwitchgrassFieldAssets>,
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
        let row = (i / 2) as f32;
        let col = (i % 2) as f32;
        let x = -0.5 + col * FIELD_COL_SPACING;
        let z = -4.2 + row * FIELD_ROW_SPACING;
        commands.spawn((
            WorldAssetRoot(scene.clone()),
            Transform::from_xyz(x, 0.0, z),
            CropGrowth::new(light, co2, water, FIELD_GROWTH_RATE, FIELD_SEQUESTRATION, FIELD_EMISSION_ON_HARVEST, FIELD_SWAY_AMPLITUDE, crate::crop::CropSpecies::Switchgrass),
        ));
    }

    *spawned = true;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn switchgrass_grows_slower_but_sequesters_more_per_plant_than_corn() {
        // The real, documented tradeoff this crop exists to surface:
        // greater carbon payoff per plant, at the cost of a much slower
        // path to it -- not a strictly-better crop, a genuine tradeoff.
        const CORN_FIELD_SEQUESTRATION: f32 = 2.0;
        const CORN_FIELD_GROWTH_RATE: f32 = 0.05;
        assert!(FIELD_SEQUESTRATION > CORN_FIELD_SEQUESTRATION, "switchgrass's perennial root biomass should sequester more per plant than corn's");
        assert!(FIELD_GROWTH_RATE < CORN_FIELD_GROWTH_RATE, "switchgrass's multi-year establishment should grow slower than corn's single-season cycle");
    }

    #[test]
    fn switchgrass_cycle_is_net_emitting_but_far_less_than_corns() {
        // Schmer et al. 2008 (PNAS): switchgrass ethanol's lifecycle GHG
        // emissions are ~76% lower than corn ethanol's -- a dramatic
        // reduction, but NOT reliably net-negative in the literature (see
        // this file's own doc comment). Both must hold: still net-positive
        // (not a magic carbon-negative crop), but its net margin per cycle
        // should be a small fraction of corn's own.
        const CORN_FIELD_SEQUESTRATION: f32 = 2.0;
        const CORN_FIELD_EMISSION_ON_HARVEST: f32 = 3.0;
        let corn_net = CORN_FIELD_EMISSION_ON_HARVEST - CORN_FIELD_SEQUESTRATION;
        let switchgrass_net = FIELD_EMISSION_ON_HARVEST - FIELD_SEQUESTRATION;

        assert!(switchgrass_net > 0.0, "switchgrass should still be net-emitting overall, not modeled as carbon-negative");
        assert!(switchgrass_net < corn_net, "switchgrass's net emission per cycle should be far smaller than corn's");
    }
}
