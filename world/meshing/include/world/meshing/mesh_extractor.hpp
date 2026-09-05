#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "engine/core/math.hpp"
#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/meshing/mesh_data.hpp"

namespace world::meshing {

// Padded local-space voxel sampling for one chunk's 3x3x3 neighborhood, resolved ONCE per
// extraction rather than a ChunkStore::find() per voxel sample: ~295k hash finds per extraction
// each construct/destroy an unordered_map iterator, and MSVC's debug-iterator bookkeeping
// (_ITERATOR_DEBUG_LEVEL=2) routes every one through a single global lock -- 16 concurrent mesh
// jobs ran ~80x slower than one, live-stalling chunk streaming's initial load. An array index per
// sample is also simply cheaper than a hash find in release builds.
//
// Public (moved out of mesh_extractor.cpp) so a caller that already holds resolved neighbor
// pointers -- WorldLoader, which resolves them via ChunkStore::find() on the single thread that
// ever mutates the store, exactly once per meshed chunk -- can hand them to a background worker
// directly instead of paying for a whole separate ChunkStore + deep-copied ChunkVoxels per
// neighbor (goal: real chunk-generation profiling pass, 2026-09-05). The resolved pointers stay
// valid for the worker to read because a chunk's voxel data is write-once (frozen the instant
// generation completes) and ChunkStore's pointees are heap-stable across its map's rehashing
// (see chunk_store.hpp) -- only the initial 27 store.find() calls need to happen on the safe,
// single-writer thread; reading through the resulting pointers afterward never races anything.
class NeighborCache {
public:
    using Neighbors = std::array<const world::chunk::Chunk*, 27>;

    NeighborCache(const world::chunk::ChunkStore& store, world::chunk::ChunkCoord base)
        : chunks_(resolve(store, base)) {}
    explicit NeighborCache(Neighbors resolved) noexcept : chunks_(resolved) {}

    // Resolves all 27 (26 neighbors + self) pointers up front. Safe to call only from the thread
    // that owns/mutates `store` (see the class comment) -- the returned pointers are then safe to
    // read from anywhere, including a background worker.
    [[nodiscard]] static Neighbors resolve(const world::chunk::ChunkStore& store,
                                           world::chunk::ChunkCoord base) {
        Neighbors result{};
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dy = -1; dy <= 1; ++dy) {
                for (std::int32_t dx = -1; dx <= 1; ++dx) {
                    result[slot(dx, dy, dz)] =
                        store.find(world::chunk::ChunkCoord{base.x + dx, base.y + dy, base.z + dz});
                }
            }
        }
        return result;
    }

    [[nodiscard]] world::chunk::MaterialID sample(std::int32_t lx, std::int32_t ly, std::int32_t lz) const;
    [[nodiscard]] world::chunk::MaterialID sample(glm::ivec3 p) const { return sample(p.x, p.y, p.z); }

private:
    [[nodiscard]] static std::size_t slot(std::int32_t dx, std::int32_t dy, std::int32_t dz) noexcept {
        return static_cast<std::size_t>(dx + 1) + static_cast<std::size_t>(dy + 1) * 3 +
               static_cast<std::size_t>(dz + 1) * 9;
    }

    Neighbors chunks_{};
};

// Naive Surface Nets. No DiligentCore types cross this boundary -- MeshData is plain vertex/index
// vectors, per project brief §3's render/interface boundary.
//
// Precondition the caller (the streaming system) is responsible for: all 26 neighbors of `coord`
// (6 face + 12 edge + 8 corner) must be at least generated -- ChunkStore::find() returns non-null
// -- before calling this, not just the 6 face-neighbors; edge/corner-adjacent cells need
// edge/corner-neighbor voxel data too. An ungenerated neighbor is treated as all-Air rather than
// crashing, but that is a safety net against a crash, not a substitute for the real precondition
// -- it silently produces incomplete boundary geometry instead.
MeshData extract_mesh(const world::chunk::ChunkStore& store, world::chunk::ChunkCoord coord);

// Same extraction, against an already-resolved neighborhood (see NeighborCache) -- the path a
// caller that has already done its own store lookups (WorldLoader) should use instead of paying
// for extract_mesh(store, coord)'s own redundant resolve.
MeshData extract_mesh(const NeighborCache& neighbors);

} // namespace world::meshing
