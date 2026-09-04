#pragma once

#include <memory>
#include <memory_resource>

#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/coord_containers.hpp"

namespace world::chunk {

// The coordinate-keyed map type behind ChunkStore, aliased so re-evaluating the container is a
// one-line change (ENGINE_HARDENING_BRIEF.md Group H task 9), not another migration -- exercised
// for real when the std::unordered_map this alias was born wrapping became CoordMap (boost flat;
// see coord_containers.hpp for the benchmark-backed decision). Callers hold Chunk*/Chunk& (the
// unique_ptr's pointee, heap-stable across any rehash of any map type) -- never map iterators or
// references to the mapped unique_ptr itself -- so this alias does NOT require a node-stable map
// (task 6's audit: the only long-lived pointer is ChunkPipelineState::chunk, pointee-stable by
// construction).
using ChunkMap = CoordMap<std::unique_ptr<Chunk>>;

// Owns every loaded chunk (PROJECT_BRIEF.md §5: unique_ptr in a coordinate-keyed map; nothing
// else holds a chunk by pointer for longer than one job's lifetime) and the pmr pool their voxel
// storage allocates from.
class ChunkStore {
public:
    ChunkStore() = default;
    ChunkStore(const ChunkStore&) = delete;
    ChunkStore& operator=(const ChunkStore&) = delete;

    Chunk& get_or_create(ChunkCoord coord);
    [[nodiscard]] Chunk* find(ChunkCoord coord) const;
    void erase(ChunkCoord coord);
    [[nodiscard]] std::size_t size() const noexcept { return chunks_.size(); }

private:
    std::pmr::synchronized_pool_resource pool_;
    ChunkMap chunks_;
};

} // namespace world::chunk
