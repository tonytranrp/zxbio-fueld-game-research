#pragma once

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/core/math.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/jobs/thread_pool.hpp"
#include "render/diligent/terrain_renderer.hpp"
#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_store.hpp"
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
// unload/discard. Voxel data itself never lives in the registry (M1_2_BRIEF.md §5).
class ChunkStreamingSystem {
public:
    ChunkStreamingSystem(world::streaming::StreamingConfig config, int seed, std::size_t workerThreads,
                         render::diligent::TerrainRenderer& renderer, engine::ecs::Registry& registry);

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
    [[nodiscard]] std::size_t in_flight_count() const noexcept {
        return pending_mesh_.size() + gen_in_flight_.size() + mesh_in_flight_.size();
    }
    [[nodiscard]] std::size_t worker_thread_count() const noexcept { return pool_.thread_count(); }
    [[nodiscard]] std::size_t pending_mesh_count() const noexcept { return pending_mesh_.size(); }
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
        bool failed = false;
    };

    void request_generation(world::chunk::ChunkCoord coord);
    void drain_generation_completions();
    void submit_ready_mesh_jobs();
    void drain_mesh_completions();
    void apply_unloads(const std::vector<world::chunk::ChunkCoord>& coords);
    void sweep_generation_margin(world::chunk::ChunkCoord anchor);
    void destroy_chunk_entity(world::chunk::ChunkCoord coord);
    [[nodiscard]] bool neighborhood_generated(world::chunk::ChunkCoord coord) const;

    world::streaming::ChunkStreamer streamer_;
    world::chunk::ChunkStore store_;
    world::generation::HeightmapGenerator heightmap_; // concurrent generate calls are safe + deterministic (stress-tested)
    render::diligent::TerrainRenderer& renderer_;
    engine::ecs::Registry& registry_;

    std::unordered_map<world::chunk::ChunkCoord, engine::ecs::Entity> chunk_entities_;
    std::unordered_set<world::chunk::ChunkCoord> pending_mesh_;   // streamer-requested, waiting on neighbors
    std::unordered_set<world::chunk::ChunkCoord> generated_;      // voxels present in store_
    std::unordered_set<world::chunk::ChunkCoord> gen_in_flight_;  // generation job running
    std::unordered_set<world::chunk::ChunkCoord> mesh_in_flight_; // mesh job running

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
