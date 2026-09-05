#pragma once

#include <atomic>
#include <cstddef>
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
    std::size_t worker_threads = 0; // 0 = hardware concurrency
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

    // Hands over the most recently finished tree, once.
    [[nodiscard]] std::optional<world::svo::BrickTree> take_finished();

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

    SvoWorldOptions options_;
    world::generation::HeightmapGenerator heightmap_;

    mutable std::mutex mutex_;
    std::optional<world::svo::BrickTree> finished_;
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
