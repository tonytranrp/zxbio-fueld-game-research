#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <utility>

#include "world_loader.hpp"

#include "engine/core/log.hpp"
#include "tree_decoration.hpp"
#include "world/chunk/chunk_load_state.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"
#include "world/streaming/chunk_events.hpp"

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

std::int32_t chebyshev_xz(ChunkCoord a, ChunkCoord b) noexcept {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

// The old streaming system's "at least a quarter of the backlog" floor (drain_mesh_completions'
// own comment explains why: a fixed per-frame count starves when fps collapses under load) was
// tuned against backlogs bounded by a small streaming radius -- at most a few hundred chunks. A
// pregenerated 48-radius world's backlog can be tens of thousands, where "a quarter" is itself
// thousands: measured directly, an unbounded generation drain let a handful of pump() calls
// swallow an entire 10k-chunk, 68-second load, making --frames stop meaning anything predictable
// and defeating goal 130's real-moving-progress-bar intent. A hard ceiling keeps the floor's
// anti-starvation property (still drains proportionally more under real backlog pressure) without
// letting one call's cost scale with the whole world's size.
constexpr std::size_t kMaxDrainBurst = 256;

// budget == 0 stays genuinely unlimited (drains the whole backlog in one call) -- kept deliberately
// unbounded, same as the old streaming system's identical convention, so the pre-fix stutter case
// is still reproducible on demand for A/B measurement rather than silently capped underneath it.
std::size_t take_count(std::size_t budget, std::size_t backlogSize) {
    if (budget == 0) {
        return backlogSize;
    }
    const std::size_t floorTake = backlogSize / 4;
    return std::min({std::max(budget, floorTake), backlogSize, kMaxDrainBurst});
}

} // namespace

WorldLoader::WorldLoader(world::streaming::WorldBounds bounds, int seed, std::size_t workerThreads,
                         render::diligent::TerrainRenderer& renderer, engine::ecs::Registry& registry,
                         engine::events::Dispatcher& dispatcher, glm::vec3 spawnWorldPosition,
                         std::size_t uploadBudgetPerTick)
    : bounds_(bounds), upload_budget_per_tick_(uploadBudgetPerTick), seed_(seed),
      spawn_position_(spawnWorldPosition), heightmap_(seed), renderer_(renderer), registry_(registry),
      dispatcher_(dispatcher), pool_(workerThreads) {}

void WorldLoader::begin() {
    ZoneScopedN("world load begin");
    const ChunkCoord spawnChunk{
        world::chunk::world_to_chunk(static_cast<std::int32_t>(std::floor(spawn_position_.x))),
        0,
        world::chunk::world_to_chunk(static_cast<std::int32_t>(std::floor(spawn_position_.z))),
    };

    auto real = world::streaming::chunks_in_bounds(bounds_);
    real_total_ = real.size();
    for (const ChunkCoord& coord : real) {
        pending_mesh_.insert(coord);
        const engine::ecs::Entity entity = registry_.create();
        registry_.emplace<ChunkPipelineState>(entity, ChunkLoadState::Requested, nullptr);
        chunk_entities_.emplace(coord, entity);
    }

    // The 1-chunk-wide meshing halo (SS3's own note): every real chunk's 26-neighbor precondition
    // must be satisfied, and for a chunk at the world's own edge that means generating (never
    // meshing) one extra ring beyond the real bounds in every direction, vertical included.
    const world::streaming::WorldBounds haloBounds{bounds_.radius_chunks + 1, bounds_.y_min - 1,
                                                   bounds_.y_max + 1};
    auto generationSet = world::streaming::chunks_in_bounds(haloBounds);

    // Nearest-to-spawn first (TERRAIN_FIXES research precedent this project already established
    // for streaming submission order): purely a loading-screen UX nicety -- the world coalesces
    // around the player's own spawn point first -- with zero effect on correctness, since every
    // coordinate in generationSet gets requested regardless of order.
    std::sort(generationSet.begin(), generationSet.end(),
              [spawnChunk](const ChunkCoord& a, const ChunkCoord& b) {
                  return chebyshev_xz(a, spawnChunk) < chebyshev_xz(b, spawnChunk);
              });

    for (const ChunkCoord& coord : generationSet) {
        request_generation(coord);
    }
    log(LogLevel::Info, "world load: {} real chunks, {} total (incl. halo), {} worker threads", real_total_,
        generationSet.size(), pool_.thread_count());
}

