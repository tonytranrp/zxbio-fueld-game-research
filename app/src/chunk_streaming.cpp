#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <utility>

#include "chunk_streaming.hpp"

#include "engine/core/log.hpp"
#include "tree_decoration.hpp"
#include "world/chunk/chunk_load_state.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"
#include "world/streaming/chunk_events.hpp"

// Task 27: zones on every job-system task (generate, mesh, upload) plus the per-frame pump --
// liberal instrumentation is justified by Tracy's measured ~15ns/zone (§2.4), and with
// TRACY_ON_DEMAND the cost only exists while a profiler is attached.
#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN(name)
#endif

namespace app {

namespace {

using engine::core::log;
using engine::core::LogLevel;
using world::chunk::ChunkCoord;
using world::chunk::ChunkLoadState;
using world::chunk::ChunkPipelineState;

// Horizontal only, matching ChunkStreamer's own distance since the ribbon-bug fix: columns are
// the streaming unit, so the generation-margin sweep must not treat altitude as distance either
// (a high camera would otherwise sweep away the low halo chunks meshing still needs).
std::int32_t chebyshev_xz(ChunkCoord a, ChunkCoord b) noexcept {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

} // namespace

ChunkStreamingSystem::ChunkStreamingSystem(world::streaming::StreamingConfig config, int seed,
                                           std::size_t workerThreads,
                                           render::diligent::TerrainRenderer& renderer,
                                           engine::ecs::Registry& registry,
                                           engine::events::Dispatcher& dispatcher,
                                           std::size_t uploadBudgetPerTick)
    : streamer_(config), upload_budget_per_tick_(uploadBudgetPerTick), seed_(seed), heightmap_(seed),
      renderer_(renderer), registry_(registry), dispatcher_(dispatcher), pool_(workerThreads) {}

void ChunkStreamingSystem::update(const glm::vec3& cameraWorldPosition, double nowSeconds) {
    ZoneScopedN("streaming update");
    const ChunkCoord cameraChunk{
        world::chunk::world_to_chunk(static_cast<std::int32_t>(std::floor(cameraWorldPosition.x))),
        world::chunk::world_to_chunk(static_cast<std::int32_t>(std::floor(cameraWorldPosition.y))),
        world::chunk::world_to_chunk(static_cast<std::int32_t>(std::floor(cameraWorldPosition.z))),
    };

    const auto commands = streamer_.tick(cameraChunk, nowSeconds);

    for (const ChunkCoord& coord : commands.start_loading) {
        pending_mesh_.insert(coord);
        const engine::ecs::Entity entity = registry_.create();
        registry_.emplace<ChunkPipelineState>(entity, ChunkLoadState::Requested, nullptr);
        chunk_entities_.emplace(coord, entity);
    }

    const auto& config = streamer_.config();
    const ChunkCoord anchor{cameraChunk.x, std::clamp(cameraChunk.y, config.y_min, config.y_max),
                            cameraChunk.z};

    drain_generation_completions();
    submit_ready_mesh_jobs(anchor);
    drain_mesh_completions();
    apply_unloads(commands.unload);
    release_gpu_meshes_budgeted();
    sweep_generation_margin(anchor);
}

bool ChunkStreamingSystem::settled() const {
    if (!pending_mesh_.empty() || !gen_in_flight_.empty() || !mesh_in_flight_.empty()) {
        return false;
    }
    // The sets above are main-thread state; the queues are the only cross-thread part.
    const std::scoped_lock lock(gen_mutex_, mesh_mutex_);
    return gen_completions_.empty() && mesh_completions_.empty();
}

void ChunkStreamingSystem::request_generation(ChunkCoord coord) {
    if (generated_.contains(coord) || gen_in_flight_.contains(coord)) {
        return;
    }
    gen_in_flight_.insert(coord);
    (void)pool_.submit([this, coord] {
        ZoneScopedN("chunk generate");
        GenCompletion completion{world::chunk::Chunk{coord}, false};
        try {
            world::generation::fill_terrain(completion.chunk, heightmap_);
        } catch (const std::exception& e) {
            log(LogLevel::Error, "chunk generation failed at [{},{},{}]: {}", coord.x, coord.y, coord.z,
                e.what());
            completion.failed = true;
        }
        const std::lock_guard guard(gen_mutex_);
        gen_completions_.push_back(std::move(completion));
    });
}

void ChunkStreamingSystem::drain_generation_completions() {
    std::vector<GenCompletion> drained;
    {
        const std::lock_guard guard(gen_mutex_);
        drained.swap(gen_completions_);
    }
    for (GenCompletion& completion : drained) {
        const ChunkCoord coord = completion.chunk.coord();
        gen_in_flight_.erase(coord);
        if (completion.failed) {
            continue; // not marked generated -- the next submit scan re-requests it (retry with logging)
        }
        // Move the worker-filled voxels into the store's pmr-pooled chunk. pmr move-assignment
        // across different memory resources degrades to an element move -- correct, and it keeps
        // every stored chunk on the store's own pool.
        store_.get_or_create(coord).voxels() = std::move(completion.chunk.voxels());
        generated_.insert(coord);
        if (const auto it = chunk_entities_.find(coord); it != chunk_entities_.end()) {
            auto& state = registry_.get<ChunkPipelineState>(it->second);
            state.state = ChunkLoadState::Generated;
            state.chunk =
                store_.find(coord); // stable: unordered_map of unique_ptr, no reallocation of the Chunk
            // Entity-gated on purpose: halo-only neighbors (generated for meshing, never streamed
            // themselves) have no entity and no lifecycle -- they don't fire events either.
            dispatcher_.trigger(world::streaming::ChunkLoaded{coord});
        }
    }
}

bool ChunkStreamingSystem::neighborhood_generated(ChunkCoord coord) const {
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                if (!generated_.contains(ChunkCoord{coord.x + dx, coord.y + dy, coord.z + dz})) {
                    return false;
                }
            }
        }
    }
    return true;
}

