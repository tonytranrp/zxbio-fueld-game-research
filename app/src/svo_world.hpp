#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "engine/core/math.hpp"
#include "engine/jobs/thread_pool.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/generation/field/terrain_field.hpp"
#include "world/svo/terrain_sampler.hpp"
#include "world/svo/cell_grid.hpp"
#include "world/svo/lod_bands.hpp"
#include "world/svo/tree_builder.hpp"

#include <vector>

namespace app {

struct SvoWorldOptions {
    int seed = 1337;
    int voxel_size_log2 = -7; // 7.8 mm: sub-centimeter, the pivot's whole point
    // Prompt 007 goal 329. 4096 m: 2048 m of view in every direction, EIGHT TIMES the 256 m the
    // 512 m default gave. V = root - voxel = 12 + 7 = 19, five bits under tree_layout.hpp's
    // kMaxVoxelBits = 24, so no cascaded root is needed -- **V was never the binding constraint in
    // this range; cost was.**
    //
    // Measured, and the shape of it is the finding: growing the REGION is nearly free because the
    // added volume is all coarse levels, while growing the LOD RADIUS (goal 328) is superlinear.
    //
    //   region   view    V   bricks    MB     build    GPU ms   fps
    //    512 m   256 m  16   219,344   68.9   0.97 s    2.69    165
    //   1024 m   512 m  17   316,179   98.4   1.37 s    3.21    165
    //   2048 m  1024 m  18   392,109  121.6   1.66 s    3.33    165
    //   4096 m  2048 m  19   445,038  138.0   2.12 s    4.09    165   <- shipped
    //   8192 m  4096 m  20   498,334  154.6   2.79 s    5.55    107
    //
    // **256x the area for 2.27x the bricks.** 4096 m is where the vsync cap still holds; 8192 m is
    // where it breaks, and that is the whole reason the default is not larger.
    //
    // The research's near/mid criterion (aesthetics §9.5/§9.9) is 1.83 km at a pixel footprint or
    // 3.44 km at 1 arcmin; 2048 m of view sits between them.
    int root_size_log2 = 12;
    // Prompt 007 goal 328. THE KNOB IS AN ANGULAR SIZE, and always was: the LOD rule
    // `target(d) = max(finest, d * finest / lod_radius)` makes target(d)/d constant beyond the
    // radius, so `lod_radius` is the denominator of a fixed angular voxel size expressed in the
    // least legible possible units.
    //
    // `lod_quality_arcmin` states it directly. **0 means "use lod_radius"**, which is what keeps
    // --lod-radius a faithful alias rather than an approximation of one: at 0 nothing is
    // recomputed and the tree is byte-identical to the pre-change build (asserted in
    // test_lod_quality.cpp).
    //
    // The shipped default is 6.71 arcmin, which is exactly what lod_radius = 4.0 has always meant
    // at a 7.8 mm finest voxel -- stated rather than changed, because changing it is a cost
    // decision and goal 328 measures that separately.
    float lod_radius = 4.0f;  // full resolution within this distance, halving per doubling beyond
    float lod_quality_arcmin = 0.0f; // 0 = derive from lod_radius

