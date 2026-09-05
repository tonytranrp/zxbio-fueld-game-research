#pragma once

#include <cstdint>
#include <memory>

#include "render/diligent/memory_tracking.hpp"
#include "render/diligent/render_context.hpp"
#include "render/interface/camera.hpp"
#include "world/svo/brick_tree.hpp"

namespace render::diligent {

// The micro-voxel renderer (docs/goals.md Group X): uploads a world::svo::BrickTree's two flat
// arrays into two StructuredBuffer<uint>s and draws one fullscreen pass whose pixel shader
// (shaders/svo_march.psh.hlsl) marches a primary ray per pixel through them, writing SV_Depth so
// the post chain and the overlay compose exactly as they do over the mesh renderer's output. No
// vertex data, no per-chunk draw calls: cost scales with pixels, which is the entire reason this
// path exists (research/micro-voxel-pivot-log.md §1). Same PIMPL firewall as TerrainRenderer.
class SvoRenderer {
public:
    struct Settings {
        bool shadows = true;         // traced sun-shadow ray per hit
        bool ao = true;              // 4 short hemisphere rays per hit
        bool lod_march = true;       // Laine-Karras early-out when a node projects under a pixel
        bool sky = true;             // analytic sky on miss (false: flat clear color, --no-sky)
        float lod_quality = 1.0f;    // 1 = stop at one pixel; <1 finer, >1 coarser
        float shadow_lod = 4.0f;     // shadow rays tolerate this much coarser LOD
        float ao_ray_length = 0.75f; // meters
    };

    // Throws std::runtime_error on shader/PSO failure. `context` must outlive the renderer.
    explicit SvoRenderer(RenderContext& context);
    ~SvoRenderer();

    SvoRenderer(const SvoRenderer&) = delete;
    SvoRenderer& operator=(const SvoRenderer&) = delete;

    // Replaces the GPU tree wholesale (new immutable buffers; the previous ones are released once
    // the GPU is done with them -- Diligent defers the destruction). Returns the upload's wall-clock
    // milliseconds. An empty tree renders sky only.
    double upload(const world::svo::BrickTree& tree);

    void set_settings(const Settings& settings) noexcept;
    [[nodiscard]] const Settings& settings() const noexcept;

    // Clears the current target + depth and draws the march pass. Does not present.
    void render(const render::interface::Camera& camera);

    [[nodiscard]] const GpuAllocationTracker& gpu_memory() const noexcept;
    [[nodiscard]] bool has_tree() const noexcept;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace render::diligent