void ChunkStreamingSystem::submit_ready_mesh_jobs(ChunkCoord anchor) {
    // Nearest-first iteration (TERRAIN_FIXES research, both subagents: godot_voxel's
    // distance-banded priority, Sodium/Veloren's select-top-K-per-tick, Minecraft's ticket
    // levels -- no surveyed engine submits terrain work unordered). The backlog already lives
    // app-side in pending_mesh_; sorting the per-tick iteration is the whole cost. With the
    // single-producer main thread, the pool's per-producer FIFO preserves this order, so the
    // upload budget spends itself on the most visible chunks first.
    std::vector<ChunkCoord> byDistance(pending_mesh_.begin(), pending_mesh_.end());
    std::sort(byDistance.begin(), byDistance.end(), [anchor](const ChunkCoord& a, const ChunkCoord& b) {
        const std::int32_t da = chebyshev_xz(a, anchor);
        const std::int32_t db = chebyshev_xz(b, anchor);
        if (da != db) {
            return da < db;
        }
        // Deterministic tiebreak so submission order is reproducible run-to-run.
        if (a.y != b.y) {
            return a.y > b.y; // surface-band chunks (higher y) first within a ring
        }
        return a.x != b.x ? a.x < b.x : a.z < b.z;
    });

    std::vector<ChunkCoord> abandoned;
    std::vector<ChunkCoord> ready;
    for (const ChunkCoord& coord : byDistance) {
        if (!streamer_.is_desired(coord)) {
            abandoned.push_back(coord); // camera left before meshing even started
            continue;
        }
        // Kick off any missing halo generation, then wait for the full 26-neighbor precondition
        // (corner-adjacent included -- face-only waiting is exactly the corner-seam bug the
        // meshing brief warns about).
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dy = -1; dy <= 1; ++dy) {
                for (std::int32_t dx = -1; dx <= 1; ++dx) {
                    request_generation(ChunkCoord{coord.x + dx, coord.y + dy, coord.z + dz});
                }
            }
        }
        if (neighborhood_generated(coord)) {
            ready.push_back(coord);
        }
    }

    for (const ChunkCoord& coord : abandoned) {
        pending_mesh_.erase(coord);
        streamer_.mark_discarded(coord);
        destroy_chunk_entity(coord);
    }

    for (const ChunkCoord& coord : ready) {
        pending_mesh_.erase(coord);
        mesh_in_flight_.insert(coord);
        if (const auto it = chunk_entities_.find(coord); it != chunk_entities_.end()) {
            registry_.get<ChunkPipelineState>(it->second).state = ChunkLoadState::Meshing;
        }

        // Private snapshot of the 27-chunk neighborhood: the job reads only its own copy, so the
        // live store stays main-thread-only with no locking. Paletted chunks make this cheap
        // (homogeneous chunks copy as a 1-entry palette, no index buffer).
        auto snapshot = std::make_shared<world::chunk::ChunkStore>();
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dy = -1; dy <= 1; ++dy) {
                for (std::int32_t dx = -1; dx <= 1; ++dx) {
                    const ChunkCoord neighbor{coord.x + dx, coord.y + dy, coord.z + dz};
                    snapshot->get_or_create(neighbor).voxels() = store_.find(neighbor)->voxels();
                }
            }
        }

        (void)pool_.submit([this, coord, snapshot = std::move(snapshot)] {
            ZoneScopedN("chunk mesh");
            MeshCompletion completion{coord, {}, 0, false};
            try {
                completion.mesh = world::meshing::extract_mesh(*snapshot, coord);
                // Group W: deterministic tree decoration appended into the same mesh -- same
                // compressed vertex format, same buffers, same draw; heightmap_ is safe to read
                // concurrently (stress-tested) and placement is a pure function of seed+coord.
                completion.tree_count = append_tree_meshes(completion.mesh, coord, seed_, heightmap_);
            } catch (const std::exception& e) {
                log(LogLevel::Error, "meshing failed at [{},{},{}]: {}", coord.x, coord.y, coord.z, e.what());
                completion.failed = true;
            }
            const std::lock_guard guard(mesh_mutex_);
            mesh_completions_.push_back(std::move(completion));
        });
    }
}

