#pragma once

#include <cstddef>

#include "world/chunk/chunk_coord.hpp"

namespace world::streaming {

// Chunk lifecycle events (engine-hardening brief §8's proof case). Plain PODs with no
// dispatcher dependency: this module defines WHAT happened; engine/events carries HOW it is
// delivered. Fired from the main thread only, during ChunkStreamingSystem's completion drains
// (see engine/events/dispatcher.hpp for the threading rule).

// Voxel data for this coordinate landed in the ChunkStore (generation applied). Fires before the
// chunk is meshed/visible — a save system would care from this point on.
struct ChunkLoaded {
    world::chunk::ChunkCoord coord;
};

// Mesh uploaded (or confirmed empty) and the chunk counts as ready/streamed-in. This is the
// moment ChunkStreamer::mark_loaded runs — the overlay's "ready chunks" count is defined by
// these events paired with ChunkUnloaded.
struct ChunkMeshReady {
    world::chunk::ChunkCoord coord;
    std::size_t vertex_count = 0; // 0 for an all-air/no-surface chunk (still ready)
};

// Chunk left the streamed set: GPU mesh removed, ECS entity destroyed, voxel data dropped.
struct ChunkUnloaded {
    world::chunk::ChunkCoord coord;
};

} // namespace world::streaming
