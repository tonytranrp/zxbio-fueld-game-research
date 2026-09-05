#include "world/chunk/chunk_store.hpp"

namespace world::chunk {

Chunk& ChunkStore::get_or_create(ChunkCoord coord) {
    auto it = chunks_.find(coord);
    if (it != chunks_.end()) {
        return *it->second;
    }
    auto [inserted, _] = chunks_.emplace(coord, std::make_unique<Chunk>(coord, &pool_));
    return *inserted->second;
}

Chunk* ChunkStore::find(ChunkCoord coord) const {
    auto it = chunks_.find(coord);
    return it != chunks_.end() ? it->second.get() : nullptr;
}

void ChunkStore::erase(ChunkCoord coord) {
    chunks_.erase(coord);
}

} // namespace world::chunk