void ChunkStreamingSystem::drain_mesh_completions() {
    // Per-frame upload budget (TERRAIN_FIXES_BRIEF Group T task 16): when a burst of mesh jobs
    // completes around the same tick, committing every one of them in a single frame is exactly
    // the observed stutter -- each upload is a synchronous GPU buffer creation on the main
    // thread. Take at most N per frame and leave the rest IN the queue: settled(), the stats
    // counters, and the mesh_in_flight_ bookkeeping all stay truthful automatically because a
    // deferred completion simply hasn't been drained yet. The default (4/frame at 60fps) still
    // commits 240 chunks/s -- an entire radius-3 initial load spreads across ~1.2s instead of one
    // multi-hundred-ms frame. 0 = unlimited, kept as a flag so the stutter is A/B-measurable on
    // one binary (task 17's before/after).
    std::vector<MeshCompletion> drained;
    {
        const std::lock_guard guard(mesh_mutex_);
        // Backlog-proportional floor: a FIXED per-frame count starves when the frame rate itself
        // collapses under generation load (measured here: a debug-build initial load at ~1.5fps
        // turned budget 4 into ~6 chunks/s while 1700+ completions piled up -- the exact
        // budgeter pathology godot_voxel's docs warn about). Draining at least a quarter of the
        // backlog keeps bursts geometrically decaying instead of unbounded, while small backlogs
        // still spread across frames.
        const std::size_t floorTake = mesh_completions_.size() / 4;
        const std::size_t take =
            upload_budget_per_tick_ == 0
                ? mesh_completions_.size()
                : std::min(std::max(upload_budget_per_tick_, floorTake), mesh_completions_.size());
        drained.assign(
            std::make_move_iterator(mesh_completions_.begin()),
            std::make_move_iterator(mesh_completions_.begin() + static_cast<std::ptrdiff_t>(take)));
        mesh_completions_.erase(mesh_completions_.begin(),
                                mesh_completions_.begin() + static_cast<std::ptrdiff_t>(take));
    }
    for (MeshCompletion& completion : drained) {
        mesh_in_flight_.erase(completion.coord);
        if (completion.failed) {
            streamer_.mark_discarded(completion.coord);
            destroy_chunk_entity(completion.coord);
            continue;
        }
        // Deliberate change from the original "discard if no longer desired" (task 24): a stale
        // completion is now APPLIED anyway and left to the normal unload hysteresis. Discarding
        // was only an optimization, and under the upload budget it created a measured feedback
        // loop: delayed completions went stale at autofly speed, every discard re-queued a fresh
        // mesh job (a 27-chunk snapshot copy on the main thread), the copies collapsed the frame
        // rate, and lower fps made MORE completions stale -- 0 chunks ever became ready at
        // ~1.4fps. Applying finished work is strictly cheaper than redoing it; a no-longer-
        // desired chunk simply unloads through the standard R_unload+delay path moments later.
        renderer_.upload_chunk_mesh(completion.coord,
                                    completion.mesh); // empty mesh -> no GPU entry, still "loaded"
        total_tree_count_ += completion.tree_count;
        tree_counts_.insert_or_assign(completion.coord, completion.tree_count);
        streamer_.mark_loaded(completion.coord);
        if (const auto it = chunk_entities_.find(completion.coord); it != chunk_entities_.end()) {
            registry_.get<ChunkPipelineState>(it->second).state = ChunkLoadState::Ready;
        }
        dispatcher_.trigger(
            world::streaming::ChunkMeshReady{completion.coord, completion.mesh.vertices.size()});
    }
}

