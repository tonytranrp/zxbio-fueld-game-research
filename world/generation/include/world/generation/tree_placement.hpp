#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/generation/heightmap_generator.hpp"

namespace world::generation {

// Procedural tree PLACEMENT + implicit SHAPE (micro-voxel pivot, docs/goals.md Group W): moved
// here from app/src/tree_decoration.cpp (where it lived as TERRAIN_FIXES_BRIEF Group W's mesh
// decoration) so two consumers can share one deterministic definition -- the greedy-mesh path still
// emits box trunks + octahedron canopies from these placements, and the sparse-brick-octree path
// voxelizes the exact same implicit shapes at whatever voxel size the tree is built at. Placement
// is a pure function of (seed, chunk column, heightfield): a jittered 8-voxel grid keyed by a
// splitmix-style hash, masked by the same analytic height query walk mode uses (no water, no
// steep slopes, no high altitude). The same seed + coordinate reproduces identical trees on every
// run, which is the property the determinism standard actually cares about.

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

inline constexpr float kTreeMinSpacing = 4.0f; // guaranteed by grid cell 8 + jitter range [2,6]
// Calibrated against the real generator, not guessed: this terrain's MEAN per-voxel slope is
// ~1.34 (heightmap smoothness test's own measurement), so a "gentle ground only" 0.8 threshold
// rejected almost every column. 2.0 keeps trees off genuine cliff faces while accepting typical
// hillsides.
inline constexpr float kTreeMaxSlope = 2.0f;   // per-axis central-difference height slope
inline constexpr float kTreeMinHeight = 1.5f;  // above sea level (0): no beach/water trees
inline constexpr float kTreeMaxHeight = 45.0f; // tree line

// Trunk half-widths per silhouette -- shared by the mesh emitter and the voxelizer so both draw
// the same trunk.
inline constexpr float kTrunkHalfWidthRound = 0.35f;
inline constexpr float kTrunkHalfWidthConifer = 0.25f;
// Trunks are sunk half a unit below the surface so nothing floats above a terraced column.
inline constexpr float kTrunkSink = 0.5f;

// All candidate trees for one chunk COLUMN (y ignored), after masking. Deterministic.
[[nodiscard]] std::vector<TreePlacement> compute_tree_placements(std::int32_t chunkX, std::int32_t chunkZ,
                                                                 int seed,
                                                                 const HeightmapGenerator& heightmap);

// One canopy octahedron with independent horizontal/vertical half-extents -- the single primitive
// every silhouette composes from. `center` is in world space.
struct CanopyLobe {
    glm::vec3 center{0.0f};
    float rh = 0.0f;
    float rv = 0.0f;
};

// The canopy lobes of a placement (1 for Round/Shrub, 3 for Conifer). Returns the count written;
// the array is sized for the largest silhouette.
inline constexpr std::size_t kMaxCanopyLobes = 3;
std::size_t tree_canopy_lobes(const TreePlacement& tree, std::array<CanopyLobe, kMaxCanopyLobes>& out);

// Trunk vertical extent [y0, y1) and half-width; Shrub has no trunk (returns false).
struct TrunkBox {
    float y0 = 0.0f;
    float y1 = 0.0f;
    float half_width = 0.0f;
};
bool tree_trunk(const TreePlacement& tree, TrunkBox& out);

// Conservative world-space bounding box of the whole tree (trunk + every lobe).
struct TreeBounds {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};
[[nodiscard]] TreeBounds tree_bounds(const TreePlacement& tree);

// The implicit-shape test at a world-space point: Wood inside the trunk (trunk wins where a lobe
// overlaps it), Leaves inside any canopy lobe, otherwise Air. Pure geometry -- no noise, so it is
// exactly consistent with the octahedra the mesh path pushes.
[[nodiscard]] world::chunk::MaterialID tree_material_at(const TreePlacement& tree, const glm::vec3& p);

// Exact box tests against the tree's convex parts (trunk box + octahedron lobes), for the sparse
// octree builder's box classification: `intersects` is false only when NO point of the box is
// inside any part (so a box outside every part can be classified without visiting its voxels);
// `contains` is true only when EVERY point of the box is inside one single lobe (convexity makes
// the 8 corners sufficient) -- a solid-leaves box.
[[nodiscard]] bool tree_intersects_box(const TreePlacement& tree, const glm::vec3& boxMin,
                                       const glm::vec3& boxMax);
[[nodiscard]] bool tree_lobe_contains_box(const TreePlacement& tree, const glm::vec3& boxMin,
                                          const glm::vec3& boxMax);

} // namespace world::generation
