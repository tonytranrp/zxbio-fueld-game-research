//! Third crop: giant miscanthus (Miscanthus x giganteus), a second
//! cellulosic/advanced-generation feedstock planted alongside switchgrass.rs's
//! own field. Where switchgrass exists to show "advanced-generation is
//! dramatically better than corn but still not a free lunch" (see its own
//! doc comment -- still net-emitting overall), miscanthus exists to show the
//! feedstock choice that genuinely IS net carbon-negative in the literature
//! -- completing a real three-tier comparison: corn (net-emitting, worst),
//! switchgrass (net-emitting, far less), miscanthus (net-negative). Reuses
//! crop.rs's own CropGrowth component unchanged, same as switchgrass.rs.
//!
//! Every constant below is calibrated against real, cited research (see
//! biofuel-climate-science-gameplay.md's research summary for the full
//! source list):
//!
//! - Net-negative lifecycle GHG: Qin et al. 2016 (GCB Bioenergy,
//!   soil-carbon-inclusive lifecycle modeling) found miscanthus at -0.6 to
//!   -7 gCO2e/MJ -- genuinely net-negative, unlike switchgrass's own 18-26
//!   (still positive) or corn's 59-66 in the same study. This is the ONE
//!   feedstock in this game's three crops actually modeled with
//!   emission_on_harvest LESS than sequestration_on_maturity.
//! - Yield: a 2021 North Carolina Piedmont field study (water-use
//!   comparison, see below) measured giant miscanthus at 29.1 Mg/ha
//!   2-year-average biomass vs. switchgrass's own 14.2 Mg/ha in the SAME
//!   study -- roughly double, consistent with Iqbal et al. 2015's separate
//!   16.2 vs 10.2 t/ha/yr figures already cited in switchgrass.rs. Modeled
//!   here as a meaningfully higher sequestration_on_maturity than
//!   switchgrass's own 2.2.
//! - Water: that same 2021 study found giant miscanthus and switchgrass had
//!   SIMILAR seasonal water use, but miscanthus had higher water-use
//!   EFFICIENCY specifically because of its much greater biomass yield per
//!   unit water, not because it draws less water -- the real advantage is
//!   yield-per-drop, which this game already represents via the higher
//!   sequestration_on_maturity above, not via a different water-fraction
//!   value. Reuses switchgrass's own water=0.7 marginal-land/deep-root
//!   framing rather than inventing an unsupported "even less water than
//!   switchgrass" claim.
//! - Establishment: 2-3 growing seasons to full yield (Univ. Maryland
//!   Extension FS-2024-0734; Penn State Extension energy-crop profile),
//!   comparable to switchgrass's own multi-year establishment -- growth
//!   pace kept in the same range as switchgrass.rs's own FIELD_GROWTH_RATE,
//!   not modeled as meaningfully faster or slower given the two studies
//!   describe similar timelines.
//! - Propagation: a sterile hybrid (produces no viable seed), propagated
//!   asexually via rhizomes/plugs rather than seed like switchgrass or corn
//!   -- a real, distinct agronomic fact not yet modeled numerically (no
//!   planting-cost/method resource exists yet), noted here as a candidate
//!   for a future "establishment cost" mechanic once one does.
#![forbid(unsafe_code)]

use crate::crop::CropGrowth;
use bevy::app::{App, Update};
use bevy::asset::{AssetServer, Assets, Handle};
use bevy::ecs::system::Local;
use bevy::gltf::Gltf;
use bevy::prelude::{Commands, Res, Resource, Transform};
use bevy::world_serialization::WorldAssetRoot;

#[derive(Resource)]
struct MiscanthusFieldAssets {
    gltf: Handle<Gltf>,
}

