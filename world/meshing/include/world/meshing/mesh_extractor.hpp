#pragma once

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/meshing/mesh_data.hpp"

namespace world::meshing {

// Naive Surface Nets. No DiligentCore types cross this boundary -- MeshData is plain vertex/index
// vectors, per PROJECT_BRIEF.md §3's render/interface boundary.
//
// Precondition the caller (the streaming system) is responsible for: all 26 neighbors of `coord`
// (6 face + 12 edge + 8 corner) must be at least generated -- ChunkStore::find() returns non-null
// -- before calling this, not just the 6 face-neighbors; edge/corner-adjacent cells need
// edge/corner-neighbor voxel data too. An ungenerated neighbor is treated as all-Air rather than
// crashing, but that is a safety net against a crash, not a substitute for the real precondition
// -- it silently produces incomplete boundary geometry instead.
MeshData extract_mesh(const world::chunk::ChunkStore& store, world::chunk::ChunkCoord coord);

} // namespace world::meshing
