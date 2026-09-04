#include <stdexcept>

#include "detail/render_context_impl.hpp"
#include "engine/core/log.hpp"
#include "render/diligent/gpu_tools.hpp"

#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#if VOXEL_HAS_D3D12
#include "Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#endif

namespace render::diligent {

namespace {

using Diligent::IDeviceContext;
using Diligent::SwapChainDesc;

// One immediate context, zero deferred: Phase 1 brief §2.4's measured caution against
// speculative multithreaded submission -- deferred contexts wait for a frame-time number proving
// single-threaded submission is the bottleneck.
constexpr std::uint32_t kNumContexts = 1;

} // namespace

const char* to_string(Backend backend) noexcept {
    switch (backend) {
    case Backend::Vulkan: return "Vulkan";
    case Backend::D3D12: return "D3D12";
    }
    return "unknown";
}

RenderContext::RenderContext(const RenderContextCreateInfo& info) : impl_(std::make_unique<Impl>()) {
    using namespace Diligent;

    if (info.native_window_handle == nullptr) {
        throw std::runtime_error("RenderContext: native_window_handle is null");
    }

    impl_->backend = info.backend;

    SwapChainDesc scDesc; // defaults: RGBA8_UNORM_SRGB color, D32_FLOAT depth -- PSO reads the
                          // real formats back from the created swap chain, never these defaults
    const Win32NativeWindow window{info.native_window_handle};
    IDeviceContext* contexts[kNumContexts] = {};

    switch (info.backend) {
    case Backend::Vulkan: {
        IEngineFactoryVk* factory = GetEngineFactoryVk();
        EngineVkCreateInfo engineCI;
        if (info.enable_validation) {
            engineCI.SetValidationLevel(VALIDATION_LEVEL_1);
        }
        // Request VK_EXT_memory_budget for the overlay's machine-wide VRAM numbers (task 30);
        // fall back to a plain device if this driver doesn't have it rather than failing startup
        // over a diagnostics feature.
        constexpr const char* kMemoryBudgetExt = "VK_EXT_memory_budget";
        engineCI.DeviceExtensionCount = 1;
        engineCI.ppDeviceExtensionNames = &kMemoryBudgetExt;
        factory->CreateDeviceAndContextsVk(engineCI, &impl_->device, contexts);
        impl_->memoryBudgetExtensionEnabled = impl_->device != nullptr;
        if (!impl_->device) {
            engine::core::log(engine::core::LogLevel::Warn,
                              "Vulkan device creation with VK_EXT_memory_budget failed -- retrying without it");
            engineCI.DeviceExtensionCount = 0;
            engineCI.ppDeviceExtensionNames = nullptr;
            factory->CreateDeviceAndContextsVk(engineCI, &impl_->device, contexts);
        }
        if (!impl_->device) {
            throw std::runtime_error("Vulkan device creation failed -- no compatible GPU/driver enumerated");
        }
        impl_->context.Attach(contexts[0]);
        factory->CreateSwapChainVk(impl_->device, impl_->context, scDesc, window, &impl_->swapchain);
        impl_->factory = factory;
        break;
    }
    case Backend::D3D12: {
#if VOXEL_HAS_D3D12
        IEngineFactoryD3D12* factory = GetEngineFactoryD3D12();
        if (!factory->LoadD3D12()) {
            throw std::runtime_error("d3d12.dll could not be loaded -- D3D12 runtime unavailable");
        }
        EngineD3D12CreateInfo engineCI;
        if (info.enable_validation) {
            engineCI.SetValidationLevel(VALIDATION_LEVEL_1);
        }
        factory->CreateDeviceAndContextsD3D12(engineCI, &impl_->device, contexts);
        if (!impl_->device) {
            throw std::runtime_error("D3D12 device creation failed -- no compatible GPU enumerated");
        }
        impl_->context.Attach(contexts[0]);
        factory->CreateSwapChainD3D12(impl_->device, impl_->context, scDesc, FullScreenModeDesc{}, window,
                                      &impl_->swapchain);
        impl_->factory = factory;
        break;
#else
        throw std::runtime_error("this build has no D3D12 backend compiled in");
#endif
    }
    }

    if (!impl_->swapchain) {
        throw std::runtime_error("swap chain creation failed");
    }

    // The runtime device-enumeration proof Phase 1 brief §0 asked for -- "the build succeeded"
    // is not "a device actually initializes on this machine."
    const GraphicsAdapterInfo& adapter = impl_->device->GetAdapterInfo();
    engine::core::log(engine::core::LogLevel::Info, "render device ready: {} on \"{}\" ({}x{})",
                      to_string(impl_->backend), adapter.Description, impl_->swapchain->GetDesc().Width,
                      impl_->swapchain->GetDesc().Height);
}

RenderContext::~RenderContext() {
    if (impl_) {
        detach_gpu_profiler(*this); // Tracy's Vulkan context must die before the VkDevice does
    }
}
RenderContext::RenderContext(RenderContext&&) noexcept = default;
RenderContext& RenderContext::operator=(RenderContext&&) noexcept = default;

void RenderContext::resize(std::uint32_t width, std::uint32_t height) {
    impl_->swapchain->Resize(width, height);
}

void RenderContext::present() {
    impl_->swapchain->Present(1); // vsync on -- correctness over speed is M1.4's own done-when
}

Backend RenderContext::backend() const noexcept {
    return impl_->backend;
}

std::uint32_t RenderContext::width() const noexcept {
    return impl_->swapchain->GetDesc().Width;
}

std::uint32_t RenderContext::height() const noexcept {
    return impl_->swapchain->GetDesc().Height;
}

std::string RenderContext::adapter_description() const {
    return impl_->device->GetAdapterInfo().Description;
}

} // namespace render::diligent
