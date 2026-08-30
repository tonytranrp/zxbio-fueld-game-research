//! On-screen HUD showing the shared carbon budget and fuel stockpile --
//! the natural next piece once both had a real emit/sequester tension
//! between them (crop.rs sequesters on crop maturity, fuel.rs emits on
//! harvest) worth actually surfacing to the player instead of just
//! existing as internal resource state nothing displays.
//!
//! KNOWN ISSUE (2026-08-30): the underlying data pipeline is fully verified
//! correct in the real running game -- update_hud runs every frame, its
//! Query<&mut Text> succeeds, and the HudText Node's ComputedNode reports a
//! correct non-degenerate computed size (320x80 physical px, scale factor
//! 1.0) -- but the text (and even a magenta BackgroundColor swapped in as a
//! control test) never visually appears on screen. This session has two
//! Camera3d entities (session.rs's world camera, order 0, and viewmodel.rs's
//! hands camera, order 1, ClearColorConfig::None) and IsDefaultUiCamera is
//! attached to the world camera, which per bevy_ui's own DefaultUiCamera
//! resolution should be sufficient -- root cause not yet found; likely a
//! bevy_ui_render extraction/camera-mapping edge case specific to targeting
//! a non-default (non-single, Camera3d-only, no Camera2d) camera setup, not
//! a bug in this file. See biofuel-climate-science-gameplay.md memory for
//! the full diagnostic trail (four real missing-Plugin crashes fixed to get
//! this far: InputPlugin, TextureAtlasPlugin, SpriteRenderPlugin, plus the
//! bevy "debug" Cargo feature used transiently to name them) before
//! re-investigating.
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use crate::fuel::FuelStockpile;
use bevy::app::{App, Update};
use bevy::color::Color;
use bevy::ecs::entity::Entity;
use bevy::prelude::{Component, Query, Res, With};
use bevy::text::{FontSize, TextColor, TextFont};
use bevy::ui::widget::Text;
use bevy::ui::{IsDefaultUiCamera, Node, PositionType, Val};

#[derive(Component)]
struct HudText;

// world_camera: the entity IsDefaultUiCamera gets attached to, so UI
// resolves against the tracking world camera rather than viewmodel.rs's
// fixed hands camera or an ambiguous default -- there are two Camera3d
// entities in this session (see session.rs's own WorldCamera marker doc
// comment for the real bug that ambiguity caused elsewhere), and Bevy UI
// needs an explicit choice here for the same reason.
pub(crate) fn setup(app: &mut App, world_camera: Entity) {
    app.world_mut().entity_mut(world_camera).insert(IsDefaultUiCamera);

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

fn update_hud(carbon: Res<CarbonBudget>, fuel: Res<FuelStockpile>, mut text_query: Query<&mut Text, With<HudText>>) {
    let Ok(mut text) = text_query.single_mut() else {
        return;
    };
    text.0 = format!("Carbon budget remaining: {:.1}\nFuel: {:.1} L", carbon.remaining(), fuel.liters());
}
