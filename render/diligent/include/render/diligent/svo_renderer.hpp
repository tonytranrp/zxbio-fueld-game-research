#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "render/diligent/memory_tracking.hpp"
#include "render/diligent/render_context.hpp"
#include "render/interface/camera.hpp"
#include "world/svo/brick_tree.hpp"

namespace render::diligent {

// Diagnostic outputs of the march pass (docs/goals.md goal 165): each replaces the shaded color
// with one term of the shading so a wrong frame can be attributed to one cause by looking, the
// way tools/svo_render's probes attribute a wrong CPU frame. Mirrors svo_march.psh.hlsl's kView*.
enum class SvoDebugView : std::uint32_t {
    None = 0,
    Lit = 1,          // sun-shadow term (white = lit)
    AO = 2,           // ambient-occlusion term
    Normal = 3,       // the blended shading normal
    FaceNormal = 4,   // the hit cube's axis-aligned face
    Level = 5,        // node level of the hit (hue ramp)
    Steps = 6,        // primary traversal iterations / 256
    Coverage = 7,     // the smoothing node's volume coverage
    CubePixels = 8,   // projected size of the hit cube in pixels / 8
    SmoothNormal = 9, // the tree's averaged normal alone (magenta = none recorded)
    LodCube = 10,     // red = LOD early-out cube, blue = real voxel / solid leaf
    Material = 11,    // the hit material's palette color, unlit
    Distance = 12,    // hit distance in 2 m bands
};

// The micro-voxel renderer (docs/goals.md Group X): uploads a world::svo::BrickTree's two flat
// arrays into two StructuredBuffer<uint>s and draws one fullscreen pass whose pixel shader
// (shaders/svo_march.psh.hlsl) marches a primary ray per pixel through them, writing SV_Depth so
// the post chain and the overlay compose exactly as they do over the mesh renderer's output. No
// vertex data, no per-chunk draw calls: cost scales with pixels, which is the entire reason this
// path exists (research/micro-voxel-pivot-log.md §1). Group Z adds a temporal anti-aliasing
// resolve (shaders/svo_taa.psh.hlsl): the march jitters its rays by a sub-pixel Halton offset and
// writes hit distances; the resolve reprojects the previous frame's result through the camera's
// motion (the world is static), rejects it on distance mismatch, clamps it to the current 3x3
// neighborhood and blends -- eight frames of supersampling for the sub-pixel voxel structure
// that no single sample per pixel can resolve. Same PIMPL firewall as TerrainRenderer.
class SvoRenderer {
public:
    struct Settings {
        bool shadows = true;        // traced sun-shadow ray per hit
        bool ao = true;             // 4 short hemisphere rays per hit
        bool lod_march = true;      // Laine-Karras early-out when a node projects under a pixel
        bool sky = true;            // analytic sky on miss (false: flat clear color, --no-sky)
        bool grain = true;          // per-cube brightness hash, faded toward pixel size (goal 167)
        bool taa = true;            // temporal anti-aliasing resolve (goal 168)
        float lod_quality = 1.0f;   // 1 = stop at one pixel; <1 finer, >1 coarser
        float shadow_lod = 4.0f;    // shadow rays tolerate this much coarser LOD (from their origin)
        float ao_lod = 8.0f;        // AO rays likewise
        float ao_radius_px = 32.0f; // AO ray length as a screen-space radius at the hit's distance
        float smooth_pixels = 6.0f; // the averaged normal comes from the ancestor spanning ~this many pixels
        float grain_amplitude = 0.10f; // +-10% brightness per cube at full size
        float taa_blend = 0.125f;      // weight of the new frame (1/8 = eight-frame history)
        SvoDebugView debug_view = SvoDebugView::None;
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

    // Clears the current target + depth and draws the march pass (and the temporal resolve when
    // enabled). Does not present.
    void render(const render::interface::Camera& camera);

    // GPU time of the last completed march pass in milliseconds (timestamp query; 0 until the
    // first result lands, or when the backend has no timestamp support). Goal 170's "is the lag
    // the GPU or the CPU" number.
    [[nodiscard]] double last_gpu_ms() const noexcept;

    [[nodiscard]] const GpuAllocationTracker& gpu_memory() const noexcept;
    [[nodiscard]] bool has_tree() const noexcept;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace render::diligent
