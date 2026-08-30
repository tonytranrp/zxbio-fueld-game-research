//! One gameplay session: opens a window, boots a real `bevy_app::App`
//! running `bevy_render`'s own renderer (pinned to the Vulkan backend),
//! spawns the ported level geometry and a first-person player
//! (`level.rs`/`player.rs`/`fp_camera.rs`), and returns once the player
//! closes the window (or a clean failure occurs).
//!
//! Deliberately does NOT use `bevy_winit`/`WinitPlugin`: that plugin always
//! builds a fresh `winit::event_loop::EventLoop`, which panics on the
//! second call in one process (see event_loop_cell.rs's doc comment) --
//! Phase 1(a) proved a hand-rolled runner against one persisted `EventLoop`
//! survives repeated sessions, so this Bevy integration is built directly
//! on top of that same runner instead of `WinitPlugin`. The bridge to
//! `bevy_render` is `bevy_window::RawHandleWrapper` -- `bevy_render`'s own
//! `WindowRenderPlugin`/`extract_windows` system (registered automatically
//! by `RenderPlugin`, confirmed from `bevy_render-0.19.1/src/lib.rs`) picks
//! up ANY entity with `Window` + `RawHandleWrapper` components and creates
//! a surface for it -- it doesn't care whether `WinitPlugin` was the thing
//! that spawned that entity.
//!
//! Because `WinitPlugin` is never used, this crate also owns input
//! delivery by hand: `ApplicationHandler::window_event`'s `KeyboardInput`
//! arm and `device_event`'s `DeviceEvent::MouseMotion` arm feed
//! `input_state::InputState`, which `update_player_and_camera` (the one
//! `Update`-schedule system this session runs) polls once per frame --
//! mirroring the poll-once-per-frame shape the ported C++ movement/camera
//! code (`CharacterController3D`/`FirstPersonCamera`) was written against.
//! Frame `dt` is likewise computed by hand (`DeltaSeconds`, driven from
//! `std::time::Instant` in `window_event`'s `RedrawRequested` arm) rather
//! than trusting `bevy_time`'s `Res<Time>` under a runner that never calls
//! `App::run()` -- same reasoning `Engine/game/src/world.rs` already
//! applied by taking `dt` as a plain function argument instead.
#![forbid(unsafe_code)]

use std::sync::Arc;
use std::time::Instant;

use bevy::app::App;
// bevy::MinimalPlugins, not bevy::app::MinimalPlugins -- it lives at the
// crate root (bevy_internal's `pub use bevy_internal::*;`), same lesson
// already learned once in Engine/game's own bevy usage this session.
use bevy::MinimalPlugins;
use bevy::asset::AssetPlugin;
use bevy::camera::{Camera, Camera3d, ClearColor, ClearColorConfig};
use bevy::color::Color;
use bevy::core_pipeline::tonemapping::Tonemapping;
use bevy::core_pipeline::CorePipelinePlugin;
use bevy::ecs::system::{Query, Res, ResMut};
use bevy::image::{ImagePlugin, TextureAtlasPlugin};
use bevy::math::Vec3;
use bevy::mesh::MeshPlugin;
use bevy::pbr::PbrPlugin;
use bevy::prelude::{Resource, Transform, With};
use bevy::render::settings::{Backends, WgpuSettings};
use bevy::render::RenderPlugin;
use bevy::tasks::tick_global_task_pools_on_main_thread;
use bevy::app::{PluginsState, Update};
use bevy::window::{
    PrimaryWindow, RawHandleWrapper, Window as BevyWindow, WindowPlugin, WindowResolution, WindowWrapper,
};

use winit::application::ApplicationHandler;
use winit::event::{DeviceEvent, DeviceId, ElementState, MouseButton, WindowEvent};
use winit::event_loop::ActiveEventLoop;
use winit::keyboard::{KeyCode, PhysicalKey};
use winit::platform::run_on_demand::EventLoopExtRunOnDemand;
use winit::window::{CursorGrabMode, Window as WinitWindow, WindowId};

use crate::carbon::CarbonBudget;
use crate::fp_camera::FirstPersonCamera;
use crate::input_state::InputState;
use crate::physics::RapierPhysics;
use crate::player::{self, PlayerController};
use crate::{adapter_probe, crop, event_loop_cell, fuel, hud, hydrogen, level, miscanthus, solar, switchgrass, viewmodel, water};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum SessionExitReason {
    ReturnedToMenu,
    VulkanUnavailable,
    InternalError,
}

