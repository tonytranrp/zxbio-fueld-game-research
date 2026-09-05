#pragma once

#include <cstddef>
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
        // Staged upload budget per frame. A whole tree is 200-400 MB at the shipping default;
        // one synchronous CreateBuffer of that size was the 45-61 ms worst frame in every run.
        std::size_t upload_bytes_per_frame = std::size_t{32} * 1024 * 1024;
    };

    // Throws std::runtime_error on shader/PSO failure. `context` must outlive the renderer.
    explicit SvoRenderer(RenderContext& context);
    ~SvoRenderer();

    SvoRenderer(const SvoRenderer&) = delete;
    SvoRenderer& operator=(const SvoRenderer&) = delete;

    // Staged replacement of the GPU tree: begin_upload sizes new buffers and takes ownership of the
    // CPU tree; pump_upload (call once per frame) copies up to Settings::upload_bytes_per_frame and
    // returns true on the frame the whole tree has landed and been swapped in -- the previous tree
    // keeps rendering until then, and its buffers are released only once the GPU is done with them
    // (Diligent defers the destruction). An empty tree renders sky only.
    void begin_upload(world::svo::BrickTree tree);
    bool pump_upload();
    [[nodiscard]] bool upload_pending() const noexcept;
    [[nodiscard]] double last_upload_ms() const noexcept;            // wall-clock from begin to swap
    [[nodiscard]] std::uint32_t last_upload_frames() const noexcept; // frames the staging took

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
