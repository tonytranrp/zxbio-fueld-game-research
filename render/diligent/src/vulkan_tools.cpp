#include "render/diligent/gpu_tools.hpp"

#include "detail/render_context_impl.hpp"
#include "engine/core/log.hpp"

#if PLATFORM_WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <vulkan/vulkan.h>

#include "Graphics/GraphicsEngineVulkan/interface/CommandQueueVk.h"
#include "Graphics/GraphicsEngineVulkan/interface/DeviceContextVk.h"
#include "Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h"

#if defined(TRACY_ENABLE)
// Symbol-table mode: Tracy resolves every vk* call it needs through the two proc-addr loaders we
// hand it, so nothing here links against vulkan-1.lib (only the runtime DLL, loaded below) --
// same reason DiligentCore itself uses volk instead of the import library.
#include <tracy/TracyVulkan.hpp>
#endif

namespace render::diligent {

namespace {

using namespace Diligent;

// vulkan-1.dll is the loader every Vulkan driver installs; if it's absent, the Vulkan backend
// couldn't have initialized in the first place, so failing soft here is purely defensive.
PFN_vkGetInstanceProcAddr load_instance_proc_addr() {
    static PFN_vkGetInstanceProcAddr addr = [] {
        const HMODULE loader = LoadLibraryA("vulkan-1.dll");
        return loader != nullptr
                   ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"))
                   : nullptr;
    }();
    return addr;
}

struct VkHandles {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    RefCntAutoPtr<IRenderDeviceVk> deviceVk;
};

bool query_vk_handles(RenderContext::Impl& rc, VkHandles& out) {
    if (rc.backend != Backend::Vulkan || !rc.device) {
        return false;
    }
    rc.device->QueryInterface(IID_RenderDeviceVk,
                              reinterpret_cast<IObject**>(static_cast<IRenderDeviceVk**>(&out.deviceVk)));
    if (!out.deviceVk) {
        return false;
    }
    out.instance = out.deviceVk->GetVkInstance();
    out.physicalDevice = out.deviceVk->GetVkPhysicalDevice();
    out.device = out.deviceVk->GetVkDevice();
    return out.instance != VK_NULL_HANDLE && out.physicalDevice != VK_NULL_HANDLE && out.device != VK_NULL_HANDLE;
}

} // namespace

void attach_gpu_profiler(RenderContext& context) {
#if defined(TRACY_ENABLE)
    auto& rc = context.impl();
    if (rc.tracyVkCtx != nullptr) {
        return; // idempotent
    }
    VkHandles handles;
    if (!query_vk_handles(rc, handles)) {
        return;
    }
    const PFN_vkGetInstanceProcAddr instanceProcAddr = load_instance_proc_addr();
    if (instanceProcAddr == nullptr) {
        return;
    }
    const auto deviceProcAddr =
        reinterpret_cast<PFN_vkGetDeviceProcAddr>(instanceProcAddr(handles.instance, "vkGetDeviceProcAddr"));
    if (deviceProcAddr == nullptr) {
        return;
    }

    // Per research/diligent-core-api-surface.md Task 4: the queue pointer is stable for the
    // context's lifetime (lock only guards concurrent submission), and the command buffer must be
    // fetched fresh at every use -- this one is only used for Tracy's initial calibration queries,
    // which Diligent submits with the next flush/present.
    ICommandQueue* queueBase = rc.context->LockCommandQueue();
    RefCntAutoPtr<ICommandQueueVk> queueVk;
    if (queueBase != nullptr) {
        queueBase->QueryInterface(IID_CommandQueueVk,
                                  reinterpret_cast<IObject**>(static_cast<ICommandQueueVk**>(&queueVk)));
    }
    const VkQueue queue = queueVk ? queueVk->GetVkQueue() : VK_NULL_HANDLE;
    rc.context->UnlockCommandQueue();
    if (queue == VK_NULL_HANDLE) {
        return;
    }

    RefCntAutoPtr<IDeviceContextVk> contextVk;
    rc.context->QueryInterface(IID_DeviceContextVk,
                               reinterpret_cast<IObject**>(static_cast<IDeviceContextVk**>(&contextVk)));
    if (!contextVk) {
        return;
    }
    VkCommandBuffer cmdBuffer = contextVk->GetVkCommandBuffer();

    tracy::VkCtx* tracyCtx = TracyVkContext(handles.instance, handles.physicalDevice, handles.device, queue,
                                            cmdBuffer, instanceProcAddr, deviceProcAddr);
    if (tracyCtx != nullptr) {
        constexpr const char kName[] = "Terrain GPU";
        TracyVkContextName(tracyCtx, kName, sizeof(kName) - 1);
        rc.tracyVkCtx = tracyCtx;
        engine::core::log(engine::core::LogLevel::Info, "Tracy Vulkan GPU-zone context attached");
    }
#else
    (void)context;
#endif
}

void detach_gpu_profiler(RenderContext& context) noexcept {
#if defined(TRACY_ENABLE)
    auto& rc = context.impl();
    if (rc.tracyVkCtx != nullptr) {
        TracyVkDestroy(static_cast<tracy::VkCtx*>(rc.tracyVkCtx));
        rc.tracyVkCtx = nullptr;
    }
#else
    (void)context;
#endif
}

GpuMemoryBudget query_gpu_memory_budget(RenderContext& context) {
    GpuMemoryBudget budget;
    auto& rc = context.impl();
    if (!rc.memoryBudgetExtensionEnabled) {
        return budget; // extension not enabled at device creation -> querying the struct is invalid
    }
    VkHandles handles;
    if (!query_vk_handles(rc, handles)) {
        return budget;
    }
    const PFN_vkGetInstanceProcAddr instanceProcAddr = load_instance_proc_addr();
    if (instanceProcAddr == nullptr) {
        return budget;
    }
    // Core-1.1 entry point (the extension's stated dependency floor); Diligent requires 1.1+ for
    // its Vulkan backend, so no KHR-suffixed fallback is needed.
    const auto getMemProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(
        instanceProcAddr(handles.instance, "vkGetPhysicalDeviceMemoryProperties2"));
    if (getMemProps2 == nullptr) {
        return budget;
    }

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps{};
    budgetProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    VkPhysicalDeviceMemoryProperties2 memProps{};
    memProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    memProps.pNext = &budgetProps;
    getMemProps2(handles.physicalDevice, &memProps);

    for (std::uint32_t i = 0; i < memProps.memoryProperties.memoryHeapCount; ++i) {
        if ((memProps.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
            budget.device_local_budget_bytes += budgetProps.heapBudget[i];
            budget.device_local_usage_bytes += budgetProps.heapUsage[i];
        }
    }
    budget.available = true;
    return budget;
}

} // namespace render::diligent

#else // !PLATFORM_WIN32

namespace render::diligent {
void attach_gpu_profiler(RenderContext&) {}
void detach_gpu_profiler(RenderContext&) noexcept {}
GpuMemoryBudget query_gpu_memory_budget(RenderContext&) { return {}; }
} // namespace render::diligent

#endif
