#pragma once

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

#include "engine/core/math.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/events/dispatcher.hpp"
#include "engine/jobs/thread_pool.hpp"
#include "render/diligent/terrain_renderer.hpp"
#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/chunk/coord_containers.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/meshing/mesh_data.hpp"
#include "world/streaming/chunk_streamer.hpp"

namespace app {

// App glue running ChunkStreamer's commands through the real pipeline (tasks 23-26):
//
//   streamer tick -> generate (worker) -> [all 27 neighbors generated] -> mesh (worker) -> upload (main)
//
// Threading model, chosen for zero locks on world data: ChunkStore is touched by the main thread
// only. Generation jobs fill a standalone Chunk and hand it back through a mutex-guarded
// completion queue; mesh jobs run world::meshing::extract_mesh against a private 27-chunk
// *snapshot* ChunkStore (paletted chunks are cheap to copy) built on the main thread at submit
// time. In-flight jobs are never cancelled -- completions are checked against
// ChunkStreamer::is_desired() and stale results discarded (§2.2's deliberate v1 choice).
//
// The ECS side (task 25): every streamed chunk coordinate gets an entity carrying
// world::chunk::ChunkPipelineState (Requested -> Generated -> Meshing -> Ready), destroyed on
// unload/discard. Voxel data itself never lives in the registry (M1.2 brief §5).
class ChunkStreamingSystem {
public:
    // uploadBudgetPerTick: max mesh completions committed to the GPU per update() call
    // (TERRAIN_FIXES_BRIEF Group T task 16's stutter fix); 0 = unlimited (the pre-fix behavior,
    // kept selectable for A/B measurement).
    ChunkStreamingSystem(world::streaming::StreamingConfig config, int seed, std::size_t workerThreads,
                         render::diligent::TerrainRenderer& renderer, engine::ecs::Registry& registry,
                         engine::events::Dispatcher& dispatcher, std::size_t uploadBudgetPerTick = 4);

    // Not movable/copyable: worker jobs capture `this` for the completion queues.
    ChunkStreamingSystem(const ChunkStreamingSystem&) = delete;
    ChunkStreamingSystem& operator=(const ChunkStreamingSystem&) = delete;

    // One streaming tick + completion drain. Call at a fixed cadence from the frame loop (it does
    // not need to run every frame; every frame is also fine at these set sizes).
    void update(const glm::vec3& cameraWorldPosition, double nowSeconds);

    // True when no generation/mesh work is running or queued anywhere -- the initial load has
    // converged for the current camera cell. Used by --verify-frame to know when a readback is
    // meaningful.
    [[nodiscard]] bool settled() const;

    [[nodiscard]] std::size_t ready_chunk_count() const noexcept { return streamer_.loaded_count(); }
    [[nodiscard]] std::pair<std::int32_t, std::int32_t> loaded_y_range() const noexcept {
        return streamer_.loaded_y_range();
    }

    // Walk mode's analytic ground query (Group V task 23) -- the same height function that
    // generates the terrain, so camera physics and the rendered surface agree by construction.
    // Clamped to sea level: an underwater column's terrain surface is submerged (the mesher
    // renders the WATER top there, not the stone), so v1 walking strides across the water
    // surface rather than descending under the ocean -- no swimming yet, documented scope.
    [[nodiscard]] float ground_height(float worldX, float worldZ) const {
        return std::max(heightmap_.height_at(worldX, worldZ), 0.0f);
    }
    [[nodiscard]] std::size_t in_flight_count() const noexcept {
        return pending_mesh_.size() + gen_in_flight_.size() + mesh_in_flight_.size();
    }
    [[nodiscard]] std::size_t worker_thread_count() const noexcept { return pool_.thread_count(); }
    [[nodiscard]] std::size_t pending_mesh_count() const noexcept { return pending_mesh_.size(); }
    [[nodiscard]] std::size_t object_count() const noexcept { return total_tree_count_; } // Group W task 35
    [[nodiscard]] std::size_t generation_in_flight_count() const noexcept { return gen_in_flight_.size(); }
    [[nodiscard]] std::size_t mesh_in_flight_count() const noexcept { return mesh_in_flight_.size(); }

private:
    struct GenCompletion {
        world::chunk::Chunk chunk; // carries its own coord; built on the worker's default resource
        bool failed = false;
    };
    struct MeshCompletion {
        world::chunk::ChunkCoord coord;
        world::meshing::MeshData mesh;
        std::size_t tree_count = 0; // decoration objects appended to this chunk's mesh (Group W)
        bool failed = false;
    };

    void request_generation(world::chunk::ChunkCoord coord);
    void drain_generation_completions();
    void submit_ready_mesh_jobs(world::chunk::ChunkCoord anchor);
    void drain_mesh_completions();
    void apply_unloads(const std::vector<world::chunk::ChunkCoord>& coords);
    void release_gpu_meshes_budgeted();
    void sweep_generation_margin(world::chunk::ChunkCoord anchor);
    void destroy_chunk_entity(world::chunk::ChunkCoord coord);
    [[nodiscard]] bool neighborhood_generated(world::chunk::ChunkCoord coord) const;

    world::streaming::ChunkStreamer streamer_;
    std::size_t upload_budget_per_tick_ = 4;
    int seed_ = 0; // tree placement shares the terrain seed (deterministic decoration, Group W)
    world::chunk::CoordMap<std::size_t> tree_counts_; // per ready chunk, for the overlay count
    std::size_t total_tree_count_ = 0;
    world::chunk::ChunkStore store_;
    world::generation::HeightmapGenerator heightmap_; // concurrent generate calls are safe + deterministic (stress-tested)
    render::diligent::TerrainRenderer& renderer_;
    engine::ecs::Registry& registry_;
    // Group L: chunk lifecycle events (world/streaming/chunk_events.hpp) fire through here,
    // always from the main-thread drains -- never from a worker (engine/events threading rule).
    engine::events::Dispatcher& dispatcher_;

    world::chunk::CoordMap<engine::ecs::Entity> chunk_entities_;
    // Unloaded chunks whose GPU buffers are not yet destroyed: buffer destruction is budgeted
    // per tick (TERRAIN_FIXES research: godot_voxel's Tracy-profiled finding that destroying the
    // trailing face of meshes on a boundary crossing is itself a main-thread spike). Main-thread
    // only.
    std::vector<world::chunk::ChunkCoord> gpu_release_queue_;
    world::chunk::CoordSet pending_mesh_;   // streamer-requested, waiting on neighbors
    world::chunk::CoordSet generated_;      // voxels present in store_
    world::chunk::CoordSet gen_in_flight_;  // generation job running
    world::chunk::CoordSet mesh_in_flight_; // mesh job running

    // Workers push, the main thread drains in update(). Guarded by their own mutexes so a worker
    // never contends with anything but the drain itself. mutable: settled() is logically const
    // but must take the locks to inspect the queues.
    mutable std::mutex gen_mutex_;
    std::vector<GenCompletion> gen_completions_;
    mutable std::mutex mesh_mutex_;
    std::vector<MeshCompletion> mesh_completions_;

    // Declaration order is load-bearing, the exact lesson ThreadPool's own members already
    // document: pool_ is declared LAST so it is destroyed FIRST, joining every worker (the pool
    // drains queued jobs on destruction) while the queues/mutexes/heightmap those jobs capture
    // through `this` are all still alive. When an external pool outlived this object instead, a
    // still-running mesh job pushed into a destroyed completion vector -- a real crash (access
    // violation in _Orphan_range on teardown), not a theoretical one.
    engine::jobs::ThreadPool pool_;
};

} // namespace app
