#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "world/chunk/chunk_coord.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/meshing/mesh_data.hpp"

namespace app {

// Procedural tree decoration (TERRAIN_FIXES_BRIEF Group W). Scope per task 28: composed simple
// primitives (a box trunk + an octahedron canopy), no imported assets, no new render path --
// tree geometry is APPENDED to the owning chunk's own terrain mesh inside the mesh job (task 32's
// written decision: same compressed vertex format, same PSO, same per-chunk buffers, so culling,
// upload budgeting, and streaming lifecycle all apply for free).
//
// Determinism (task 30): placement is a pure function of (seed, chunk column, heightfield) --
// a jittered 8-voxel grid keyed by a splitmix-style hash, masked by the same analytic height
// query walk mode uses (no water, no steep slopes, no high altitude). Evaluated at mesh time,
// but generation-DERIVED: the same seed + coordinate reproduces identical trees on every run,
// which is the property the determinism standard actually cares about.

// Silhouette variants (goal 36, research/water-foliage-design.md): selected deterministically
// from the placement key -- shape variety, not a new placement or rendering system.
enum class TreeShape : std::uint8_t {
    Round,   // box trunk + single octahedron canopy (the original)
    Conifer, // taller/thinner trunk + 3 stacked shrinking octahedra
    Shrub,   // no trunk; one squashed octahedron on the ground
};

struct TreePlacement {
    float world_x = 0.0f;
    float world_z = 0.0f;
    float base_height = 0.0f;   // terrain surface world-Y at the tree's column
    float trunk_height = 0.0f;  // world units above base
    float canopy_radius = 0.0f; // octahedron half-extent
    TreeShape shape = TreeShape::Round;
    // Goal 38: per-tree brightness jitter in [0.80, 1.0], carried through the vertex AO
    // attribute (tree geometry is unoccluded by construction, so the byte is free).
    float color_jitter = 1.0f;
};

inline constexpr float kTreeMinSpacing = 4.0f;  // guaranteed by grid cell 8 + jitter range [2,6]
// Calibrated against the real generator, not guessed: this terrain's MEAN per-voxel slope is
// ~1.34 (heightmap smoothness test's own measurement), so a "gentle ground only" 0.8 threshold
// rejected almost every column. 2.0 keeps trees off genuine cliff faces while accepting typical
// hillsides.
inline constexpr float kTreeMaxSlope = 2.0f;    // per-axis central-difference height slope
inline constexpr float kTreeMinHeight = 1.5f;   // above sea level (0): no beach/water trees
inline constexpr float kTreeMaxHeight = 45.0f;  // tree line

// All candidate trees for one chunk COLUMN (y ignored), after masking. Deterministic.
[[nodiscard]] std::vector<TreePlacement> compute_tree_placements(
    std::int32_t chunkX, std::int32_t chunkZ, int seed, const world::generation::HeightmapGenerator& heightmap);

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