// Sky/background clear color -- an arbitrary dusk blue-grey, swapped for a
// real sky (or removed entirely once the level is fully enclosed) in a
// later phase.
const CLEAR_COLOR: Color = Color::srgb(0.35, 0.42, 0.55);

// Frame delta time, computed by hand in window_event's RedrawRequested arm
// -- see this module's doc comment for why Res<Time> isn't used instead.
// pub(crate): every gameplay system added since (crop.rs, and whatever
// follows) needs frame dt the same way update_player_and_camera does below,
// not just this module's own system.
#[derive(Resource, Default)]
pub(crate) struct DeltaSeconds(pub(crate) f32);

// Marker distinguishing the world camera (this module's own spawn, tracks
// the player every frame) from viewmodel.rs's fixed, non-tracking hands
// camera -- both are Camera3d, so a query filtered on just With<Camera3d>
// matches both and Query::single_mut() silently returns Err every frame
// once there are two (never panics, just never updates anything). Real bug
// this fixed: once viewmodel.rs's camera existed, the world camera's
// Transform was frozen at its spawn-time value forever, confirmed by a
// debug probe logging camera3d_count=2 and an unchanging pos/rot across 60
// frames -- the player/look code was computing the right transform the
// whole time, it just had nowhere valid to write it.
#[derive(bevy::prelude::Component)]
struct WorldCamera;

#[derive(Default)]
struct SessionApp {
    app: Option<App>,
    // Kept alive for the session's duration -- RawHandleWrapper only holds
    // a type-erased Arc clone of the same underlying window, not a typed
    // handle back to it, and this is also what receives request_redraw()
    // and the cursor-grab calls.
    winit_window: Option<Arc<WindowWrapper<WinitWindow>>>,
    last_frame: Option<Instant>,
    failed: bool,
}

impl SessionApp {
    fn fail(&mut self, event_loop: &ActiveEventLoop) {
        self.failed = true;
        event_loop.exit();
    }
}

fn update_player_and_camera(
    dt: Res<DeltaSeconds>,
    mut input: ResMut<InputState>,
    mut physics: ResMut<RapierPhysics>,
    mut player: ResMut<PlayerController>,
    mut camera_state: ResMut<FirstPersonCamera>,
    mut camera_query: Query<&mut Transform, With<WorldCamera>>,
) {
    let dt = dt.0;
    let (dx, dy) = input.take_mouse_delta();
    camera_state.add_look_delta(dx, dy);

    let mut axis_x = 0.0;
    let mut axis_y = 0.0;
    if input.is_down(KeyCode::KeyD) {
        axis_x += 1.0;
    }
    if input.is_down(KeyCode::KeyA) {
        axis_x -= 1.0;
    }
    if input.is_down(KeyCode::KeyW) {
        axis_y += 1.0;
    }
    if input.is_down(KeyCode::KeyS) {
        axis_y -= 1.0;
    }

    let move_input = player::MoveInput {
        move_axis: (axis_x, axis_y),
        yaw_radians: camera_state.yaw(),
        sprint: input.is_down(KeyCode::ShiftLeft),
        jump: input.is_down(KeyCode::Space),
    };

    physics.step(dt);
    player.step(&physics, &move_input, dt);
    player.sync_body_position(&mut physics);

    camera_state.update_bob(player.horizontal_speed(), dt);

    if let Ok(mut transform) = camera_query.single_mut() {
        let p = player.position();
        let eye = Vec3::new(p.x, p.y + player.eye_height(), p.z);
        transform.translation = eye + camera_state.bob_offset();
        transform.rotation = camera_state.rotation();
    }
}

impl ApplicationHandler for SessionApp {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        let attrs = WinitWindow::default_attributes().with_title("Fuel Farm");
        let winit_window = match event_loop.create_window(attrs) {
            Ok(w) => w,
            Err(_) => return self.fail(event_loop),
        };
        // FPS-style mouse look: lock the cursor in place and hide it,
        // mirroring raylib's DisableCursor() (used by the C++
        // ExplorationScreen this session's player controller ports).
        // Locked isn't supported on every platform (X11 in particular) --
        // fall back to Confined, which at least keeps the cursor from
        // escaping the window; either way hide it explicitly, since neither
        // mode guarantees that on its own per winit's own docs.
        if winit_window.set_cursor_grab(CursorGrabMode::Locked).is_err() {
            let _ = winit_window.set_cursor_grab(CursorGrabMode::Confined);
        }
        winit_window.set_cursor_visible(false);

