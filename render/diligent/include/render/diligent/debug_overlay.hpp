#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "render/diligent/gpu_tools.hpp"
#include "render/diligent/render_context.hpp"

struct GLFWwindow; // input backend handle only -- no GLFW header crosses this boundary

namespace render::diligent {

// What the overlay panel shows (task 31): the caller fills this each frame from whatever systems
// own the numbers (frame clock, streaming system, TerrainRenderer's tracker) and hands the
// budget in from its own timer-driven query_gpu_memory_budget() poll -- the overlay renders
// state, it doesn't own or gather any.
struct OverlayStats {
    float fps = 0.0f;
    float frame_ms = 0.0f;
    std::size_t ready_chunks = 0;
    std::size_t visible_chunks = 0;
    std::size_t total_chunk_meshes = 0;
    std::size_t objects = 0; // decoration objects (trees) in ready chunks
    // Goal 82: per-silhouette breakdown (sums to `objects`).
    std::size_t objects_round = 0;
    std::size_t objects_conifer = 0;
    std::size_t objects_shrub = 0;
    // Goal 84: crosshair-aim readout ("Grass @ 12,34,56"); empty = no hit / not computed.
    char aim_line[64] = {};
    std::uint64_t gpu_self_bytes = 0; // §2.3 number 1: our own chunk buffers (GpuAllocationTracker)
    std::uint64_t gpu_self_peak_bytes = 0;
    GpuMemoryBudget budget; // §2.3 number 2: VK_EXT_memory_budget, machine-wide
};

// Dear ImGui debug overlay: Diligent's vendored ImGui renderer (DiligentTools) + the vendored
// GLFW platform backend -- no new dependency (Phase 1 completion brief §2.5). Chain-installs
// its GLFW callbacks, so construct it AFTER any engine/input callback registration and it will
// forward events on.
class DebugOverlay {
public:
    // Throws std::runtime_error if ImGui initialization fails. `context` and `window` must
    // outlive the overlay.
    DebugOverlay(RenderContext& context, GLFWwindow* window);
    ~DebugOverlay();

    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;

    // Builds and draws the panel into the currently bound render target -- call after the terrain
    // pass, before Present.
    void render(const OverlayStats& stats);

    // Goal 130 (Voxel Representation Redesign SS5): the one-time world-load progress screen, using
    // this same ImGui overlay infrastructure rather than a new UI system -- a real, moving bar fed
    // by WorldLoader's own completion count, not a static "Loading..." string.
    void render_loading(std::size_t chunksReady, std::size_t chunksTotal);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace render::diligent
