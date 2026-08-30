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
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use crate::fuel::FuelStockpile;
use crate::water::WaterBody;
use bevy::app::{App, Update};
use bevy::color::Color;
use bevy::ecs::entity::Entity;
use bevy::prelude::{Component, Query, Res, With};
use bevy::text::{FontSize, TextColor, TextFont};
use bevy::ui::widget::Text;
use bevy::ui::{IsDefaultUiCamera, Node, PositionType, Val};

#[derive(Component)]
struct HudText;

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

    app.add_systems(Update, update_hud);
}

fn update_hud(carbon: Res<CarbonBudget>, fuel: Res<FuelStockpile>, water: Res<WaterBody>, mut text_query: Query<&mut Text, With<HudText>>) {
    let Ok(mut text) = text_query.single_mut() else {
        return;
    };
    text.0 = format!(
        "Carbon budget remaining: {:.1}\nFuel: {:.1} L\nPond pH: {:.2}",
        carbon.remaining(),
        fuel.liters(),
        water.ph()
    );
}