        let size = winit_window.inner_size();
        let wrapped = Arc::new(WindowWrapper::new(winit_window));

        let raw_handle = match RawHandleWrapper::new(&wrapped) {
            Ok(h) => h,
            Err(_) => return self.fail(event_loop),
        };

        let mut app = App::new();
        // bevy_render::view::prepare_view_targets reads this global
        // resource unconditionally, even though this session's one camera
        // overrides it per-camera via ClearColorConfig::Custom below.
        app.insert_resource(ClearColor(CLEAR_COLOR));
        // MinimalPlugins first: AssetPlugin's async loading needs
        // IoTaskPool, which only exists once TaskPoolPlugin (part of this
        // group) has run.
        app.add_plugins(MinimalPlugins);
        // Nothing else in this crate's deliberately minimal plugin set
        // installs a tracing subscriber -- without this, every warn!/
        // error! Bevy's own internals emit (real example this phase: an
        // unbound-tonemapping-LUT error that was the actual cause of the
        // viewmodel hands rendering solid magenta) goes out silently. Kept
        // permanently, not just for that one diagnosis -- worth having for
        // whatever the next Bevy-internals surprise turns out to be.
        app.add_plugins(bevy::log::LogPlugin::default());
        // MinimalPlugins is deliberately minimal enough that it does NOT
        // include this -- confirmed by reading bevy_internal-0.19.1/src/
        // default_plugins.rs's own MinimalPlugins group definition (just
        // TaskPoolPlugin/FrameCountPlugin/TimePlugin/ScheduleRunnerPlugin).
        // Without it, Transform -> GlobalTransform propagation never runs,
        // so every entity (including the camera) renders at
        // GlobalTransform::IDENTITY regardless of what its own Transform is
        // set to -- confirmed by a real repro: the level and camera
        // rendered as a single uniform fill color with zero visible
        // geometry until this was added, even though per-frame debug
        // logging showed the camera's own Transform being computed
        // correctly.
        app.add_plugins(bevy::transform::plugins::TransformPlugin);
        // Registers the Window-related message/resource types bevy_render's
        // own systems expect to exist -- bevy_window's plugin, unrelated to
        // bevy_winit's WinitPlugin (still correctly not used here).
        // primary_window: None so it doesn't spawn its own competing
        // PrimaryWindow entity -- this code spawns its own below, wired to
        // the real winit window instead of one WindowPlugin would create
        // itself.
        app.add_plugins(WindowPlugin {
            primary_window: None,
            ..Default::default()
        });
        // bevy_camera::CameraPlugin bundles VisibilityPlugin (computes
        // InheritedVisibility/ViewVisibility from frustum culling) and
        // CameraProjectionPlugin -- not auto-added by RenderPlugin/PbrPlugin
        // despite bevy_camera being a cascaded Cargo dependency of both,
        // same "types come along, the Plugin registration doesn't"
        // gotcha already hit once for LightPlugin. Confirmed necessary by a
        // real repro: without it, every spawned entity's ViewVisibility
        // stays at its default (hidden), so nothing ever renders and
        // nothing panics either -- a silent, not an error, failure mode,
        // unlike every other missing piece found so far this phase.
        app.add_plugins(bevy::camera::CameraPlugin);
        // Must precede RenderPlugin: RenderPlugin::build() itself calls
        // app.init_asset::<Shader>(), which needs AssetPlugin's resources
        // already registered.
        app.add_plugins(AssetPlugin::default());
        // TonemappingPlugin (added transitively by CorePipelinePlugin
        // below) unconditionally touches an Assets<Image> resource even
        // with tonemapping_luts disabled.
        app.add_plugins(ImagePlugin::default());
        // Registers the Mesh asset type -- Core3d's render pipeline reads
        // an AssetEvent<Mesh> message reader unconditionally.
        app.add_plugins(MeshPlugin);
        // hud.rs's Text/Node don't use image content sizing, but bevy_ui's
        // own update_image_content_size_system runs unconditionally once
        // UiPlugin is added and reads Res<Assets<TextureAtlasLayout>> --
        // confirmed by a real crash without this. That asset type is
        // registered by bevy_image::TextureAtlasPlugin::build() (a single
        // app.init_asset::<TextureAtlasLayout>() call), same "cascaded
        // Cargo dependency, not an auto-added Plugin" pattern as this
        // session's other fixes.
        app.add_plugins(TextureAtlasPlugin);
        app.add_plugins(RenderPlugin {
            render_creation: WgpuSettings {
                backends: Some(Backends::VULKAN),
                ..Default::default()
            }
            .into(),
            ..Default::default()
        });
        app.add_plugins(CorePipelinePlugin);
        // Phase 4: must precede PbrPlugin below -- PbrPlugin::build() itself
        // calls bevy_pbr::gltf::add_gltf() when its own "bevy_gltf" Cargo
        // feature is on (cascaded automatically by this crate's own
        // "bevy_gltf" feature, see Cargo.toml), which does
        // world.resource_mut::<GltfExtensionHandlers>() -- a resource only
        // GltfPlugin::build() inserts. Confirmed by a real panic with the
        // plugins in the other order ("Requested resource ... does not
        // exist", inside bevy_pbr::gltf::add_gltf). AnimationPlugin
        // (AnimationPlayer/AnimationGraph playback) and
        // WorldSerializationPlugin (turns a WorldAssetRoot component into
        // an actually-instantiated child entity hierarchy) are each their
        // own plugin too, same "cascaded Cargo dependency, but the Plugin
        // that registers it isn't auto-added" pattern hit repeatedly this
        // phase and last -- order between these three doesn't matter, only
        // being before PbrPlugin does.
        app.add_plugins(bevy::gltf::GltfPlugin::default());
        app.add_plugins(bevy::animation::AnimationPlugin);
        app.add_plugins(bevy::world_serialization::WorldSerializationPlugin);
        // Phase 3: real scene content needs Mesh3d/MeshMaterial3d<
        // StandardMaterial> and a real Camera3d, which PbrPlugin registers.
        // LightPlugin is a separate, NOT-auto-bundled plugin from the
        // bevy_light crate (cascaded as a Cargo dependency by the "bevy_pbr"
        // feature, but its own Plugin type isn't added by PbrPlugin::build()
        // -- confirmed by a real panic without it: bevy_pbr::render::light::
        // extract_lights reads Res<PointLightShadowMap>, which only
        // LightPlugin::build() inserts). No light entities are spawned this
        // phase (level.rs's materials are unlit) but the extraction systems
        // still run every frame regardless of whether any light exists.
        app.add_plugins(PbrPlugin {
            // Simpler forward-rendering pipeline instead of PbrPlugin's
            // default deferred setup + prepass -- fewer moving parts while
            // this phase is just proving solid-color boxes render at all;
            // revisit if a later phase's needs (SSAO, decals, ...) call for
            // deferred specifically.
            add_default_deferred_lighting_plugin: false,
            prepass_enabled: false,
            ..Default::default()
        });
        app.add_plugins(bevy::light::LightPlugin);
        // hud.rs's on-screen text. TextPlugin registers font/glyph-layout
        // systems, UiPlugin the layout (Node/flexbox) systems, UiRenderPlugin
        // the actual render-graph node that draws it -- three separate
        // plugins across three separate crates, same "cascaded Cargo
        // dependency, not an auto-added Plugin" pattern hit repeatedly this
        // migration (Light/Gltf/Animation/WorldSerialization above).
        app.add_plugins(bevy::text::TextPlugin);
        app.add_plugins(bevy::ui::UiPlugin);
        app.add_plugins(bevy::ui_render::UiRenderPlugin);
        // bevy_ui_render's own prepare_uinodes (render-world, runs even for
        // hud.rs's plain Text/Node, no Sprite entities anywhere in this
        // crate) reads the render sub-app's Res<SpriteAssetEvents> --
        // confirmed by a real crash without this. That resource is
        // registered by bevy_sprite_render::SpriteRenderPlugin::build() (on
        // RenderApp, not the main app -- source note: bevy_sprite_render-
        // 0.19.1/src/lib.rs's SpriteRenderPlugin impl), which the
        // "bevy_ui_render" Cargo feature pulls in as a compiled dependency
        // (confirmed: SpriteRenderPlugin's own build() early-returns via
        // is_plugin_added::<TextureAtlasPlugin> reuse, so no conflict with
        // this file's own explicit TextureAtlasPlugin above) but never
        // instantiates as a Plugin -- same pattern as every other fix in
        // this block.
        app.add_plugins(bevy::sprite_render::SpriteRenderPlugin);
        // UiPlugin's own ui_focus_system (hover/click bookkeeping for
        // Interaction, unused by hud.rs's plain read-only Text/Node but
        // still scheduled unconditionally by UiPlugin) reads
        // Res<ButtonInput<MouseButton>> -- confirmed by a real crash without
        // this ("Resource does not exist", name only visible with Cargo's
        // "debug" feature temporarily enabled: bevy_ui::focus::ui_focus_
        // system / Res<ButtonInput<MouseButton>>). bevy_input is a hard
        // (non-optional) dependency of bevy_internal already, so this needs
        // no new Cargo feature -- just the Plugin, which this crate's
        // deliberately-minimal set (no DefaultPlugins, no bevy_winit) never
        // added. Its build() only registers messages/resources and systems
        // that consume them; since nothing here emits winit-sourced
        // MouseButtonInput events, ButtonInput<MouseButton> simply stays at
        // its default "nothing pressed" state forever, which is fine since
        // this crate does not use bevy_ui's Interaction/Button widgets for
        // real input -- input_state.rs's own hand-rolled path is what
        // actually drives gameplay.
        app.add_plugins(bevy::input::InputPlugin);

