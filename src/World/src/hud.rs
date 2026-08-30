//! On-screen HUD showing the shared carbon budget and fuel stockpile --
//! the natural next piece once both had a real emit/sequester tension
//! between them (crop.rs sequesters on crop maturity, fuel.rs emits on
//! harvest) worth actually surfacing to the player instead of just
//! existing as internal resource state nothing displays.
//!
//! RESOLVED (2026-08-30, third diagnostic pass): visually confirmed
//! rendering correctly in the real game after two full prior passes found
//! nothing wrong anywhere in the data pipeline. Exhaustive render-world
//! probing across all three passes proved every single stage -- extraction
//! (`ExtractedUiNodes` non-empty every frame), camera-view attachment
//! (`UiCameraView` present), phase queueing (a real `TransparentUi` item
//! queued), pipeline compilation (`PipelineCache` state `Ok`), and the
//! `UiViewTarget` -> `ViewTarget` chain (intact, zero render errors) -- all
//! succeed. Root cause found via a one-off differential test instead:
//! temporarily disabling `viewmodel.rs`'s second `Camera3d` (order=1, the
//! first-person hands) made the SAME HUD code render correctly with no
//! other changes. `bevy_render::view::ViewTarget`'s own doc comment states
//! its `main_texture` ping-pong index is "shared across view targets with
//! the same render target" -- the leading theory is the viewmodel camera's
//! own later Core3d pass (postprocess/tonemapping included) flips that
//! shared index after the world camera's own `ui_pass` already wrote to
//! it, stranding the UI draw on a slot nothing ever presents.
//!
//! Fix applied: target UI at whichever camera renders LAST (highest
//! `Camera.order`) instead of the world camera -- `session.rs` now passes
//! the viewmodel camera's own entity to `setup()` below. This sidesteps
//! the interaction rather than fixing its root cause upstream in Bevy
//! itself; if a real 2-camera-safe fix is ever needed (e.g. because a
//! future camera gets added with an even higher order), re-open
//! biofuel-climate-science-gameplay.md's full diagnostic trail before
//! re-deriving any of the above.
//!
//! Also renders a small history graph of `CarbonBudget::remaining()` --
//! the single number every other system in this game ultimately feeds
//! (crops sequester into it, fuel/hydrogen emit into it), so a trend of
//! IT alone tells the most complete "story" in the least screen space,
//! rather than graphing every individual system separately. A fixed row
//! of `HISTORY_CAPACITY` bars, each independently absolutely-positioned
//! (no flexbox/child-parent layout -- matches this file's own existing
//! `HudText` node, and avoids depending on an unconfirmed
//! `with_children`-on-`EntityWorldMut` API this codebase has never used
//! elsewhere), recomputing height+top together each frame so a bar's
//! BOTTOM edge stays visually anchored while its top edge rises/falls --
//! Bevy UI's `Val::Px` position is relative to the top edge, so anchoring
//! the bottom takes recomputing both, not just height alone.
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use crate::crop::{CropGrowth, CropSpecies};
use crate::daynight::DayNightCycle;
use crate::fuel::FuelStockpile;
use crate::hydrogen::HydrogenStockpile;
use crate::session::DeltaSeconds;
use crate::water::WaterBody;
use bevy::app::{App, Update};
use bevy::color::Color;
use bevy::ecs::entity::Entity;
use bevy::math::Vec3;
use bevy::prelude::{Component, Query, Res, ResMut, Resource, With};
use bevy::text::{FontSize, TextColor, TextFont};
use bevy::ui::widget::Text;
use bevy::ui::{BackgroundColor, IsDefaultUiCamera, Node, PositionType, Val};
use std::collections::VecDeque;

#[derive(Component)]
struct HudText;

#[derive(Component)]
struct HistoryBar(usize);

#[derive(Resource, Default)]
struct CarbonHistory {
    samples: VecDeque<f32>,
    elapsed_since_sample: f32,
}

const HISTORY_CAPACITY: usize = 24;
const HISTORY_SAMPLE_INTERVAL_SECONDS: f32 = 2.5;
const HISTORY_BAR_WIDTH_PX: f32 = 6.0;
const HISTORY_BAR_GAP_PX: f32 = 2.0;
const HISTORY_GRAPH_LEFT_PX: f32 = 12.0;
// Bumped from the graph's own original 150.0 once update_hud() gained a
// 6th (Day/Night) text line -- enough clearance below the taller text
// block that the graph's own top row doesn't visually overlap it.
const HISTORY_GRAPH_TOP_PX: f32 = 175.0;
const HISTORY_GRAPH_HEIGHT_PX: f32 = 40.0;
const HISTORY_MIN_BAR_HEIGHT_PX: f32 = 2.0;
// A visibly distinct neutral grey (darker than the healthy-green end of
// bar_color()'s own real-data range) for bar slots with no sample yet --
// e.g. the first ~60 real seconds of a session, before HISTORY_CAPACITY
// samples have accumulated -- so an empty slot reads as "no data" rather
// than misleadingly appearing as a real zero/critical reading.
const HISTORY_NO_DATA_COLOR: Color = Color::srgb(0.22, 0.22, 0.22);

