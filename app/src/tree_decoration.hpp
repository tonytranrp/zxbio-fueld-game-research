#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "world/chunk/chunk_coord.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_placement.hpp"
#include "world/meshing/mesh_data.hpp"

namespace app {

// Procedural tree decoration (TERRAIN_FIXES_BRIEF Group W). Scope per task 28: composed simple
// primitives (a box trunk + an octahedron canopy), no imported assets, no new render path --
// tree geometry is APPENDED to the owning chunk's own terrain mesh inside the mesh job (task 32's
// written decision: same compressed vertex format, same PSO, same per-chunk buffers, so culling,
// upload budgeting, and streaming lifecycle all apply for free).
//
// Micro-voxel pivot (docs/goals.md Group W): placement + implicit shape moved to
// world/generation/tree_placement.hpp so the sparse-brick-octree voxelizer shares the exact same
// deterministic trees. The names below are re-exported unchanged for this app's own callers and
// tests; only the MESH emission stays here.
using world::generation::compute_tree_placements;
using world::generation::kTreeMaxHeight;
using world::generation::kTreeMaxSlope;
using world::generation::kTreeMinHeight;
using world::generation::kTreeMinSpacing;
using world::generation::TreePlacement;
using world::generation::TreeShape;

// Per-shape emission tally (goal 82: the overlay breaks "objects" down by silhouette).
struct TreeEmitCounts {
    std::size_t round = 0;
    std::size_t conifer = 0;
    std::size_t shrub = 0;
    [[nodiscard]] std::size_t total() const noexcept { return round + conifer + shrub; }
    TreeEmitCounts& operator+=(const TreeEmitCounts& o) noexcept {
        round += o.round;
        conifer += o.conifer;
        shrub += o.shrub;
        return *this;
    }
    TreeEmitCounts& operator-=(const TreeEmitCounts& o) noexcept {
        round -= o.round;
        conifer -= o.conifer;
        shrub -= o.shrub;
        return *this;
    }
};

// Appends tree geometry for every placement whose base surface lies inside THIS chunk's Y range
// and whose full height fits under the chunk's local ceiling (trees straddling a chunk top are
// skipped -- documented v1 simplification; the boundary layer keeps the skip deterministic).
// Returns the per-shape emission counts.
TreeEmitCounts append_tree_meshes(world::meshing::MeshData& mesh, world::chunk::ChunkCoord chunk, int seed,
                                  const world::generation::HeightmapGenerator& heightmap);

} // namespace app