// Same (water, light, co2) convention as crop.rs's/switchgrass.rs's own
// FIELD_SUPPLY -- see this file's own doc comment on why water reuses
// switchgrass's 0.7 rather than an unsupported lower value.
const FIELD_SUPPLY: [(f32, f32, f32); 4] = [(0.7, 1.0, 1.0), (0.7, 1.0, 1.0), (0.7, 1.0, 1.0), (0.7, 1.0, 1.0)];
const FIELD_COL_SPACING: f32 = 1.0;
const FIELD_ROW_SPACING: f32 = 0.6;
// Comparable to switchgrass.rs's own FIELD_GROWTH_RATE (0.02) -- both real
// studies cited in this file's own doc comment describe similar multi-year
// establishment timelines, not a meaningfully faster or slower one.
pub(crate) const FIELD_GROWTH_RATE: f32 = 0.02;
// Roughly double switchgrass.rs's own FIELD_SEQUESTRATION (2.2), matching
// the ~2x real yield ratio this file's own doc comment cites (29.1 vs 14.2
// Mg/ha; separately corroborated by Iqbal et al. 2015's 16.2 vs 10.2
// t/ha/yr).
pub(crate) const FIELD_SEQUESTRATION: f32 = 4.0;
// Calibrated so this crop's net-per-cycle (emission - sequestration) is
// NEGATIVE, unlike corn's (+1.0) or switchgrass's own (+0.3) -- the one
// real, qualitative fact this crop exists to surface (see this file's own
// doc comment's Qin et al. 2016 citation: miscanthus is the feedstock most
// consistently found net carbon-negative in soil-carbon-inclusive lifecycle
// models, unlike switchgrass).
pub(crate) const FIELD_EMISSION_ON_HARVEST: f32 = 2.0;
// Between corn's 0.03 and switchgrass's 0.15 (see crop.rs's own
// FIELD_SWAY_AMPLITUDE and switchgrass.rs's own equivalent for the general
// reasoning) -- miscanthus's real bamboo-like canes (this file's own
// generation prompt) are stiffer than switchgrass's fine blade-like
// leaves, but still more flexible than corn's own thicker stalk.
pub(crate) const FIELD_SWAY_AMPLITUDE: f32 = 0.10;

// Same Z depth as switchgrass.rs's own field (Z=-4.2/-3.6, farther from the
// player than corn's own Z=-6.0/-4.8 row, hence a wider camera frustum at
// that distance -- see switchgrass.rs's own placement doc comment for the
// screenshot-verified reasoning this reuses), offset to the right in X
// (1.2/1.7) rather than stacking a third row into the same already-narrow
// X range switchgrass uses. Checked clear of level.rs's drum obstacles
// (X=3.2/3.9, so this field's max X=1.7 sits comfortably short of them) and
// far enough right of the crates (X=-3.0/-2.0) to not matter. Kept
// conservative relative to the frustum-width estimate this depth affords
// (switchgrass.rs's own doc comment estimates roughly +-2.0..2.3 possible)
// specifically because that estimate was derived from limited data points,
// not a precisely known FOV value.
pub(crate) fn setup(app: &mut App) {
    let gltf = app.world().resource::<AssetServer>().load("models/miscanthus/miscanthus.glb");
    app.insert_resource(MiscanthusFieldAssets { gltf });
    app.add_systems(Update, spawn_miscanthus_field_once_loaded);
}

fn spawn_miscanthus_field_once_loaded(
    mut commands: Commands,
    assets: Res<MiscanthusFieldAssets>,
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
        let x = 1.2 + col * FIELD_COL_SPACING;
        let z = -4.2 + row * FIELD_ROW_SPACING;
        commands.spawn((
            WorldAssetRoot(scene.clone()),
            Transform::from_xyz(x, 0.0, z),
            CropGrowth::new(light, co2, water, FIELD_GROWTH_RATE, FIELD_SEQUESTRATION, FIELD_EMISSION_ON_HARVEST, FIELD_SWAY_AMPLITUDE, crate::crop::CropSpecies::Miscanthus),
        ));
    }

    *spawned = true;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn miscanthus_cycle_is_net_negative_unlike_corn_and_switchgrass() {
        // The real, distinguishing fact this crop exists to surface: unlike
        // corn or switchgrass (both still net-emitting overall, see
        // crop.rs's/switchgrass.rs's own equivalent tests), a full
        // miscanthus grow-then-harvest cycle should sequester more than it
        // emits.
        let net = FIELD_EMISSION_ON_HARVEST - FIELD_SEQUESTRATION;
        assert!(net < 0.0, "miscanthus's net emission per cycle should be negative (net carbon-sequestering), unlike corn's or switchgrass's own positive net");
    }

    #[test]
    fn miscanthus_sequesters_more_per_plant_than_switchgrass() {
        const SWITCHGRASS_FIELD_SEQUESTRATION: f32 = 2.2;
        assert!(
            FIELD_SEQUESTRATION > SWITCHGRASS_FIELD_SEQUESTRATION,
            "miscanthus's real ~2x yield advantage over switchgrass should show up as higher sequestration per plant"
        );
    }
}