// Bar height for a given remaining/total_budget reading -- pulled out as
// a pure function so the normalization curve is directly unit-testable,
// the same shape this file's own established convention (crop.rs's
// sway_angle(), fuel.rs's puff_scale()) already uses for per-frame visual
// math.
fn bar_height_px(remaining: f32, total_budget: f32) -> f32 {
    if total_budget <= 0.0 {
        return HISTORY_MIN_BAR_HEIGHT_PX;
    }
    let fraction = (remaining / total_budget).clamp(0.0, 1.0);
    HISTORY_MIN_BAR_HEIGHT_PX + fraction * (HISTORY_GRAPH_HEIGHT_PX - HISTORY_MIN_BAR_HEIGHT_PX)
}

// A healthy-green (budget mostly intact) to critical-red (budget mostly
// or fully consumed) ramp -- the same "color communicates state at a
// glance" idea water.rs's own ph_to_color() already establishes for the
// pond.
fn bar_color(remaining: f32, total_budget: f32) -> Color {
    if total_budget <= 0.0 {
        return Color::srgb(0.85, 0.25, 0.2);
    }
    let fraction = (remaining / total_budget).clamp(0.0, 1.0);
    let critical = Vec3::new(0.85, 0.25, 0.2);
    let healthy = Vec3::new(0.25, 0.75, 0.35);
    let mixed = critical.lerp(healthy, fraction);
    Color::srgb(mixed.x, mixed.y, mixed.z)
}

// ui_target_camera: the entity IsDefaultUiCamera gets attached to -- must
// be whichever of this session's two Camera3d entities renders LAST (see
// this module's own doc comment for why); session.rs passes the viewmodel
// camera's own entity, not the world camera's.
pub(crate) fn setup(app: &mut App, ui_target_camera: Entity) {
    app.world_mut().entity_mut(ui_target_camera).insert(IsDefaultUiCamera);

    app.world_mut().spawn((
        Node {
            position_type: PositionType::Absolute,
            top: Val::Px(12.0),
            left: Val::Px(12.0),
            ..Default::default()
        },
        Text::new(""),
        TextFont {
            font_size: FontSize::Px(20.0),
            ..Default::default()
        },
        TextColor(Color::WHITE),
        HudText,
    ));

    for i in 0..HISTORY_CAPACITY {
        let left = HISTORY_GRAPH_LEFT_PX + i as f32 * (HISTORY_BAR_WIDTH_PX + HISTORY_BAR_GAP_PX);
        app.world_mut().spawn((
            Node {
                position_type: PositionType::Absolute,
                left: Val::Px(left),
                top: Val::Px(HISTORY_GRAPH_TOP_PX + HISTORY_GRAPH_HEIGHT_PX - HISTORY_MIN_BAR_HEIGHT_PX),
                width: Val::Px(HISTORY_BAR_WIDTH_PX),
                height: Val::Px(HISTORY_MIN_BAR_HEIGHT_PX),
                ..Default::default()
            },
            BackgroundColor(HISTORY_NO_DATA_COLOR),
            HistoryBar(i),
        ));
    }

    app.insert_resource(CarbonHistory::default());
    app.add_systems(Update, (update_hud, sample_carbon_history, update_history_bars));
}

fn sample_carbon_history(dt: Res<DeltaSeconds>, carbon: Res<CarbonBudget>, mut history: ResMut<CarbonHistory>) {
    history.elapsed_since_sample += dt.0;
    if history.elapsed_since_sample < HISTORY_SAMPLE_INTERVAL_SECONDS {
        return;
    }
    history.elapsed_since_sample = 0.0;
    history.samples.push_back(carbon.remaining());
    if history.samples.len() > HISTORY_CAPACITY {
        history.samples.pop_front();
    }
}

