#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "render/diligent/memory_tracking.hpp"
#include "render/diligent/render_context.hpp"
#include "world/wind/wind_field.hpp"
#include "render/interface/camera.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/cell_grid.hpp"

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
        // Prompt 005 goal 278: band-limit the albedo toward the hit node's area-weighted
        // average as its cube approaches pixel size. Goal 277 measured discrete material
        // sampling as 59% of the moire -- the largest single contributor, and the only one of
        // the six candidates the prompt named that was not measured at zero.
        bool filter_albedo = true;
        bool taa = true;            // temporal anti-aliasing resolve (goal 168)
        // Goal 266: the coarse start-t pre-pass. One conservative cone bound per `beam_tile`
        // square of pixels, computed at 1/beam_tile resolution, from which every primary ray in
        // that tile starts instead of at the root's entry face.
        //
        // OFF BY DEFAULT, because it was measured and it does not pay -- see
        // research/frame-time-and-gpu-architecture-log.md section 13. It works, it is correct (the shipping golden
        // passes with it on at 0.0093% of pixels changed), and it removes 29% of traversal steps
        // and 19% of the march. It also costs 0.69 ms of its own, because 14,400 pixels each
        // chasing a chain of dependent node loads cannot fill this GPU -- so on vk the two cancel
        // to within 0.01 ms and on d3d12 it is 0.72 ms WORSE. Kept behind this knob so a coarser
        // tree, a different GPU, or the reprojected variant that could overlap it with the march
        // can be re-measured by changing one number.
        int beam_tile = 0;
        // Goal 261: the marcher stamps every cell it steps into with the frame index, and sets a
        // request bit on the ones that were not resident. ON by default -- its cost is measured in
        // research section 22 -- and switchable so that measurement has a zero to compare against.
        bool mark_cell_usage = true;
        float lod_quality = 1.0f;   // 1 = stop at one pixel; <1 finer, >1 coarser
        float shadow_lod = 4.0f;    // shadow rays tolerate this much coarser LOD (from their origin)
        float ao_lod = 8.0f;        // AO rays likewise
        float ao_radius_px = 32.0f; // AO ray length as a screen-space radius at the hit's distance
        float smooth_pixels = 6.0f; // the averaged normal comes from the ancestor spanning ~this many pixels
        float grain_amplitude = 0.10f; // +-10% brightness per cube at full size
        float taa_blend = 0.125f;      // weight of the new frame (1/8 = eight-frame history)
        SvoDebugView debug_view = SvoDebugView::None;
        // The ONE wind field (world/wind, Prompt 001 Group B). Everything that moves reads it;
        // --no-wind sets still_wind(), which zeroes the field itself rather than making each
        // consumer test a flag.
        world::wind::WindParams wind;
        // Staged upload budget per frame. A whole tree is 200-400 MB at the shipping default;
        // one synchronous CreateBuffer of that size was the 45-61 ms worst frame in every run.
        std::size_t upload_bytes_per_frame = std::size_t{32} * 1024 * 1024;
    };

    // The animation clock the shaders are given (water phase, wind time). Exposed so the overlay
    // can sample the SAME wind the marcher is drawing, rather than a second clock that agrees only
    // approximately -- the whole point of Group B is one field, read consistently.
    [[nodiscard]] float anim_seconds() const noexcept;

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
    // Takes a SHARED handle, not the object (Prompt 003 goal 227). The simulation queries the same
    // immutable tree the marcher is drawing -- which is what makes "you cannot pass through
    // anything the renderer draws" true by construction rather than by a tolerance. Two owners of
    // one const object is what shared_ptr is for; two owners of a mutable one would be a bug, and
    // BrickTree has been immutable-after-construction since the pivot.
    void begin_upload(std::shared_ptr<const world::svo::BrickTree> tree);

    // Prompt 004 goal 256: the same staged replacement for a grid of cells. The slicing machinery
    // is shared -- a grid is still two word arrays -- and the per-cell records ride along as one
    // small extra buffer (4,096 cells is 64 KB, so it is uploaded whole rather than sliced).
    void begin_upload(std::shared_ptr<const world::svo::FlatCellGrid> grid);

    // Prompt 004 goal 262: the marcher's per-cell usage words, read back from the GPU.
    //
    // Fenced and pipelined over three frames -- the copy for frame N is read on frame N+3, so the
    // CPU never waits on the GPU. Returns false until a slot's copy has actually completed, which is
    // the difference between a readback and a stall.
    //
    // The size is the whole point and it is why there is no GPU-side compaction here: one uint per
    // CELL, so 4,096 cells is 16 KB per frame. GigaVoxels compacts because it has millions of
    // elements to report; this grid has thousands, and compacting 16 KB to reduce a 16 KB transfer
    // would be machinery with no subject. See research section 23 for the measured cost.
    // Prompt 004 goals 263-265: the streaming path. Cells arrive one at a time and only the bricks
    // that actually changed are sent, against the staged whole-tree transfer this replaces.
    //
    // BEHIND A FLAG AND OFF BY DEFAULT, deliberately. The single-tree and whole-grid paths are
    // measured and shipping; this one changes the upload architecture, and a new architecture that
    // silently replaces a working one is how a late change becomes a regression nobody can bisect.
    void begin_stream(world::svo::CellGrid shape, std::size_t brick_slots,
                      std::shared_ptr<const world::svo::BrickTree> proxy);
    /// Install one built cell. False when the pool is full -- the caller evicts and retries.
    bool install_cell(std::size_t index, const world::svo::BrickTree& tree);
    void evict_cell(std::size_t index);
    /// Repack the node array and send whatever is dirty, bounded by `upload_bytes_per_frame`.
    /// Returns the bytes actually sent this frame.
    std::uint64_t flush_cells();
    [[nodiscard]] bool streaming() const noexcept;
    [[nodiscard]] std::size_t resident_cells() const noexcept;
    [[nodiscard]] std::uint64_t stream_bytes_total() const noexcept;

    [[nodiscard]] bool read_cell_usage(std::vector<std::uint32_t>& out);
    /// Bytes the last readback moved, and how long the copy itself cost on the GPU.
    [[nodiscard]] std::uint64_t last_usage_readback_bytes() const noexcept;
    [[nodiscard]] double last_usage_readback_ms() const noexcept;
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