        let bevy_window = BevyWindow {
            resolution: WindowResolution::new(size.width, size.height),
            title: "Fuel Farm".to_string(),
            ..Default::default()
        };
        app.world_mut().spawn((bevy_window, raw_handle, PrimaryWindow));

        // Canonical bootstrap sequence bevy_app's own default runner uses
        // before its first update() -- RenderPlugin creates its Device/
        // Queue asynchronously and only actually unpacks them into the app
        // inside Plugin::finish(), which nothing calls automatically when
        // App::run() itself is never invoked (this code only ever calls
        // .update()). plugins_state() also drives Plugin::ready() to
        // completion, which is what the tick_global_task_pools_on_main_thread
        // loop is for -- without ticking, the async future backing
        // FutureRenderResources never gets polled to readiness.
        while app.plugins_state() == PluginsState::Adding {
            tick_global_task_pools_on_main_thread();
        }
        app.finish();
        app.cleanup();

        // ---- Phase 3: physics world, level geometry, player, camera ----
        app.insert_resource(RapierPhysics::new());
        level::spawn_level(&mut app);

        let player_controller = {
            let mut physics = app.world_mut().resource_mut::<RapierPhysics>();
            PlayerController::spawn(&mut physics, level::PLAYER_SPAWN, player::Config::default())
        };
        app.insert_resource(player_controller);
        app.insert_resource(FirstPersonCamera::default());
        app.insert_resource(InputState::default());
        app.insert_resource(DeltaSeconds::default());
        app.add_systems(Update, update_player_and_camera);

