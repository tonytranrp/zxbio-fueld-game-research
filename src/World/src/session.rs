//! One gameplay session: opens a window, boots a real `bevy_app::App`
//! running `bevy_render`'s own renderer (pinned to the Vulkan backend) to
//! clear it to a color every frame, and returns once the player closes the
//! window (or a clean failure occurs).
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
#![forbid(unsafe_code)]

use std::sync::Arc;

use bevy::app::App;
// bevy::MinimalPlugins, not bevy::app::MinimalPlugins -- it lives at the
// crate root (bevy_internal's `pub use bevy_internal::*;`), same lesson
// already learned once in Engine/game's own bevy usage this session.
use bevy::MinimalPlugins;
use bevy::asset::AssetPlugin;
use bevy::camera::{Camera, Camera2d, ClearColor, ClearColorConfig};
use bevy::color::Color;
use bevy::core_pipeline::tonemapping::Tonemapping;
use bevy::core_pipeline::CorePipelinePlugin;
use bevy::image::ImagePlugin;
use bevy::mesh::MeshPlugin;
use bevy::render::settings::{Backends, WgpuSettings};
use bevy::render::RenderPlugin;
use bevy::tasks::tick_global_task_pools_on_main_thread;
use bevy::app::PluginsState;
use bevy::window::{
    PrimaryWindow, RawHandleWrapper, Window as BevyWindow, WindowPlugin, WindowResolution, WindowWrapper,
};

use winit::application::ApplicationHandler;
use winit::event::WindowEvent;
use winit::event_loop::ActiveEventLoop;
use winit::platform::run_on_demand::EventLoopExtRunOnDemand;
use winit::window::{Window as WinitWindow, WindowId};

use crate::{adapter_probe, event_loop_cell};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum SessionExitReason {
    ReturnedToMenu,
    VulkanUnavailable,
    InternalError,
}

// Background clear color for this milestone -- an arbitrary dark navy,
// swapped for real scene content in a later phase.
const CLEAR_COLOR: Color = Color::srgb(0.04, 0.05, 0.08);

#[derive(Default)]
struct SessionApp {
    app: Option<App>,
    // Kept alive for the session's duration -- RawHandleWrapper only holds
    // a type-erased Arc clone of the same underlying window, not a typed
    // handle back to it, and this is also what receives request_redraw().
    winit_window: Option<Arc<WindowWrapper<WinitWindow>>>,
    failed: bool,
}

impl SessionApp {
    fn fail(&mut self, event_loop: &ActiveEventLoop) {
        self.failed = true;
        event_loop.exit();
    }
}

impl ApplicationHandler for SessionApp {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        let attrs = WinitWindow::default_attributes().with_title("Fuel Farm");
        let winit_window = match event_loop.create_window(attrs) {
            Ok(w) => w,
            Err(_) => return self.fail(event_loop),
        };
        let size = winit_window.inner_size();
        let wrapped = Arc::new(WindowWrapper::new(winit_window));

        let raw_handle = match RawHandleWrapper::new(&wrapped) {
            Ok(h) => h,
            Err(_) => return self.fail(event_loop),
        };

