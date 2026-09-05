#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "world/chunk/material.hpp"
#include "world/svo/brick.hpp"
#include "world/svo/tree_layout.hpp"

namespace world::svo {

// The built sparse-brick octree: two flat uint32 arrays (nodes in the tree_layout.hpp encoding,
// bricks as consecutive 144-word blocks) plus the geometry that maps world space onto them. This
// IS the GPU representation -- SvoRenderer uploads `nodes` and `bricks` verbatim into two
// StructuredBuffer<uint>s, and the CPU reference marcher (ray_trace.hpp) walks these same words,
// which is what makes "the shader mirrors the reference" a testable claim rather than a hope.
// Immutable after construction by design (research/micro-voxel-pivot-log.md §2.6: the tree is
// rebuilt from the analytic sampler when the camera moves; live editing is the HashDAG-shaped
// follow-up, not this structure's job).
class BrickTree {
public:
    TreeGeometry geometry;
    std::vector<std::uint32_t> nodes;
    std::vector<std::uint32_t> bricks; // kBrickWords per brick
    std::uint32_t root = 0;            // word offset of the root header; meaningless when empty()

    [[nodiscard]] bool empty() const noexcept { return nodes.empty(); }
    [[nodiscard]] std::size_t brick_count() const noexcept { return bricks.size() / kBrickWords; }
    [[nodiscard]] std::size_t node_words() const noexcept { return nodes.size(); }
    [[nodiscard]] std::size_t memory_bytes() const noexcept {
        return (nodes.size() + bricks.size()) * sizeof(std::uint32_t);
    }
    [[nodiscard]] const std::uint32_t* brick_words(std::uint32_t brickIndex) const noexcept {
        return bricks.data() + static_cast<std::size_t>(brickIndex) * kBrickWords;
    }

    // Point query at whatever resolution the tree holds there: the brick voxel containing the
    // point, or the solid leaf's material, or Air where no node exists. Exact for a uniform-LOD
    // tree (the dense-comparison tests), the coarse answer for a distance-LOD one.
    [[nodiscard]] world::chunk::MaterialID material_at(const glm::vec3& worldPos) const noexcept;

    // The level of the node that answers material_at for this point (a brick or solid leaf's
    // level; the level of the internal node whose missing child covers the point for Air; -1
    // outside the tree). Tests use it to check where distance-based LOD stopped subdividing.
    [[nodiscard]] int leaf_level_at(const glm::vec3& worldPos) const noexcept;

    struct Stats {
        std::size_t internal_nodes = 0;
        std::size_t brick_leaves = 0;
        std::size_t solid_leaves = 0;
        std::size_t padding_words = 0; // reserved-but-unused child slots (see tree_builder.hpp)
        int deepest_level = -1;
        std::array<std::size_t, kMaxLevels> bricks_per_level{}; // where the memory actually goes
    };
    [[nodiscard]] Stats stats() const;
};

} // namespace world::svo
