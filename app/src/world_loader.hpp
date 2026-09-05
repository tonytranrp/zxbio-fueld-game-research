#pragma once

#include <cstddef>
#include <mutex>
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
#include "world/streaming/world_bounds.hpp"

#include "tree_decoration.hpp"

namespace app {

// Group S (Voxel Representation Redesign SS3.3): replaces the old ChunkStreamingSystem's per-tick
// desired-set/hysteresis machinery with a ONE-TIME parallel generation pass over a fixed, known-
// in-advance chunk set -- there is no "what should load/unload right now" question left to answer
// once the world is static, so there is no per-frame decision logic left to run. The
// generate->mesh->upload pipeline shape and its threading model (ChunkStore touched only by the
// main thread; workers hand results back through mutex-guarded completion queues; snapshot-based
// mesh jobs so a running job never observes a store mutation) are carried over unchanged from the
// old system -- that part was never the problem the redesign is solving.
//
// Lifecycle: construct, call begin() once (enqueues generation for the whole world PLUS its
// 1-chunk meshing halo), then call pump() every loading-screen frame until finished() is true.
// Nothing in this class runs, or needs to run, after that -- a static world has no post-load
// per-frame work at all, which is the whole point.
class WorldLoader {
public:
    WorldLoader(world::streaming::WorldBounds bounds, int seed, std::size_t workerThreads,
                render::diligent::TerrainRenderer& renderer, engine::ecs::Registry& registry,
                engine::events::Dispatcher& dispatcher, glm::vec3 spawnWorldPosition,
                std::size_t uploadBudgetPerTick = 4);

    // Not movable/copyable: worker jobs capture `this` for the completion queues.
    WorldLoader(const WorldLoader&) = delete;
    WorldLoader& operator=(const WorldLoader&) = delete;

    // Enqueues generation for every chunk in the world's bounds plus its 1-chunk-wide meshing halo
    // (goal 128's check: every in-bounds chunk generated exactly once -- verified by construction,
    // chunks_in_bounds() never repeats a coordinate, and request_generation() is itself idempotent
    // per coordinate). Call once, before the first pump().
    void begin();

    // One increment of work: drains finished generation/mesh jobs, submits newly-ready mesh jobs
    // (a chunk becomes ready to mesh the instant all 26 of its neighbors have generated), and
    // uploads a budgeted number of finished meshes to the GPU. Call every loading-screen frame.
    void pump();

    [[nodiscard]] bool finished() const noexcept { return ready_count_ == real_total_; }

    struct Progress {
        std::size_t generated = 0;
        std::size_t ready = 0;
        std::size_t total = 0;
    };
    [[nodiscard]] Progress progress() const noexcept { return {generated_count_, ready_count_, real_total_}; }

    [[nodiscard]] std::size_t ready_chunk_count() const noexcept { return ready_count_; }
    [[nodiscard]] std::size_t total_chunk_count() const noexcept { return real_total_; }
    [[nodiscard]] TreeEmitCounts object_counts() const noexcept { return total_tree_count_; }

    // Walk mode's analytic ground query (Group V task 23) -- the same height function that
    // generates the terrain, so camera physics and the rendered surface agree by construction.
    [[nodiscard]] float ground_height(float worldX, float worldZ) const {
        return heightmap_.height_at(worldX, worldZ);
    }
    [[nodiscard]] const world::generation::HeightmapGenerator& heightmap() const noexcept {
        return heightmap_;
    }

private:
    struct GenCompletion {
        world::chunk::Chunk chunk;
        bool failed = false;
    };
    struct MeshCompletion {
        world::chunk::ChunkCoord coord;
        world::meshing::MeshData mesh;
        TreeEmitCounts tree_count;
        bool failed = false;
    };

    void request_generation(world::chunk::ChunkCoord coord);
    void drain_generation_completions();
    void consider_mesh_candidate(world::chunk::ChunkCoord coord);
    void drain_mesh_completions();
    [[nodiscard]] bool neighborhood_generated(world::chunk::ChunkCoord coord) const;

    world::streaming::WorldBounds bounds_;
    std::size_t upload_budget_per_tick_ = 4;
    int seed_ = 0;
    glm::vec3 spawn_position_{0.0f};
    std::size_t real_total_ = 0;
    std::size_t generated_count_ = 0;
    std::size_t ready_count_ = 0;

    // Real (to-be-meshed) chunks whose mesh job has not been submitted yet -- shrinks to empty as
    // loading completes. Checked only for the up-to-27 candidates near a chunk that just finished
    // generating (consider_mesh_candidate), never rescanned wholesale: at the 48-radius trial size
    // (~28k real chunks) a naive "rescan everything pending every pump()" is a real O(n^2)-shaped
    // cost across the whole load, not a hypothetical one worth guarding against speculatively.
    world::chunk::CoordSet pending_mesh_;
    world::chunk::CoordSet generated_;
    world::chunk::CoordSet mesh_in_flight_;

    world::chunk::CoordMap<TreeEmitCounts> tree_counts_;
    TreeEmitCounts total_tree_count_;
    world::chunk::ChunkStore store_;
    world::generation::HeightmapGenerator heightmap_;
    render::diligent::TerrainRenderer& renderer_;
    engine::ecs::Registry& registry_;
    engine::events::Dispatcher& dispatcher_;
    world::chunk::CoordMap<engine::ecs::Entity> chunk_entities_;

    mutable std::mutex gen_mutex_;
    std::vector<GenCompletion> gen_completions_;
    mutable std::mutex mesh_mutex_;
    std::vector<MeshCompletion> mesh_completions_;

    // Declaration order is load-bearing (the same lesson chunk_streaming.hpp's own history
    // documents): pool_ is declared LAST so it is destroyed FIRST, joining every worker while the
    // queues/mutexes/heightmap those jobs capture through `this` are all still alive.
    engine::jobs::ThreadPool pool_;
};

} // namespace app
