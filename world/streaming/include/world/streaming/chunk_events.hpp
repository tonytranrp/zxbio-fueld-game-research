#pragma once

#include <cstddef>

#include "world/chunk/chunk_coord.hpp"

namespace world::streaming {

// Chunk lifecycle events (engine-hardening brief §8's proof case). Plain PODs with no
// dispatcher dependency: this module defines WHAT happened; engine/events carries HOW it is
// delivered. Fired from the main thread only, during WorldLoader's completion drains (see
// engine/events/dispatcher.hpp for the threading rule).
//
// Group S note (Voxel Representation Redesign SS3.4): `ChunkUnloaded` is gone. A static,
// pregenerated world never tears a chunk back down once loaded -- keeping that event around after
// nothing could ever fire it would be exactly the dead, untested code path SS3.4 says to remove
// deliberately rather than leave inert.

// Voxel data for this coordinate landed in the ChunkStore (generation applied). Fires before the
// chunk is meshed/visible — a save system would care from this point on.
struct ChunkLoaded {
    world::chunk::ChunkCoord coord;
};

// Mesh uploaded (or confirmed empty) and the chunk counts as ready. This is the moment
// WorldLoader's loading-screen progress bar (goal 130) advances by one.
struct ChunkMeshReady {
    world::chunk::ChunkCoord coord;
    std::size_t vertex_count = 0; // 0 for an all-air/no-surface chunk (still ready)
};

} // namespace world::streaming
