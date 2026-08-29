// Crate-wide #![forbid(unsafe_code)] was tried and reverted here: unlike
// rapier_bridge (whose hand-written logic lives in separate modules that can
// each be #[forbid]'d individually, leaving only the cxx-generated `ffi`
// module exempt), this crate's hand-written code lives at the crate root
// alongside `mod ffi` with no module boundary between them -- there is
// nothing to scope a forbid to without restructuring the file. cxx's own
// `#[cxx::bridge]` macro necessarily expands into `unsafe impl`/`extern "C"`
// glue to cross the ABI (verified: a crate-wide forbid fails to compile here
// with the exact same "implementation of an unsafe trait" errors seen if you
// try it in rapier_bridge before that crate's per-module scoping). This
// crate's own hand-written code (build_headless_app and friends) has zero
// unsafe today, same as rapier_bridge, just not compiler-enforced.

use std::time::Duration;

use bevy::app::PluginsState;
use bevy::asset::{Assets, RenderAssetUsages};
use bevy::camera::{ClearColor, RenderTarget};
use bevy::ecs::prelude::*;
use bevy::image::Image;
use bevy::prelude::{App, Camera3d, Color, DefaultPlugins, Handle, PluginGroup};
use bevy::render::render_resource::{PollType, TextureFormat};
use bevy::render::renderer::RenderDevice;
use bevy::render::view::screenshot::{Screenshot, ScreenshotCaptured};
use bevy::render::RenderPlugin;
use bevy::time::TimeUpdateStrategy;
use bevy::window::{ExitCondition, WindowPlugin};

// #[allow(unused_qualifications)]: cxx's macro expansion of the shared
// struct below trips the workspace's `unused_qualifications` lint on its own
// generated code, not this declaration itself (same false positive as
// rapier_bridge's ffi module).
#[allow(unused_qualifications)]
#[cxx::bridge(namespace = "biofuel::engine::bevy_bridge")]
mod ffi {
    #[derive(Copy, Clone, Debug)]
    struct BridgeInputState {
        move_forward: bool,
        move_back: bool,
        move_left: bool,
        move_right: bool,
        look_dx: f32,
        look_dy: f32,
    }

    extern "Rust" {
        type BevyRenderer;

        fn new_renderer(width: u32, height: u32) -> Box<BevyRenderer>;
        fn step_frame(renderer: &mut BevyRenderer, dt: f32, input: BridgeInputState);
        fn frame_pixels(renderer: &BevyRenderer) -> &[u8];
        fn frame_width(renderer: &BevyRenderer) -> u32;
        fn frame_height(renderer: &BevyRenderer) -> u32;
    }
}

#[derive(Resource)]
struct LatestFrame(Vec<u8>);

#[derive(Resource, Clone)]
struct RenderTargetHandle(Handle<Image>);

// #[allow(missing_debug_implementations)]: bevy::app::SubApps doesn't derive
// or implement Debug, so a derive here isn't an option.
#[allow(missing_debug_implementations)]
pub struct BevyRenderer {
    sub_apps: bevy::app::SubApps,
    width: u32,
    height: u32,
    gpu_lost: bool,
}

fn build_headless_app(width: u32, height: u32) -> bevy::app::SubApps {
    let mut app = App::new();

    // DefaultPlugins is entirely #[cfg(feature = "...")]-gated per plugin
    // (bevy_internal::default_plugins), so with our trimmed, no-bevy_winit
    // feature set it naturally includes exactly the plugins our enabled
    // features provide (AssetPlugin, RenderPlugin, ImagePlugin, MeshPlugin,
    // CameraPlugin, LightPlugin, CorePipelinePlugin, PbrPlugin, TransformPlugin,
    // ...) and naturally OMITS WinitPlugin -- there is no need to hand-assemble
    // this list (and no reliable way to, short of duplicating Bevy's own
    // internal plugin dependency graph one missing-resource panic at a time,
    // which is exactly what a hand-picked list here originally hit).
    app.add_plugins(
        DefaultPlugins
            .set(WindowPlugin {
                primary_window: None,
                exit_condition: ExitCondition::DontExit,
                ..Default::default()
            })
            .set(RenderPlugin {
                synchronous_pipeline_compilation: true,
                ..Default::default()
            }),
    );

    while app.plugins_state() == PluginsState::Adding {
        bevy::tasks::tick_global_task_pools_on_main_thread();
    }
    app.finish();
    app.cleanup();

    let mut target = Image::new_fill(
        bevy::render::render_resource::Extent3d {
            width,
            height,
            depth_or_array_layers: 1,
        },
        bevy::render::render_resource::TextureDimension::D2,
        &[255, 0, 255, 255],
        TextureFormat::Rgba8UnormSrgb,
        RenderAssetUsages::RENDER_WORLD,
    );
    target.texture_descriptor.usage |= bevy::render::render_resource::TextureUsages::COPY_SRC
        | bevy::render::render_resource::TextureUsages::RENDER_ATTACHMENT
        | bevy::render::render_resource::TextureUsages::TEXTURE_BINDING;

    let world = app.world_mut();
    let handle = world.resource_mut::<Assets<Image>>().add(target);
    world.insert_resource(RenderTargetHandle(handle.clone()));
    world.insert_resource(ClearColor(Color::srgb(1.0, 0.0, 1.0)));
    world.insert_resource(LatestFrame(vec![0u8; (width * height * 4) as usize]));

    world.spawn((
        Camera3d::default(),
        RenderTarget::Image(handle.into()),
    ));

    std::mem::take(app.sub_apps_mut())
}

pub fn new_renderer(width: u32, height: u32) -> Box<BevyRenderer> {
    let width = width.max(1);
    let height = height.max(1);
    let sub_apps = build_headless_app(width, height);
    Box::new(BevyRenderer {
        sub_apps,
        width,
        height,
        gpu_lost: false,
    })
}

fn store_latest_frame(trigger: On<ScreenshotCaptured>, mut frame: ResMut<LatestFrame>) {
    if let Some(data) = trigger.event().image.data.clone() {
        frame.0 = data;
    }
}

pub fn step_frame(renderer: &mut BevyRenderer, dt: f32, _input: ffi::BridgeInputState) {
    if renderer.gpu_lost {
        return;
    }

    let world = renderer.sub_apps.main.world_mut();
    world.insert_resource(TimeUpdateStrategy::ManualDuration(Duration::from_secs_f32(
        dt.max(0.0),
    )));

    let target = world.resource::<RenderTargetHandle>().0.clone();
    world
        .spawn(Screenshot::image(target))
        .observe(store_latest_frame);

    renderer.sub_apps.update();

    let poll_result = renderer
        .sub_apps
        .main
        .world()
        .resource::<RenderDevice>()
        .wgpu_device()
        .poll(PollType::Wait {
            submission_index: None,
            timeout: None,
        });

    if poll_result.is_err() {
        renderer.gpu_lost = true;
    }
}

pub fn frame_pixels(renderer: &BevyRenderer) -> &[u8] {
    renderer
        .sub_apps
        .main
        .world()
        .resource::<LatestFrame>()
        .0
        .as_slice()
}

pub fn frame_width(renderer: &BevyRenderer) -> u32 {
    renderer.width
}

pub fn frame_height(renderer: &BevyRenderer) -> u32 {
    renderer.height
}