        let mut app = App::new();
        // bevy_render::view::prepare_view_targets reads this global
        // resource unconditionally (even though this milestone's one
        // camera overrides it per-camera via ClearColorConfig::Custom
        // below) -- confirmed missing by a real panic ("Resource does not
        // exist" for Res<ClearColor>).
        app.insert_resource(ClearColor(CLEAR_COLOR));
        // MinimalPlugins first: AssetPlugin's async loading needs
        // IoTaskPool, which only exists once TaskPoolPlugin (part of this
        // group) has run -- confirmed by a real panic without it
        // ("The IoTaskPool has not been initialized yet"). Safe to include
        // even though this group also sets a ScheduleRunnerPlugin runner:
        // this code never calls App::run() (only .update(), driven by our
        // own winit callbacks below), so whatever runner is set is unused.
        app.add_plugins(MinimalPlugins);
        // Registers the Window-related message/resource types bevy_render's
        // own systems (e.g. WindowRenderPlugin's extract_windows, which
        // reads a WindowClosing message reader) expect to exist -- this is
        // bevy_window's plugin, unrelated to bevy_winit's WinitPlugin
        // (still correctly not used here); confirmed missing by a real
        // panic ("Message not initialized") without it. primary_window:
        // None so it doesn't spawn its own competing PrimaryWindow entity
        // -- this code spawns its own below, wired to the real winit
        // window instead of one WindowPlugin would create itself.
        app.add_plugins(WindowPlugin {
            primary_window: None,
            ..Default::default()
        });
        // Must precede RenderPlugin: RenderPlugin::build() itself calls
        // app.init_asset::<Shader>(), which needs AssetPlugin's resources
        // already registered -- confirmed by a real panic without this
        // ("Requested resource ... does not exist in the World").
        app.add_plugins(AssetPlugin::default());
        // TonemappingPlugin (added transitively by CorePipelinePlugin
        // below) unconditionally touches an Assets<Image> resource even
        // with tonemapping_luts disabled -- confirmed by a real panic
        // without this ("Requested resource ... does not exist").
        app.add_plugins(ImagePlugin::default());
        // Core3d's render pipeline (added via CorePipelinePlugin below)
        // unconditionally reads an AssetEvent<Mesh> message reader even
        // with zero Mesh entities spawned -- confirmed by a real panic
        // ("Message not initialized" for MeshReader<AssetEvent<Mesh>>)
        // without this. Registers the Mesh asset type, nothing more (no
        // actual mesh data needed for this milestone).
        app.add_plugins(MeshPlugin);
        app.add_plugins(RenderPlugin {
            render_creation: WgpuSettings {
                backends: Some(Backends::VULKAN),
                ..Default::default()
            }
            .into(),
            ..Default::default()
        });
        app.add_plugins(CorePipelinePlugin);

        let bevy_window = BevyWindow {
            resolution: WindowResolution::new(size.width, size.height),
            title: "Fuel Farm".to_string(),
            ..Default::default()
        };
        app.world_mut().spawn((bevy_window, raw_handle, PrimaryWindow));

        // Tonemapping::default() is TonyMcMapface, which silently binds a
        // placeholder LUT (wrong-looking output, not a crash) unless the
        // tonemapping_luts feature is enabled -- explicit override instead
        // of adding that feature (see the migration plan's tonemapping
        // section): zero extra Cargo weight, and KhronosPbrNeutral is
        // designed to preserve color fidelity, a better fit for verifying
        // imported-model color later than a stylized filmic LUT anyway.
        app.world_mut().spawn((
            Camera2d,
            Camera {
                clear_color: ClearColorConfig::Custom(CLEAR_COLOR),
                ..Default::default()
            },
            Tonemapping::KhronosPbrNeutral,
        ));

        // Canonical bootstrap sequence bevy_app's own default runner uses
        // before its first update() -- RenderPlugin creates its Device/
        // Queue asynchronously (see FutureRenderResources) and only
        // actually unpacks them into the app (inserting resources like
        // DeviceErrorHandler) inside Plugin::finish(), which nothing calls
        // automatically when App::run() itself is never invoked (this code
        // only ever calls .update()). Confirmed necessary by a real panic
        // without it ("Requested resource ... DeviceErrorHandler does not
        // exist"). plugins_state() also drives Plugin::ready() to
        // completion, which is what the tick_global_task_pools_on_main_thread
        // loop is for -- without ticking, the async future backing
        // FutureRenderResources never gets polled to readiness.
        while app.plugins_state() == PluginsState::Adding {
            tick_global_task_pools_on_main_thread();
        }
        app.finish();
        app.cleanup();

        self.app = Some(app);
        self.winit_window = Some(wrapped);
    }

    fn window_event(&mut self, event_loop: &ActiveEventLoop, _id: WindowId, event: WindowEvent) {
        match event {
            WindowEvent::CloseRequested => event_loop.exit(),
            WindowEvent::RedrawRequested => {
                if let Some(app) = &mut self.app {
                    app.update();
                }
                if let Some(w) = &self.winit_window {
                    w.request_redraw();
                }
            }
            _ => {}
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
// validated end-to-end on a real main thread twice: the standalone Phase-0
// spike (3/3 sequential sessions, plain wgpu) and the Phase-1(a) manual
// C++-driven check (tests/engine/WorldBridgeManualCheck.cpp, 2/2 sessions
// through the real FFI path) -- re-run that manual check after this change
// to confirm the same holds with real bevy_render in the loop too.
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
