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
#include "world/svo/tree_builder.hpp"

namespace app {

struct SvoWorldOptions {
    int seed = 1337;
    int voxel_size_log2 = -7; // 7.8 mm: sub-centimeter, the pivot's whole point
    int root_size_log2 = 9;   // 512 m region around the camera
    float lod_radius = 4.0f;  // full resolution within this distance, halving per doubling beyond
    bool trees = true;
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
        std::size_t bricks = 0;
        std::size_t memory_bytes = 0;
        std::size_t trees = 0;
        double sampler_seconds = 0.0;
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
