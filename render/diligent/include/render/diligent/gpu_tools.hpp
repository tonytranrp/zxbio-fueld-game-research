#pragma once

#include <cstdint>

#include "render/diligent/render_context.hpp"

namespace render::diligent {

// VK_EXT_memory_budget snapshot (task 30): the machine-wide answer ("how much VRAM exists and how
// much is in use, other processes included") that complements the self-tracked
// GpuAllocationTracker ("how much are our own chunk buffers using") -- two numbers, two questions.
struct GpuMemoryBudget {
    bool available = false;                      // false: not Vulkan, extension absent, or the query failed
    std::uint64_t device_local_budget_bytes = 0; // sum over DEVICE_LOCAL heaps
    std::uint64_t device_local_usage_bytes = 0;
};

// Creates the Tracy Vulkan GPU-zone context (task 28) against the live device/queue/command
// buffer via the IRenderDeviceVk/ICommandQueueVk QueryInterface chain. No-op (safely) when the
// backend isn't Vulkan, Tracy is compiled out, or vulkan-1.dll isn't loadable. Idempotent.
void attach_gpu_profiler(RenderContext& context);

// Destroys the Tracy context; called by ~RenderContext, safe to call when never attached.
void detach_gpu_profiler(RenderContext& context) noexcept;

// Polls VK_EXT_memory_budget. Cheap, but the extension's own documented usage pattern is periodic
// polling -- call on a timer (every couple of seconds), not per frame.
[[nodiscard]] GpuMemoryBudget query_gpu_memory_budget(RenderContext& context);

} // namespace render::diligent