    /// The radius the builder should actually use: the angular quality when one was asked for,
    /// otherwise `lod_radius` untouched. One function so the four call sites cannot diverge.
    [[nodiscard]] float effective_lod_radius() const noexcept {
        if (lod_quality_arcmin <= 0.0f) {
            return lod_radius;
        }
        const float finest = std::exp2(static_cast<float>(voxel_size_log2));
        return world::svo::BuildParams::radius_for_quality(finest, lod_quality_arcmin);
    }
    // Prompt 004 goals 254-256: build the region as a GRID of cells this size instead of one tree.
    // 0 keeps the single tree. 5 is 32 m, the size goal 254's arithmetic settled on: 12 levels per
    // cell against 16 for a 512 m region, which measured 21.7 -> 10.5 octree steps per ray on the
    // CPU reference. A cell is an ordinary BrickTree, so nothing about the encoding changes.
    int cell_size_log2 = 0;
    // Prompt 004 goal 264: stream cells one at a time instead of building the whole grid before
    // showing anything. Requires cell_size_log2 > 0.
    bool stream_cells = false;
    // Cells handed to the renderer per frame. Bounded because the failure mode research §1.9(a)
    // warns about is starvation of the GPU by an unbounded producer -- GigaVoxels DP reports a 2x
    // gain purely from fixing that -- and because goal 170 measured 12 of 13 slow frames as
    // `present` stalls with a build running.
    int cells_per_frame = 8;
    // The coarse always-resident proxy's voxel size, as a log2 in metres. 0 = 1 m voxels over the
    // whole region: at a 512 m region that is a 9-level tree, cheap to build once and small enough
    // to keep resident forever.
    int proxy_voxel_log2 = 0;
    // Brick slots in the renderer's fixed pool. Kept at 1.2 M across goal 258's palette change, so
    // the SLOT COUNT is a constant and the change shows up honestly as bytes: 1.2 M x 576 B was
    // ~690 MB, 1.2 M x 280 B is ~336 MB. Still comfortably above the 902,616 bricks the shipping
    // 512 m world measures, and now with 354 MB more of the 7,180 MiB budget left over.
    int brick_slots = 1200000;
    // Prompt 006 Group AM-A: bake the macro terrain field at load and generate the world from
    // it, rather than from four octaves of analytic noise. OFF by default until the pipeline's
    // physics stages exist (goals 300+) -- goal 295's option (b) is rejected precisely because a
    // world whose shape depends on what has been baked is a determinism hazard, and shipping a
    // half-built pipeline as the default would be the same mistake from the other direction.
    /// ON by default as of goal 321. It was opt-in while Group AM-B was being built, which meant
    /// every rendered frame during this pass showed the OLD four-octave noise terrain -- caught by
    /// taking a capture after the detail retune and seeing near-vertical spires where the metrics
    /// said 4.8 degrees. A pipeline the shipped binary does not run is not shipped.
    bool macro_field = true;
    // Stop the pipeline after N stages; -1 runs all of them. How one stage's contribution is
    // isolated for a capture or an acceptance statistic.
    int field_stages = -1;
    bool trees = true;
    // Prompt 007 goal 336: metres within which trees are voxelized from their grown skeleton rather
    // than the implicit box+octahedron. 0 = every tree implicit, which is what the tools and the
    // equivalence test use.
    float skeleton_radius = 128.0f;
    // Prompt 007 goal 338: metres of voxel ground cover. Far smaller than the skeleton radius --
    // a 0.28 m blade is only representable inside the finest LOD ring.
    float grass_radius = 24.0f;
    // How many real plants one voxel tuft stands for. Lower is denser and more expensive.
    float grass_plants_per_tuft = 20.0f;
    std::size_t worker_threads = 0; // 0 = three quarters of the hardware threads (goal 170)

    // ---- when to rebuild (Prompt 004 goals 249, 250) ---------------------------------------------
    //
    // The trigger used to be `distance_from_build_center > lod_radius * 0.5f`, which ties how OFTEN
    // the world is rebuilt to a DETAIL parameter. That is a coincidence, not a design, and at the
    // 4 m default it asked for a full 400 MB rebuild every 2 metres -- against a 40 m/s fly speed
    // and a ~2 s build, so builds ran back to back forever while you moved. Measured: p99 13.16 ms
    // with rebuilds, 6.89 without (goal 247).
    //
    // THIS IS A STOPGAP. It reduces how often the wrong thing happens; AK-C/D replace the whole
    // "rebuild the world" model with per-cell streaming. Labelled as one deliberately.
    //
    // 8 m, and BOTH halves of that were measured rather than chosen.
    //
    // (a) Frame time is flat in this parameter. Ramping it over 4 / 8 / 16 / 24 m on fly_transect
    //     gives p99 9.32 / 8.19 / 8.06 / 8.28 ms -- inside the run-to-run spread. The reason is
    //     `rebuild_max_speed` below: during a fast flight NO rebuild triggers at any distance, so
    //     the distance only decides behaviour while the camera is moving SLOWLY, which is exactly
    //     when a rebuild is cheap to absorb. The AK-B win is the speed gate's, not this number's.
    // (b) Image quality is NOT flat. Rendering the same pose with the LOD centre displaced by
    //     4 / 8 / 16 / 24 m (tools/svo_render --lod-center, the deterministic reproduction) gives a
    //     staleness of 0.03 / 0.27 / 5.11 / 13.70 mean levels, over 0.2% / 2.9% / 36.7% / 45.3% of
    //     pixels. It is free up to 8 m and falls off a cliff between 8 and 16.
    //
    // So 8 m keeps essentially all of the frame-time win and 94% of the image quality that 24 m
    // threw away. A first draft of this shipped 24 and the near field was visibly blocky
    // (research/captures/ak_lod_staleness.png); the ramp is what caught it.
    float rebuild_trigger_metres = 8.0f;
    // Hysteresis: having triggered, do not trigger again until the camera has settled within this
    // of the NEW centre. Without it a camera drifting along the trigger radius re-triggers every
    // frame it crosses back and forth.
    float rebuild_settle_metres = 3.0f;
    // A build cannot start until this long after the previous one's upload finished. The upload is
    // ~13-21 frames of UpdateBuffer traffic; starting the next build inside that window stacks a
    // CPU-side resample on top of it, which is two of goal 247's five slow frames.
    float rebuild_min_interval_seconds = 1.5f;
    // Goal 250: a rebuild centred where the camera WAS is worthless by the time it lands. DEFER
    // while moving fast rather than predicting the centre from velocity -- prediction needs a
    // reliable estimate of build duration to extrapolate by, and that duration varies 1.9-3.6 s
    // with terrain complexity (measured), so a prediction would be wrong by tens of metres exactly
    // when it matters most. Deferring is honest: it says "not while you are moving", and the world
    // is no more stale than the prediction would have made it.
    float rebuild_max_speed = 12.0f;
};

// The micro-voxel world (docs/goals.md Group X): owns the generator and builds world::svo
// BrickTrees around the camera on a background thread, one at a time -- the app asks for a new one
// whenever the camera has moved far enough from the last build center for the finest LOD ring to
// have drifted (research/micro-voxel-pivot-log.md §2.6: whole-tree rebuild first, measured, with
// incremental subtree reuse the named follow-up). Replaces WorldLoader entirely on this path:
// there are no chunks, no meshes, nothing per-frame except "is a new tree ready to upload".
class SvoWorld {
public:
    explicit SvoWorld(const SvoWorldOptions& options);
    ~SvoWorld();