void WorldLoader::request_generation(ChunkCoord coord) {
    (void)pool_.submit([this, coord] {
        ZoneScopedN("chunk generate");
        GenCompletion completion{world::chunk::Chunk{coord}, false};
        const auto genStart = std::chrono::steady_clock::now();
        try {
            world::generation::fill_terrain(completion.chunk, heightmap_);
        } catch (const std::exception& e) {
            log(LogLevel::Error, "chunk generation failed at [{},{},{}]: {}", coord.x, coord.y, coord.z,
                e.what());
            completion.failed = true;
        }
        generation_ns_.fetch_add(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - genStart)
                .count(),
            std::memory_order_relaxed);
        const std::lock_guard guard(gen_mutex_);
        gen_completions_.push_back(std::move(completion));
    });
}

void WorldLoader::drain_generation_completions() {
    // Same budgeted-drain shape as drain_mesh_completions, and for the same reason the old
    // streaming system already learned the hard way: this side has no GPU cost, but each
    // completion can cascade into consider_mesh_candidate resolving a 27-neighbor pointer view and
    // submitting a mesh job, and worker threads (16 here) generate far faster than one pump() call
    // can be allowed to
    // drain unbounded -- confirmed for real, not hypothetically: a --frames 60 run at radius 20
    // finished the ENTIRE 68.9s, 10k-chunk load inside those 60 iterations, because an unbounded
    // drain here let a handful of pump() calls each swallow a massive backlog. That is exactly
    // the "not a real, moving progress indicator" failure goal 130 warns against, and it made
    // --frames stop meaning anything predictable as a testing knob.
    std::vector<GenCompletion> drained;
    {
        const std::lock_guard guard(gen_mutex_);
        const std::size_t take = take_count(upload_budget_per_tick_, gen_completions_.size());
        drained.assign(std::make_move_iterator(gen_completions_.begin()),
                       std::make_move_iterator(gen_completions_.begin() + static_cast<std::ptrdiff_t>(take)));
        gen_completions_.erase(gen_completions_.begin(),
                               gen_completions_.begin() + static_cast<std::ptrdiff_t>(take));
    }
    for (GenCompletion& completion : drained) {
        const ChunkCoord coord = completion.chunk.coord();
        if (completion.failed) {
            continue; // never marked generated; a static world has no retry-on-next-tick to lean
                      // on (there is no "next tick" scan), so a failure here silently leaves that
                      // one chunk's real neighbors permanently unmeshable -- acceptable for now
                      // (a generation failure is an OOM/exception-class event, not a normal path)
                      // but worth knowing: it is not the same silent-recoverable case the old
                      // streaming system had.
        }
        store_.get_or_create(coord).voxels() = std::move(completion.chunk.voxels());
        generated_.insert(coord);
        ++generated_count_;
        if (const auto it = chunk_entities_.find(coord); it != chunk_entities_.end()) {
            auto& state = registry_.get<ChunkPipelineState>(it->second);
            state.state = ChunkLoadState::Generated;
            state.chunk = store_.find(coord);
            dispatcher_.trigger(world::streaming::ChunkLoaded{coord});
        }
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dy = -1; dy <= 1; ++dy) {
                for (std::int32_t dx = -1; dx <= 1; ++dx) {
                    consider_mesh_candidate(ChunkCoord{coord.x + dx, coord.y + dy, coord.z + dz});
                }
            }
        }
    }
}

bool WorldLoader::neighborhood_generated(ChunkCoord coord) const {
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

void WorldLoader::consider_mesh_candidate(ChunkCoord coord) {
    if (!pending_mesh_.contains(coord) || !neighborhood_generated(coord)) {
        return;
    }
    pending_mesh_.erase(coord);
    mesh_in_flight_.insert(coord);
    if (const auto it = chunk_entities_.find(coord); it != chunk_entities_.end()) {
        registry_.get<ChunkPipelineState>(it->second).state = ChunkLoadState::Meshing;
    }

    // Resolved on the main thread -- the only thread that ever mutates store_ -- exactly once per
    // meshed chunk. world::meshing::NeighborCache's own doc comment explains why the resulting
    // pointers are then safe for a worker thread to read with no further synchronization: a
    // chunk's voxel data is write-once (frozen the instant its own generation completes, and every
    // one of these 27 neighbors is already generated by construction -- neighborhood_generated()
    // just confirmed it), and ChunkStore's pointees are heap-stable across the map's own
    // rehashing, so a concurrent store_.get_or_create() for some OTHER, unrelated coordinate on
    // this same thread later can never invalidate or move the Chunk objects these pointers name.
    //
    // This replaces the old per-candidate "allocate a whole new ChunkStore (its own synchronized
    // memory pool + hash map), deep-copy 27 ChunkVoxels into it" snapshot the redesign had carried
    // over unchanged from the old per-tick streaming system. That copy was real, safe, and
    // necessary THERE (a live store under concurrent per-tick mutation); it is only pure overhead
    // here, where the whole neighborhood is already frozen forever before this function is even
    // called -- ~56k allocate-and-copy cycles run synchronously on the main thread turned out to
    // be a real, measured contributor to load time, not a hypothetical one (chunk-generation
    // profiling pass, 2026-09-05).
    const auto neighbors = world::meshing::NeighborCache::resolve(store_, coord);

    (void)pool_.submit([this, coord, neighbors] {
        ZoneScopedN("chunk mesh");
        MeshCompletion completion{coord, {}, {}, false};
        const auto meshStart = std::chrono::steady_clock::now();
        try {
            completion.mesh = world::meshing::extract_mesh(world::meshing::NeighborCache{neighbors});
            completion.tree_count = append_tree_meshes(completion.mesh, coord, seed_, heightmap_);
        } catch (const std::exception& e) {
            log(LogLevel::Error, "meshing failed at [{},{},{}]: {}", coord.x, coord.y, coord.z, e.what());
            completion.failed = true;
        }
        mesh_ns_.fetch_add(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - meshStart)
                .count(),
            std::memory_order_relaxed);
        const std::lock_guard guard(mesh_mutex_);
        mesh_completions_.push_back(std::move(completion));
    });
}

