//! On-screen HUD showing the shared carbon budget and fuel stockpile --
//! the natural next piece once both had a real emit/sequester tension
//! between them (crop.rs sequesters on crop maturity, fuel.rs emits on
//! harvest) worth actually surfacing to the player instead of just
//! existing as internal resource state nothing displays.
//!
//! KNOWN ISSUE (2026-08-30, second diagnostic pass): still doesn't visually
//! render, but this pass ruled out the two most likely suspects with real
//! evidence, not just code-reading:
//! - `bevy_ui_render::extract_ui_camera_view` DOES attach `UiCameraView` to
//!   the world camera's render-world entity every frame (a render-world
//!   probe confirmed `UiCameraView=true` on both cameras from frame 2
//!   onward -- frame 1 alone showed 0 entities, a pipelining artifact, not
//!   a bug).
//! - `ExtractedUiNodes` genuinely receives this module's data every frame
//!   -- a second render-world probe confirmed `uinodes.len()=2` (Node +
//!   BackgroundColor, tested with an explicit 320x80 Val::Px size and a
//!   magenta color as a control) and `glyphs.len()=42` (real text glyph
//!   data) every single frame, not zero.
//! - `queue_uinodes`'s camera->view->phase lookup chain (bevy_ui_render-
//!   0.19.1 lib.rs `extract_ui_camera_view`, ~line 776-875) was read in
//!   full: it spawns a dedicated UI view entity per camera and calls
//!   `transparent_render_phases.prepare_for_new_frame(retained_view_entity)`
//!   for it -- structurally correct on paper, not yet independently
//!   verified at runtime.
//! - `RUST_LOG=wgpu=warn,wgpu_core=warn,wgpu_hal=warn,bevy_render=debug,
//!   bevy_ui_render=debug` (no rebuild needed, just an env var on the
//!   already-built exe) surfaced zero UI/pipeline/format-related warnings
//!   -- only a generic Vulkan loader startup message, arguing against (but
//!   not fully ruling out) a silently-failing shader/pipeline compile.
//!
//! Extraction is proven correct; the gap is downstream of it. Next
//! candidate, not yet tried: pipeline specialization/PipelineCache
//! resolution inside `queue_uinodes` (`pipelines.specialize(&pipeline_cache,
//! &ui_pipeline, UiPipelineKey{ target_format: view.target_format, .. })`)
//! silently returning a pipeline id that never actually finishes compiling,
//! which would make `SetItemPipeline`'s render-command execution
//! (bevy_render-0.19.1 `render_phase/mod.rs`) return `Skip` with no log --
//! needs a probe reading `Res<PipelineCache>` for that specific id's
//! compile state, or capturing wgpu's own validation layer output more
//! directly than RUST_LOG surfaced. See biofuel-climate-science-gameplay.md
//! memory for the full diagnostic trail across both passes (four real
//! missing-Plugin crashes fixed to get this far: InputPlugin,
//! TextureAtlasPlugin, SpriteRenderPlugin, plus the bevy "debug" Cargo
//! feature used transiently to name them) before re-investigating.
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