void ChunkStreamingSystem::apply_unloads(const std::vector<ChunkCoord>& coords) {
    for (const ChunkCoord& coord : coords) {
        // ECS entity and voxel data go immediately (cheap, and the >=2-chunk hysteresis gap
        // guarantees no still-desired chunk's meshing halo can reference this coordinate -- see
        // StreamingConfig). The ChunkUnloaded event fires NOW -- it means "left the streamed
        // set", which is true at command time -- keeping the event-derived overlay count equal
        // to the streamer's. Only the GPU buffer destruction is deferred to the budgeted queue:
        // a boundary crossing unloads a whole trailing face of columns at once, and destroying
        // that many buffers in one tick is itself a measured main-thread spike class (godot_
        // voxel's Tracy finding, via this pass's research).
        destroy_chunk_entity(coord);
        store_.erase(coord);
        generated_.erase(coord);
        if (const auto it = tree_counts_.find(coord); it != tree_counts_.end()) {
            total_tree_count_ -= it->second;
            tree_counts_.erase(it);
        }
        dispatcher_.trigger(world::streaming::ChunkUnloaded{coord});
        gpu_release_queue_.push_back(coord);
    }
}

void ChunkStreamingSystem::release_gpu_meshes_budgeted() {
    // Same per-tick budget shape as the upload side; 0 = unlimited. A re-desired coordinate
    // re-uploads through upload_chunk_mesh, which replaces any existing entry wholesale, so a
    // stale queued release of a coordinate that streamed back IN must be skipped -- releasing it
    // would delete the fresh mesh. is_desired() at release time is that guard.
    // Same backlog/4 floor as the drain side, for the same measured reason: a fixed count
    // starves when frames are slow -- here that starvation showed up as 34k undestroyed meshes
    // and 1.1 GiB of GPU buffers by the end of a low-fps autofly run.
    const std::size_t releaseFloor = gpu_release_queue_.size() / 4;
    const std::size_t budget =
        upload_budget_per_tick_ == 0
            ? gpu_release_queue_.size()
            : std::min(std::max(upload_budget_per_tick_, releaseFloor), gpu_release_queue_.size());
    for (std::size_t i = 0; i < budget; ++i) {
        const ChunkCoord coord = gpu_release_queue_[i];
        if (!streamer_.is_desired(coord)) {
            renderer_.remove_chunk_mesh(coord);
        }
    }
    gpu_release_queue_.erase(gpu_release_queue_.begin(),
                             gpu_release_queue_.begin() + static_cast<std::ptrdiff_t>(budget));
}

void ChunkStreamingSystem::sweep_generation_margin(ChunkCoord anchor) {
    // Halo chunks were generated for meshing but never meshed themselves; drop their voxel data
    // once they are far outside every live neighborhood. Snapshot-based mesh jobs own copies, so
    // a running job can never observe this erase.
    const std::int32_t sweepRadius = streamer_.config().unload_radius + 1;
    std::vector<ChunkCoord> stale;
    for (const ChunkCoord& coord : generated_) {
        if (chebyshev_xz(coord, anchor) > sweepRadius) {
            stale.push_back(coord);
        }
    }
    for (const ChunkCoord& coord : stale) {
        store_.erase(coord);
        generated_.erase(coord);
    }
}

void ChunkStreamingSystem::destroy_chunk_entity(ChunkCoord coord) {
    if (const auto it = chunk_entities_.find(coord); it != chunk_entities_.end()) {
        registry_.destroy(it->second);
        chunk_entities_.erase(it);
    }
}

} // namespace app