    SvoWorld(const SvoWorld&) = delete;
    SvoWorld& operator=(const SvoWorld&) = delete;

    // Starts a background build centered on `camera`. Returns false (and does nothing) while a
    // build is already running.
    bool request_build(glm::vec3 camera);

    // Goals 249/250: the whole trigger policy in one pure-ish place, so the frame loop asks a
    // question instead of embedding a rule. `speed` is the camera's current m/s and `now` the
    // app clock. Returns true when a build should be requested this frame.
    [[nodiscard]] bool should_rebuild(glm::vec3 camera, float speed, double now) const noexcept;
    // Called by the app when a finished tree is adopted, so the minimum-interval clock starts from
    // the end of the upload rather than the start of the build.
    void note_adopted(double now, glm::vec3 cameraAtAdopt) noexcept;
    // Diagnostics for goal 250's check: how far the camera had moved from the build centre by the
    // time the tree was adopted.
    [[nodiscard]] float last_adopt_lag_metres() const noexcept { return lastAdoptLag_; }

    // Hands over the most recently finished tree, once, as a SHARED handle: the renderer's staged
    // upload and the simulation's collision query are two owners of one immutable object
    // (Prompt 003 goal 227). BrickTree has been immutable-after-construction since the pivot, so
    // this is a shared_ptr and nothing else -- no copy of 400 MB, no synchronisation.
    [[nodiscard]] std::shared_ptr<const world::svo::BrickTree> take_finished();

    // Prompt 004 goal 256/257: the same handoff for the cell grid. Exactly one of these two is
    // ever non-null for a given build -- `Options::cell_size_log2` decides which, and 0 means the
    // single-tree path this engine shipped with.
    [[nodiscard]] std::shared_ptr<const world::svo::FlatCellGrid> take_finished_grid();

    // Goal 264: the streaming producer. `start_stream` fixes the grid shape and builds the coarse
    // proxy; `take_built_cells` hands over whatever finished since the last call, nearest-first.
    struct BuiltCell {
        std::size_t index = 0;
        std::shared_ptr<const world::svo::BrickTree> tree; // null = genuinely empty, still resident
    };
    [[nodiscard]] world::svo::CellGrid stream_shape(glm::vec3 camera) const;
    [[nodiscard]] std::shared_ptr<const world::svo::BrickTree> build_proxy(const world::svo::CellGrid& shape);
    void start_stream(const world::svo::CellGrid& shape, glm::vec3 camera);
    /// Goal 264: submit at most `max` queued cell builds. Called once per frame.
    ///
    /// start_stream PLANS the order and pump_stream SUBMITS, and the split is the fix for a
    /// measured failure rather than a style choice: submitting all 4,096 jobs from the frame loop
    /// put 1,710 ms on one frame and left the pool saturated for seconds afterwards, which is
    /// exactly the "synchronization and starvation of GPU cores" research 1.9(a) warns about.
    void pump_stream(std::size_t max);
    [[nodiscard]] std::vector<BuiltCell> take_built_cells(std::size_t max);
    [[nodiscard]] std::size_t stream_pending() const;
    // True while a finished tree is waiting to be taken (diagnostics: the frame that takes it
    // pays for the GPU buffer creation).
    [[nodiscard]] bool take_finished_pending() const {
        const std::lock_guard guard(mutex_);
        return finished_ != nullptr;
    }