        // ---- Content: a first grow-able crop, Liebig's-law limited (see
        // crop.rs's own doc comment) and feeding the shared carbon budget
        // (carbon.rs) once each plant reaches maturity. Real Meshy-
        // generated corn model (assets/models/corn_plant/) -- crop::setup
        // starts its async load and registers both the spawn-once-loaded
        // and per-frame growth systems, same pattern as viewmodel::setup.
        app.insert_resource(CarbonBudget::default());

        // ---- A farm pond coupled to the same CarbonBudget -- must be set
        // up before crop::setup below, since update_crop_growth reads
        // Res<water::WaterBody> every frame. See water.rs's own doc comment
        // for the real ocean-acidification chemistry and irrigation-quality
        // research this closes a genuine feedback loop with.
        water::setup(&mut app);

        crop::setup(&mut app);

        // ---- Content: a second crop, switchgrass -- real cellulosic/
        // advanced-generation biofuel feedstock, planted immediately beside
        // corn's own field so the first-gen vs. advanced-gen contrast is a
        // first-sight visual comparison. See switchgrass.rs's own doc
        // comment for the cited real-research calibration behind every one
        // of its constants (growth pace, sequestration, harvest emission).
        switchgrass::setup(&mut app);

        // ---- Content: a third crop, miscanthus -- a second cellulosic
        // feedstock alongside switchgrass, completing a real three-tier
        // biofuel comparison (corn net-emitting, switchgrass net-emitting
        // but far less, miscanthus genuinely net-negative). See
        // miscanthus.rs's own doc comment for the cited real-research
        // calibration behind every one of its constants.
        miscanthus::setup(&mut app);

