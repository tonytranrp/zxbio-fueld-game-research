//! One gameplay session: opens a window with a Vulkan-backed wgpu device,
//! clears it to a color every frame, and returns once the player closes the
//! window (or a clean failure occurs). Phase 1(a) scope -- see lib.rs's
//! module doc for why this is plain wgpu rather than real bevy_render yet.
#![forbid(unsafe_code)]

use std::sync::Arc;

use winit::application::ApplicationHandler;
use winit::event::WindowEvent;
use winit::event_loop::ActiveEventLoop;
use winit::platform::run_on_demand::EventLoopExtRunOnDemand;
use winit::window::{Window, WindowId};

use crate::{adapter_probe, event_loop_cell};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum SessionExitReason {
    ReturnedToMenu,
    VulkanUnavailable,
    InternalError,
}

struct GpuState {
    surface: wgpu::Surface<'static>,
    device: wgpu::Device,
    queue: wgpu::Queue,
}

#[derive(Default)]
struct SessionApp {
    window: Option<Arc<Window>>,
    gpu: Option<GpuState>,
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
        let attrs = Window::default_attributes().with_title("Fuel Farm");
        let window = match event_loop.create_window(attrs) {
            Ok(w) => Arc::new(w),
            Err(_) => return self.fail(event_loop),
        };

        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            backends: wgpu::Backends::VULKAN,
            ..wgpu::InstanceDescriptor::new_without_display_handle()
        });

        let Ok(surface) = instance.create_surface(window.clone()) else {
            return self.fail(event_loop);
        };

        let adapter_result = pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
            power_preference: wgpu::PowerPreference::HighPerformance,
            compatible_surface: Some(&surface),
            force_fallback_adapter: false,
        }));
        let Ok(adapter) = adapter_result else {
            return self.fail(event_loop);
        };

        let device_result = pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor::default()));
        let Ok((device, queue)) = device_result else {
            return self.fail(event_loop);
        };

        let size = window.inner_size();
        let caps = surface.get_capabilities(&adapter);
        let Some(&format) = caps.formats.first() else {
            return self.fail(event_loop);
        };
        let config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format,
            width: size.width.max(1),
            height: size.height.max(1),
            present_mode: wgpu::PresentMode::Fifo,
            alpha_mode: caps.alpha_modes[0],
            view_formats: vec![],
            desired_maximum_frame_latency: 2,
        };
        surface.configure(&device, &config);

        self.gpu = Some(GpuState { surface, device, queue });
        self.window = Some(window);
    }

    fn window_event(&mut self, event_loop: &ActiveEventLoop, _id: WindowId, event: WindowEvent) {
        match event {
            WindowEvent::CloseRequested => event_loop.exit(),
            WindowEvent::RedrawRequested => {
                if let Some(gpu) = &self.gpu {
                    let acquired = match gpu.surface.get_current_texture() {
                        wgpu::CurrentSurfaceTexture::Success(t)
                        | wgpu::CurrentSurfaceTexture::Suboptimal(t) => Some(t),
                        _ => None,
                    };
                    if let Some(frame) = acquired {
                        let view = frame
                            .texture
                            .create_view(&wgpu::TextureViewDescriptor::default());
                        let mut encoder = gpu
                            .device
                            .create_command_encoder(&wgpu::CommandEncoderDescriptor::default());
                        {
                            let _pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                                label: None,
                                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                                    view: &view,
                                    depth_slice: None,
                                    resolve_target: None,
                                    ops: wgpu::Operations {
                                        load: wgpu::LoadOp::Clear(wgpu::Color {
                                            r: 0.04,
                                            g: 0.05,
                                            b: 0.08,
                                            a: 1.0,
                                        }),
                                        store: wgpu::StoreOp::Store,
                                    },
                                })],
                                depth_stencil_attachment: None,
                                timestamp_writes: None,
                                occlusion_query_set: None,
                                multiview_mask: None,
                            });
                        }
                        gpu.queue.submit(Some(encoder.finish()));
                        frame.present();
                    }
                }
                if let Some(w) = &self.window {
                    w.request_redraw();
                }
            }
            _ => {}
        }
    }

    fn about_to_wait(&mut self, _event_loop: &ActiveEventLoop) {
        if let Some(w) = &self.window {
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
// validated end-to-end on a real main thread by the standalone Phase-0
// spike (3/3 sequential sessions, see the migration plan artifact); the
// manual C++-driven check (src/World/tests/manual_two_session_check.cpp)
// is what re-validates it inside this actual crate/FFI path.
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
