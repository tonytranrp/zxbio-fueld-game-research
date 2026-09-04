#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <utility>

#include "chunk_streaming.hpp"

#include "engine/core/log.hpp"
#include "world/chunk/chunk_load_state.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"

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

std::int32_t chebyshev(ChunkCoord a, ChunkCoord b) noexcept {
    return std::max({std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z)});
}

} // namespace

ChunkStreamingSystem::ChunkStreamingSystem(world::streaming::StreamingConfig config, int seed,
                                           std::size_t workerThreads,
                                           render::diligent::TerrainRenderer& renderer,
                                           engine::ecs::Registry& registry)
    : streamer_(config), heightmap_(seed), renderer_(renderer), registry_(registry), pool_(workerThreads) {}

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

    drain_generation_completions();
    submit_ready_mesh_jobs();
    drain_mesh_completions();
    apply_unloads(commands.unload);

    const auto& config = streamer_.config();
    const ChunkCoord anchor{cameraChunk.x, std::clamp(cameraChunk.y, config.y_min, config.y_max), cameraChunk.z};
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
            log(LogLevel::Error, "chunk generation failed at [{},{},{}]: {}", coord.x, coord.y, coord.z, e.what());
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
            state.chunk = store_.find(coord); // stable: unordered_map of unique_ptr, no reallocation of the Chunk
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

void ChunkStreamingSystem::submit_ready_mesh_jobs() {
    std::vector<ChunkCoord> abandoned;
    std::vector<ChunkCoord> ready;
    for (const ChunkCoord& coord : pending_mesh_) {
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
            MeshCompletion completion{coord, {}, false};
            try {
                completion.mesh = world::meshing::extract_mesh(*snapshot, coord);
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
    std::vector<MeshCompletion> drained;
    {
        const std::lock_guard guard(mesh_mutex_);
        drained.swap(mesh_completions_);
    }
    for (MeshCompletion& completion : drained) {
        mesh_in_flight_.erase(completion.coord);
        if (completion.failed) {
            streamer_.mark_discarded(completion.coord);
            destroy_chunk_entity(completion.coord);
            continue;
        }
        // Task 24: the stale-result check -- membership in the *current* desired set, decided at
        // completion time, not at submit time.
        if (!streamer_.is_desired(completion.coord)) {
            streamer_.mark_discarded(completion.coord);
            destroy_chunk_entity(completion.coord);
            continue;
        }
        renderer_.upload_chunk_mesh(completion.coord, completion.mesh); // empty mesh -> no GPU entry, still "loaded"
        streamer_.mark_loaded(completion.coord);
        if (const auto it = chunk_entities_.find(completion.coord); it != chunk_entities_.end()) {
            registry_.get<ChunkPipelineState>(it->second).state = ChunkLoadState::Ready;
        }
    }
}

void ChunkStreamingSystem::apply_unloads(const std::vector<ChunkCoord>& coords) {
    for (const ChunkCoord& coord : coords) {
        // Task 26 then task 25, explicitly sequenced: GPU buffers + allocation-tracker decrement
        // first (remove_chunk_mesh), then the ECS pipeline entity, then the voxel data. The
        // >=2-chunk hysteresis gap guarantees no still-desired chunk's meshing halo can reference
        // this coordinate (see StreamingConfig), so dropping the voxels here is safe.
        renderer_.remove_chunk_mesh(coord);
        destroy_chunk_entity(coord);
        store_.erase(coord);
        generated_.erase(coord);
    }
}

void ChunkStreamingSystem::sweep_generation_margin(ChunkCoord anchor) {
    // Halo chunks were generated for meshing but never meshed themselves; drop their voxel data
    // once they are far outside every live neighborhood. Snapshot-based mesh jobs own copies, so
    // a running job can never observe this erase.
    const std::int32_t sweepRadius = streamer_.config().unload_radius + 1;
    std::vector<ChunkCoord> stale;
    for (const ChunkCoord& coord : generated_) {
        if (chebyshev(coord, anchor) > sweepRadius) {
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
