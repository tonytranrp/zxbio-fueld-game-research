//! Pre-flight check that a Vulkan-capable adapter actually exists, before
//! touching the persisted `EventLoop` at all.
//!
//! Without this, a machine with no Vulkan-capable driver would hit wgpu's
//! internal `.expect(GPU_NOT_FOUND_ERROR_MESSAGE)` deep inside adapter
//! creation (see the migration plan's Vulkan-backend-selection research) --
//! an unhandled panic that this project's `panic = "abort"` policy turns
//! into an instant, silent process kill instead of a clean menu-side error.
//! Probing first lets `run_world_session` return `VulkanUnavailable` across
//! cxx cleanly, before the `EventLoop`/window ever gets involved.
#![forbid(unsafe_code)]

pub(crate) fn vulkan_adapter_available() -> bool {
    let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
        backends: wgpu::Backends::VULKAN,
        ..wgpu::InstanceDescriptor::new_without_display_handle()
    });

    pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
        power_preference: wgpu::PowerPreference::HighPerformance,
        // No surface yet at this point (probing happens before any window
        // exists) -- None is the documented-correct value when you don't
        // have one to check compatibility against.
        compatible_surface: None,
        force_fallback_adapter: false,
    }))
    .is_ok()
}
