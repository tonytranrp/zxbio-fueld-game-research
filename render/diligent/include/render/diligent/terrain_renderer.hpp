#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "render/diligent/memory_tracking.hpp"
#include "render/diligent/render_context.hpp"
#include "render/interface/camera.hpp"
#include "world/chunk/chunk_coord.hpp"
#include "world/meshing/mesh_data.hpp"

namespace render::diligent {

// Sky color every frame clears to -- shared with the frame-verification readback so "non-clear
// pixels" means the same thing in both places. Linear-space RGBA (the sRGB swap chain encodes on
// write).
inline constexpr std::array<float, 4> kClearColor{0.25f, 0.5f, 0.8f, 1.0f};

// Uploads world/meshing MeshData into per-chunk GPU vertex/index buffers and draws the visible
// set each frame: one terrain PSO (hand-written HLSL pair under render/diligent/shaders/),
// CPU-side frustum culling per chunk, single-threaded immediate-context submission
// (Phase 1 brief §2.2, §2.6, and §2.4's measured caution against deferred contexts). Same
// PIMPL compile-firewall as RenderContext: consumers never see a DiligentCore type.
class TerrainRenderer {
public:
    // Throws std::runtime_error when shader compilation or PSO creation fails. `context` must
    // outlive this renderer.
    explicit TerrainRenderer(RenderContext& context);
    ~TerrainRenderer();

    TerrainRenderer(TerrainRenderer&&) noexcept;
    TerrainRenderer& operator=(TerrainRenderer&&) noexcept;
    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;

    // Creates and uploads this chunk's vertex/index buffers, replacing any previous upload for
    // the same coordinate (the old buffers are freed and the allocation tracker decremented
    // first). An empty mesh just removes the entry -- all-air and fully-buried chunks cost no GPU
    // memory and no draw call.
    void upload_chunk_mesh(world::chunk::ChunkCoord coord, const world::meshing::MeshData& mesh);

    // Frees the chunk's GPU buffers and decrements the allocation tracker -- the teardown path
    // chunk streaming's unload (Group D, task 26) will call; no-op if the coordinate has no
    // upload.
    void remove_chunk_mesh(world::chunk::ChunkCoord coord);

    // Clears the current back buffer + depth, then draws every uploaded chunk whose AABB
    // intersects the camera frustum. Does not present -- the caller owns frame pacing.
    void render(const render::interface::Camera& camera);

    [[nodiscard]] std::size_t chunk_count() const noexcept;
    [[nodiscard]] std::size_t last_visible_count() const noexcept; // culling stat from the most recent render()
    [[nodiscard]] const GpuAllocationTracker& gpu_memory() const noexcept;

    // Opaque outside this module (complete only in detail/terrain_renderer_impl.hpp) -- public so
    // the module's own free functions (pipeline creation) can take Impl&.
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace render::diligent
