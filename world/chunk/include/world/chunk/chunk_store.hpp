#pragma once

#include <memory>
#include <memory_resource>
#include <unordered_map>

#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_coord.hpp"

namespace world::chunk {

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
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> chunks_;
};

} // namespace world::chunk