void WorldLoader::drain_mesh_completions() {
    // Same budgeted-drain shape as the old streaming system's stutter fix (TERRAIN_FIXES_BRIEF
    // Group T task 16), reused here per the redesign doc's own note: a large one-time upload burst
    // benefits from exactly the pacing logic built for streaming bursts. There is no "is this
    // still desired" staleness check anymore -- a static world's real set never shrinks, so every
    // finished mesh is unconditionally wanted.
    std::vector<MeshCompletion> drained;
    {
        const std::lock_guard guard(mesh_mutex_);
        const std::size_t take = take_count(upload_budget_per_tick_, mesh_completions_.size());
        drained.assign(
            std::make_move_iterator(mesh_completions_.begin()),
            std::make_move_iterator(mesh_completions_.begin() + static_cast<std::ptrdiff_t>(take)));
        mesh_completions_.erase(mesh_completions_.begin(),
                                mesh_completions_.begin() + static_cast<std::ptrdiff_t>(take));
    }
    for (MeshCompletion& completion : drained) {
        mesh_in_flight_.erase(completion.coord);
        if (completion.failed) {
            continue; // see drain_generation_completions' failure note -- same honest limitation
        }
        renderer_.upload_chunk_mesh(completion.coord, completion.mesh);
        total_tree_count_ += completion.tree_count;
        tree_counts_.insert_or_assign(completion.coord, completion.tree_count);
        ++ready_count_;
        if (const auto it = chunk_entities_.find(completion.coord); it != chunk_entities_.end()) {
            registry_.get<ChunkPipelineState>(it->second).state = ChunkLoadState::Ready;
        }
        dispatcher_.trigger(
            world::streaming::ChunkMeshReady{completion.coord, completion.mesh.vertices.size()});
    }
}

void WorldLoader::pump() {
    ZoneScopedN("world load pump");
    drain_generation_completions();
    drain_mesh_completions();
}

void WorldLoader::log_timings() const {
    const double generationSeconds =
        std::chrono::duration<double>(std::chrono::nanoseconds(generation_ns_.load())).count();
    const double meshSeconds =
        std::chrono::duration<double>(std::chrono::nanoseconds(mesh_ns_.load())).count();
    // generated_count_/ready_count_ (not real_total_/generationSet.size(), neither of which
    // survives past begin()) are the two persistent counters guaranteed to hold their final,
    // full totals by the time a caller observes finished(): every real chunk's 26-neighbor
    // precondition (which covers the entire halo by construction) must already be satisfied for
    // ready_count_ to have reached real_total_, which means every chunk this load ever generated
    // -- halo included -- already ran through drain_generation_completions().
    const double generationMsPerChunk =
        generated_count_ > 0 ? generationSeconds * 1000.0 / static_cast<double>(generated_count_) : 0.0;
    const double meshMsPerChunk =
        ready_count_ > 0 ? meshSeconds * 1000.0 / static_cast<double>(ready_count_) : 0.0;
    log(LogLevel::Info,
        "world load timings: generation {:.2f}s CPU-time over {} chunks ({:.3f}ms/chunk avg), "
        "meshing {:.2f}s CPU-time over {} chunks ({:.3f}ms/chunk avg), {} worker threads",
        generationSeconds, generated_count_, generationMsPerChunk, meshSeconds, ready_count_, meshMsPerChunk,
        pool_.thread_count());
}

} // namespace app