        // solar::setup must run before hydrogen::setup: it inserts the
        // SolarArray resource update_electrolyzer reads every frame. See
        // solar.rs's own doc comment for why solar alone only partially
        // (not fully) offsets the electrolyzer's own emissions.
        solar::setup(&mut app);

        // ---- A second, distinct energy pathway alongside the crops'
        // biofuel one: a hydrogen electrolyzer, coupled to the same
        // CarbonBudget. See hydrogen.rs's own doc comment for the cited
        // real research this is grounded in -- deliberately surfaces that
        // an electrolyzer isn't automatically clean, the same lesson
        // crop.rs's/fuel.rs's own corn-ethanol design already teaches.
        hydrogen::setup(&mut app);

        // Harvest -> fermentation -> combustion: the first real EMITTING
        // counterpart to crop.rs's own sequestration-on-maturity payout --
        // see fuel.rs's own doc comment for the real chemistry, the
        // harvest-moment VFX flourish, and the design rationale both are
        // grounded in.
        fuel::setup(&mut app);

        // The corn model was refined with real PBR materials (enable_pbr:
        // true), unlike level.rs's deliberately-unlit boxes -- without an
        // actual light source it would render near-black, since a PBR
        // material with zero incident light produces ~zero output the way
        // an unlit material's flat base_color doesn't. A single directional
        // "sun" light is also thematically the right first light for a
        // farm, and a natural future feed for CropGrowth's own `light`
        // input (time-of-day/sun-angle instead of a fixed 1.0/0.35).
        app.world_mut().spawn((
            bevy::light::DirectionalLight {
                illuminance: 6000.0,
                ..Default::default()
            },
            Transform::from_xyz(0.0, 10.0, 0.0).looking_at(Vec3::new(0.3, 0.0, 0.5), Vec3::Y),
        ));

        // Tonemapping::default() is TonyMcMapface, which silently binds a
        // placeholder LUT (wrong-looking output, not a crash) unless the
        // tonemapping_luts feature is enabled -- explicit override instead
        // of adding that feature: zero extra Cargo weight, and
        // KhronosPbrNeutral is designed to preserve color fidelity, a
        // better fit for verifying imported-model color later than a
        // stylized filmic LUT anyway.
        app.world_mut().spawn((
            Camera3d::default(),
            Camera {
                clear_color: ClearColorConfig::Custom(CLEAR_COLOR),
                ..Default::default()
            },
            Tonemapping::KhronosPbrNeutral,
            Transform::from_xyz(level::PLAYER_SPAWN.x, level::PLAYER_SPAWN.y + 0.85, level::PLAYER_SPAWN.z),
            WorldCamera,
        ));

        // ---- Phase 4: viewmodel hands (own fixed camera, order=1, drawn
        // on top of the world camera above without clearing it) ----
        let viewmodel_camera_entity = viewmodel::setup(&mut app);

        // hud.rs's UI is targeted at the VIEWMODEL camera (order=1, the
        // later of the two Camera3d entities), not the world camera --
        // real, screenshot-verified finding: with the world camera as the
        // UI target, the HUD's data pipeline was fully correct (confirmed
        // via multiple render-world probes: UiCameraView attached, non-empty
        // ExtractedUiNodes, a queued TransparentUi phase item, a compiled
        // pipeline, an intact UiViewTarget->ViewTarget chain, zero render
        // errors) yet nothing ever appeared on screen -- and disabling the
        // viewmodel camera entirely (a one-off differential test, not kept)
        // made the SAME HUD code render correctly with the world camera
        // still as the target. That isolates the cause to the two-camera
        // interaction specifically: ViewTarget's `main_texture` ping-pong
        // index is documented as "shared across view targets with the same
        // render target" (bevy_render-0.19.1's own view/mod.rs), so the
        // leading theory is the viewmodel camera's own later Core3d pass
        // (postprocess/tonemapping included) flips that shared index after
        // the world camera's own ui_pass already wrote to it, stranding the
        // UI draw on a ping-pong slot nothing ever presents. Targeting
        // whichever camera renders LAST sidesteps this rather than fixing
        // the root cause upstream -- see biofuel-climate-science-gameplay.md
        // for the full trail if a real fix (vs. this workaround) is wanted.
        hud::setup(&mut app, viewmodel_camera_entity);

