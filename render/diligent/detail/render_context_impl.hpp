#pragma once

// Module-internal (render/diligent/src/ only): the one place RenderContext's Diligent members are
// visible. Anything outside this module includes render/diligent/render_context.hpp and sees only
// the opaque Impl forward declaration.

#include "render/diligent/render_context.hpp"

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/EngineFactory.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

namespace render::diligent {

struct RenderContext::Impl {
    Backend backend = Backend::Vulkan;
    // Declaration order is teardown order in reverse: swap chain releases before the context,
    // context before the device, device before the factory that made it.
    Diligent::RefCntAutoPtr<Diligent::IEngineFactory> factory;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain;

    // Group E, Vulkan only. tracyVkCtx is a tracy::VkCtx* stored type-erased so only the TUs that
    // actually include TracyVulkan.hpp (vulkan_tools.cpp, terrain_renderer.cpp) see Tracy/Vulkan
    // types; destroyed via detach_gpu_profiler() from ~RenderContext, before the device dies.
    void* tracyVkCtx = nullptr;
    bool memoryBudgetExtensionEnabled = false; // VK_EXT_memory_budget was requested AND the device came up with it
};

} // namespace render::diligent
