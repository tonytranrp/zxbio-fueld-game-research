#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/generation/tree_placement.hpp"
#include "world/generation/tree_skeleton.hpp"

namespace world::generation {

// The grown skeleton as a SOLID (Prompt 007 goal 336 = docs/goals.md goal 191): capsule branches and
// per-segment leaf clouds, answering the same three questions `tree_placement.hpp`'s implicit
// box-plus-octahedron answers, so `TerrainSampler` can swap one for the other.
//
//   material_at(p)         -- Wood, Leaves or Air at a point
//   intersects_box(lo, hi) -- false ONLY when no point of the box is inside anything
//   contains_box(lo, hi)   -- true ONLY when the box is definitely full (false negatives allowed)
//
// That contract is copied deliberately: the sparse-octree builder's box classification depends on
// the asymmetry, and a volume that got it backwards would produce holes rather than a slow build.
//
// ---------------------------------------------------------------------------------------------
// THE COST PROBLEM, AND THE TWO THINGS THAT MAKE IT AFFORDABLE
// ---------------------------------------------------------------------------------------------
//
// A placement is one box and up to three octahedra. A skeleton is ~160 segments. Tested naively
// that is a 160x more expensive point query, at 7.8 mm voxels, over a canopy -- and
// `research/tree-motion-growth-and-appearance.md` and the micro-voxel research agree that vegetation
// is the worst-case content class for a sparse octree. Two things bound it:
//
// 1. A UNIFORM GRID over the tree's own bounds. Cells are sized to the largest primitive so the
//    candidate list per cell stays short; a point query then touches a handful of segments rather
//    than all of them. Built once per tree, ~10 KB, thrown away with the build.
//
// 2. ONLY THE NEAR TREES GET ONE. Beyond `TerrainSamplerParams::skeleton_radius_m` the implicit
//    shape is still used, and the criterion for that radius is not taste -- it is the LOD ladder.
//    The voxel edge at distance d is `max(finest, d * finest / lod_radius)`, so a branch of radius r
//    stops being representable at all beyond `d = lod_radius * 2r / finest`: 10 m for a 1 cm twig,
//    26 m for a 5 cm limb, 41 m for an 8 cm trunk. Past that the skeleton and the octahedron
//    voxelize to the same blob because neither has any detail the grid can hold. The default is
//    larger than 41 m only because the octree is built once and the camera keeps walking.

struct TreeVolumeParams {
    // Leaf area density, m^2 of leaf per m^3 of crown -- what turns a segment's leaf AREA (which the
    // pipe model gives, goal 188) into a leaf-cloud VOLUME, and thence a radius.
    //
    // DERIVED, NOT CHOSEN, and the first version's fixed 2.0 is why. A crown of 45 m^2 of leaf over a
    // 3 m radius canopy occupies about 113 m^3, so its real density is 0.4 -- packing the same leaf
    // at 2.0 puts it in a fifth of the volume, and the capture showed exactly that: a dozen separate
    // spheres on a stick instead of a canopy. The density used is therefore
    // `total leaf area / crown volume`, measured from the skeleton's own leaf-bearing extent, so the
    // clouds fill the crown the tree actually grew. These two clamp it into the band real canopies
    // measure in, for the degenerate skeletons (a shrub of three segments) where the extent is not
    // a crown.
    float min_leaf_area_density = 0.15f;
    float max_leaf_area_density = 5.0f;
    // Clouds smaller than this are not worth a primitive -- they are below one coarse voxel and
    // their leaf area is folded into their parent's cloud instead.
    float min_leaf_radius = 0.12f;
    float max_leaf_radius = 1.20f;
    // Branches thinner than this are drawn at this thickness. A 1 cm twig is a real twig, but a
    // capsule that thin at a 7.8 mm voxel grid is a dotted line, and a dotted branch reads as an
    // artefact rather than as a fine one.
    float min_branch_radius = 0.02f;
};

// One primitive: a capsule when `leaf` is false (a branch), a ball around `b` when it is true.
struct TreePrimitive {
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    float radius = 0.0f;
    bool leaf = false;
};

class TreeVolume {
public:
    TreeVolume() = default;
    TreeVolume(const TreeSkeleton& skeleton, const TreeVolumeParams& params);

    [[nodiscard]] bool empty() const noexcept { return primitives_.empty(); }
    [[nodiscard]] const TreeBounds& bounds() const noexcept { return bounds_; }
    [[nodiscard]] const std::vector<TreePrimitive>& primitives() const noexcept { return primitives_; }

    [[nodiscard]] world::chunk::MaterialID material_at(const glm::vec3& p) const;
    [[nodiscard]] bool intersects_box(const glm::vec3& lo, const glm::vec3& hi) const;
    [[nodiscard]] bool contains_box(const glm::vec3& lo, const glm::vec3& hi) const;

    // Bytes this volume occupies, for the memory accounting goal 191's Check asks for.
    [[nodiscard]] std::size_t memory_bytes() const noexcept;

private:
    // Candidate primitives whose bounds touch `[lo, hi]`, appended to `out`.
    void gather(const glm::vec3& lo, const glm::vec3& hi, std::vector<std::uint32_t>& out) const;

    std::vector<TreePrimitive> primitives_;
    TreeBounds bounds_{};

    glm::vec3 gridMin_{0.0f};
    float cellSize_ = 1.0f;
    glm::ivec3 dims_{1};
    std::vector<std::uint32_t> cellStart_; // dims product + 1, prefix-summed
    std::vector<std::uint32_t> cellItems_;
};

// Grow the skeleton a placement implies and turn it into a volume, in one call -- the form
// `TerrainSampler` wants.
[[nodiscard]] TreeVolume tree_volume_for(int seed, const TreePlacement& tree,
                                         const TreeVolumeParams& params = {});

} // namespace world::generation
