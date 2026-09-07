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
#include "Graphics/GraphicsTools/interface/DurationQueryHelper.hpp"

#include <array>
#include <cstddef>
#include <optional>

#include "render/diligent/gpu_passes.hpp"

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
    bool memoryBudgetExtensionEnabled =
        false; // VK_EXT_memory_budget was requested AND the device came up with it

    // Post-process scene target (goals.md Group D): when PostProcessor is live, this is the
    // offscreen HDR color texture the scene pass renders into (TerrainRenderer checks it each
    // frame and falls back to the swap chain when null). Owned/updated by PostProcessor::Impl;
    // lives here so the renderer needs no public-API change and no second plumbing path.
    Diligent::RefCntAutoPtr<Diligent::ITexture> sceneColor;

    // Per-pass GPU timestamps (goal 220). One DurationQueryHelper per range; see
    // render/diligent/gpu_passes.hpp for why there is no "present" range and why the first two
    // frames are always skipped.
    struct GpuPassTimers {
        static constexpr std::size_t kCount = static_cast<std::size_t>(GpuPass::Count);
        std::array<std::optional<Diligent::DurationQueryHelper>, kCount> timers;
        std::array<double, kCount> lastMs{};
        std::array<bool, kCount> open{};
        std::uint32_t frameCounter = 0;
        bool enabled = true;
        bool supported = false;

        // The two-frame skip: CLAUDE.md records a real crash inside the NVIDIA driver when a
        // timestamp is the app's very first Vulkan command.
        [[nodiscard]] bool active() const noexcept { return enabled && supported && frameCounter >= 2; }
    } gpuPasses;
};

} // namespace render::diligent