fn update_history_bars(carbon: Res<CarbonBudget>, history: Res<CarbonHistory>, mut bars: Query<(&HistoryBar, &mut Node, &mut BackgroundColor)>) {
    let total_budget = carbon.total_budget();
    for (bar, mut node, mut color) in &mut bars {
        let Some(&remaining) = history.samples.get(bar.0) else {
            node.height = Val::Px(HISTORY_MIN_BAR_HEIGHT_PX);
            node.top = Val::Px(HISTORY_GRAPH_TOP_PX + HISTORY_GRAPH_HEIGHT_PX - HISTORY_MIN_BAR_HEIGHT_PX);
            color.0 = HISTORY_NO_DATA_COLOR;
            continue;
        };
        let height = bar_height_px(remaining, total_budget);
        node.height = Val::Px(height);
        node.top = Val::Px(HISTORY_GRAPH_TOP_PX + HISTORY_GRAPH_HEIGHT_PX - height);
        color.0 = bar_color(remaining, total_budget);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_full_budget_reaches_max_bar_height_and_an_exhausted_one_reaches_the_floor() {
        let total_budget = 1000.0;
        assert!(
            (bar_height_px(total_budget, total_budget) - HISTORY_GRAPH_HEIGHT_PX).abs() < 1.0e-6,
            "a fully-intact budget should render at the graph's own configured max height"
        );
        assert!(
            (bar_height_px(0.0, total_budget) - HISTORY_MIN_BAR_HEIGHT_PX).abs() < 1.0e-6,
            "a fully-exhausted budget should render at the floor height, not vanish to zero"
        );
        assert!(
            (bar_height_px(-500.0, total_budget) - HISTORY_MIN_BAR_HEIGHT_PX).abs() < 1.0e-6,
            "an overshot (negative remaining) budget should clamp to the same floor height as exactly-exhausted, not go negative"
        );
    }

    #[test]
    fn bar_color_ramps_from_critical_red_toward_healthy_green_as_budget_recovers() {
        let total_budget = 1000.0;
        let Color::Srgba(critical) = bar_color(0.0, total_budget) else {
            panic!("expected an Srgba color");
        };
        let Color::Srgba(healthy) = bar_color(total_budget, total_budget) else {
            panic!("expected an Srgba color");
        };

        assert!(critical.red > critical.green, "a fully-consumed budget should read as red-dominant (critical)");
        assert!(healthy.green > healthy.red, "a fully-intact budget should read as green-dominant (healthy)");
    }
}

fn update_hud(
    carbon: Res<CarbonBudget>,
    fuel: Res<FuelStockpile>,
    water: Res<WaterBody>,
    hydrogen: Res<HydrogenStockpile>,
    daynight: Res<DayNightCycle>,
    crops: Query<&CropGrowth>,
    mut text_query: Query<&mut Text, With<HudText>>,
) {
    let Ok(mut text) = text_query.single_mut() else {
        return;
    };
    // is_daytime(), not a light_level() threshold -- see that method's
    // own doc comment for why light_level() alone can't distinguish a
    // dim dawn/dusk moment from genuine night.
    let day_or_night = if daynight.is_daytime() { "Day" } else { "Night" };
    // The warning line only appears once WaterBody::has_overshot_optimal()
    // itself goes true (past the real nutrient-uptake optimum, not merely
    // "pH has moved") -- see that method's own doc comment for why a
    // naive "changed at all" check would misleadingly fire during the
    // healthy half of the pond's own peaked pH curve.
    let overshoot_warning = if water.has_overshot_optimal() {
        "\n! Pond has overshot its optimal pH -- irrigation quality is now declining"
    } else {
        ""
    };

    // Counts every crop currently alive (seedling through mature), not
    // just harvestable ones -- shows the farm's actual composition, which
    // fuel.rs's own Fuel/kg reading alone can't convey (a field of
    // seedlings looks identical to an empty field on that number alone).
    let (mut corn, mut switchgrass, mut miscanthus) = (0u32, 0u32, 0u32);
    for crop in &crops {
        match crop.species() {
            CropSpecies::Corn => corn += 1,
            CropSpecies::Switchgrass => switchgrass += 1,
            CropSpecies::Miscanthus => miscanthus += 1,
        }
    }

    text.0 = format!(
        "{}\nCarbon budget remaining: {:.1}\nFuel: {:.1} L\nPond pH: {:.2}\nHydrogen: {:.2} kg\nCorn: {} | Switchgrass: {} | Miscanthus: {}{}",
        day_or_night,
        carbon.remaining(),
        fuel.liters(),
        water.ph(),
        hydrogen.kg(),
        corn,
        switchgrass,
        miscanthus,
        overshoot_warning
    );
}