    [[nodiscard]] bool building() const noexcept { return building_.load(); }
    [[nodiscard]] float distance_from_build_center(glm::vec3 camera) const noexcept;
    [[nodiscard]] bool has_requested() const noexcept { return requested_; }

    // The region a build centered on `camera` would cover: XZ-centered (snapped to 8 m so
    // rebuilds keep voxel alignment), Y over [8 - half, 8 + half) -- this terrain spans [-64, 64] m
    // plus ~15 m of trees, so 128 m+ roots keep every hilltop and tree.
    [[nodiscard]] world::svo::TreeGeometry geometry_for(glm::vec3 camera) const noexcept;

    // Walk mode's analytic ground query -- the same height function the tree is sampled from.
    [[nodiscard]] float ground_height(float worldX, float worldZ) const {
        return heightmap_.height_at(worldX, worldZ);
    }
    [[nodiscard]] const world::generation::HeightmapGenerator& heightmap() const noexcept {
        return heightmap_;
    }
    [[nodiscard]] const SvoWorldOptions& options() const noexcept { return options_; }

    struct LastBuild {
        world::svo::BuildStats stats;
        world::svo::BrickTree::Stats tree;
    std::size_t cells = 0;         // goal 256: present cells, 0 on the single-tree path
    std::size_t cells_rebuilt = 0; // goal 257: of those, the ones this build actually rebuilt
    std::size_t cells_reused = 0;  // and the ones carried over from the previous grid
        std::size_t bricks = 0;
        std::size_t memory_bytes = 0;
        std::size_t trees = 0;
        double sampler_seconds = 0.0;
        // Goal 336's accounting, carried out of the build thread with everything else.
        std::size_t skeleton_trees = 0;
        std::size_t skeleton_primitives = 0;
        std::size_t skeleton_bytes = 0;
        double skeleton_seconds = 0.0;
        bool valid = false;
    };
    [[nodiscard]] LastBuild last_build() const;

private:
    void build_job(glm::vec3 camera);

    // Trigger state (goals 249/250). `settled_` is the hysteresis latch: set when a build is
    // requested, cleared once the camera comes back inside `rebuild_settle_metres` of the centre.
    mutable bool awaitingSettle_ = false;
    double lastAdoptSeconds_ = -1.0e9;
    float lastAdoptLag_ = 0.0f;
    glm::vec3 lastAdoptCamera_{0.0f};

    SvoWorldOptions options_;
    world::generation::HeightmapGenerator heightmap_;

    mutable std::mutex mutex_;
    std::shared_ptr<const world::svo::BrickTree> finished_;
    std::shared_ptr<const world::svo::FlatCellGrid> finishedGrid_;
    // Goal 257: what the LAST grid build produced, so the next one can reuse the cells whose band
    // did not change. Touched only on the build thread between builds (one at a time, enforced by
    // `building_`), so it needs no lock of its own.
    // Goal 264's producer state. The queue is drained on the main thread and filled by the pool,
    // so it takes the same mutex the finished-tree handoff does rather than inventing a second
    // synchronisation scheme (skill rule 33-35: use the pattern that is already here).
    std::vector<BuiltCell> streamReady_;
    std::vector<char> streamRequested_;
    std::vector<std::size_t> streamQueue_; // planned order, nearest first; drained by pump_stream
    std::size_t streamQueueNext_ = 0;
    world::svo::TerrainSampler::FocusTiers streamTiers_;
    glm::vec3 streamCamera_{0.0f};
    world::svo::CellGrid streamShape_;
    std::atomic<std::size_t> streamInFlight_{0};
    std::vector<std::shared_ptr<const world::svo::BrickTree>> lastCells_;
    std::vector<int> lastBands_;
    glm::ivec3 lastOriginCell_{0};
    bool haveLastGrid_ = false;

    void build_grid_job(const world::svo::TreeGeometry& g,
                        const world::svo::TerrainSamplerParams& sp, const world::svo::BuildParams& bp,
                        const world::svo::TerrainSampler& seeded, double samplerSeconds);
    LastBuild lastBuild_;
    std::atomic<bool> building_{false};
    bool requested_ = false;
    glm::vec3 buildCenter_{0.0f};

    // Declaration order is teardown order in reverse: the worker thread (which submits into
    // pool_ and waits on it) is destroyed -- joined -- BEFORE the pool it uses.
    engine::jobs::ThreadPool pool_;
    std::jthread worker_;
};

} // namespace app