        self.app = Some(app);
        self.winit_window = Some(wrapped);
        self.last_frame = Some(Instant::now());
    }

    fn window_event(&mut self, event_loop: &ActiveEventLoop, _id: WindowId, event: WindowEvent) {
        match event {
            WindowEvent::CloseRequested => event_loop.exit(),
            WindowEvent::KeyboardInput { event, .. } => {
                if let PhysicalKey::Code(code) = event.physical_key {
                    if let Some(app) = &mut self.app {
                        app.world_mut()
                            .resource_mut::<InputState>()
                            .set_key(code, event.state == ElementState::Pressed);
                    }
                }
            }
            // Drives fuel.rs's own click-to-harvest -- confirmed via a
            // temporary probe (see biofuel-climate-science-gameplay.md)
            // that unlike Windows-MCP's synthetic keyboard input (which
            // never reaches WindowEvent::KeyboardInput above), real mouse
            // clicks DO reach this Rust/winit World session's own
            // WindowEvent::MouseInput, not just the separate C++/raylib
            // menu screen's own input path.
            WindowEvent::MouseInput { state, button: MouseButton::Left, .. } => {
                if state == ElementState::Pressed {
                    if let Some(app) = &mut self.app {
                        app.world_mut().resource_mut::<InputState>().set_left_click();
                    }
                }
            }
            WindowEvent::RedrawRequested => {
                if let Some(app) = &mut self.app {
                    let now = Instant::now();
                    let dt = self.last_frame.map_or(0.0, |t| now.duration_since(t).as_secs_f32());
                    self.last_frame = Some(now);
                    app.world_mut().resource_mut::<DeltaSeconds>().0 = dt;
                    app.update();
                }
                if let Some(w) = &self.winit_window {
                    w.request_redraw();
                }
            }
            _ => {}
        }
    }

    fn device_event(&mut self, _event_loop: &ActiveEventLoop, _device_id: DeviceId, event: DeviceEvent) {
        if let DeviceEvent::MouseMotion { delta } = event {
            if let Some(app) = &mut self.app {
                app.world_mut()
                    .resource_mut::<InputState>()
                    .accumulate_mouse_delta(delta.0 as f32, delta.1 as f32);
            }
        }
    }

    fn about_to_wait(&mut self, _event_loop: &ActiveEventLoop) {
        if let Some(w) = &self.winit_window {
            w.request_redraw();
        }
    }
}

pub(crate) fn run(_save_slot: i32) -> SessionExitReason {
    if !adapter_probe::vulkan_adapter_available() {
        return SessionExitReason::VulkanUnavailable;
    }

    let mut app = SessionApp::default();

    let ran = event_loop_cell::with_event_loop(|event_loop| event_loop.run_app_on_demand(&mut app));

    match ran {
        Ok(Ok(())) if app.failed => SessionExitReason::InternalError,
        Ok(Ok(())) => SessionExitReason::ReturnedToMenu,
        Ok(Err(_)) => SessionExitReason::InternalError,
        Err(()) => SessionExitReason::VulkanUnavailable,
    }
}

// NOTE on why there is no #[test] here for the EventLoop-reentrancy
// behavior itself: winit's EventLoop::new() requires being called from the
// process's actual main thread (a real Windows/CRT-level check -- see
// event_loop_cell.rs's doc comment) -- but `cargo test`'s default harness
// runs each test on a worker thread, not `main`, so a test exercising
// with_event_loop()/run_app_on_demand() here would hit winit's own
// off-main-thread panic unconditionally, regardless of whether the
// reentrancy logic itself is correct. That's a `cargo test`-harness
// artifact, not a production concern -- the real call path (C++ calling
// run_world_session() from the same thread main() runs on) satisfies this
// requirement by construction. This exact reentrancy pattern was already
// validated end-to-end on a real main thread multiple times across Phase
// 0/1(a)/1(b)/2 -- re-run the manual checks (tests/engine/
// WorldBridgeManualCheck.cpp, WorldRaylibHandoffCheck.cpp) after this
// change to confirm the same holds with real scene content in the loop.
#[cfg(test)]
mod tests {
    use super::adapter_probe;

    // adapter_probe has no main-thread requirement (it never touches
    // winit's EventLoop), so this one is safely `cargo test`-able.
    #[test]
    fn adapter_probe_does_not_panic() {
        let _ = adapter_probe::vulkan_adapter_available();
    }
}
