#pragma once

#include <bit>
#include <cmath>
#include <cstdint>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"

namespace world::svo {

// Flat node encoding of the sparse-brick octree (research/micro-voxel-pivot-log.md §2.3): the
// SVDAG-paper layout (Kämpe, Sintorn & Assarsson 2013, §3 of the research brief) -- one 32-bit
// header word followed by one 32-bit child pointer per set child-mask bit, stored consecutively --
// extended with a node KIND and a representative MATERIAL in the header's otherwise-unused bits:
//
//   bits  0..7  child mask (internal nodes): octant i has a child entry
//   bits  8..9  kind: 0 = internal, 1 = brick leaf (one payload word: brick index), 2 = solid leaf
//   bits 16..23 material: the solid leaf's material, or the representative material an LOD
//               early-out shades an internal node / brick with (never Air for a present node)
//
// A child slot holds the word offset of the child's header inside the same node array. Air is not
// a node at all -- an unset mask bit. Octant bit layout: bit0 = +x half, bit1 = +y, bit2 = +z.
inline constexpr std::uint32_t kNodeKindInternal = 0u;
inline constexpr std::uint32_t kNodeKindBrick = 1u;
inline constexpr std::uint32_t kNodeKindSolid = 2u;
inline constexpr std::uint32_t kNoNode = 0xFFFFFFFFu; // "absent child" sentinel inside the builder only

[[nodiscard]] constexpr std::uint32_t make_node_header(std::uint32_t kind, std::uint32_t childMask,
                                                       world::chunk::MaterialID material) noexcept {
    return (childMask & 0xFFu) | ((kind & 3u) << 8) | (static_cast<std::uint32_t>(material) << 16);
}
[[nodiscard]] constexpr std::uint32_t node_child_mask(std::uint32_t header) noexcept {
    return header & 0xFFu;
}
[[nodiscard]] constexpr std::uint32_t node_kind(std::uint32_t header) noexcept {
    return (header >> 8) & 3u;
}
[[nodiscard]] constexpr world::chunk::MaterialID node_material(std::uint32_t header) noexcept {
    return static_cast<world::chunk::MaterialID>((header >> 16) & 0xFFu);
}
// Word offset (relative to the header) of octant `octant`'s child pointer: pointers are packed in
// octant order, so it is 1 + the number of present octants below it.
[[nodiscard]] constexpr std::uint32_t node_child_slot(std::uint32_t header, int octant) noexcept {
    const std::uint32_t below = node_child_mask(header) & ((1u << octant) - 1u);
    return 1u + static_cast<std::uint32_t>(std::popcount(below));
}
[[nodiscard]] constexpr int octant_of(int cx, int cy, int cz) noexcept {
    return (cx & 1) | ((cy & 1) << 1) | ((cz & 1) << 2);
}

// Where the tree sits in the world and how finely it is subdivided. Everything is a power of two
// so cell boundaries are exact in float and integer voxel coordinates are plain bit fields:
//   root edge            = 2^root_size_log2 meters
//   finest voxel edge    = 2^voxel_size_log2 meters (negative exponent = sub-meter; -7 = 7.8mm)
//   level L node edge    = root edge / 2^L;  a level-L brick's voxel edge = node edge / 8
//   finest brick level   = root_size_log2 - voxel_size_log2 - 3
//   voxel_bits (V)       = root_size_log2 - voxel_size_log2: integer voxel coordinates span [0, 2^V)
struct TreeGeometry {
    glm::vec3 origin{0.0f};   // world-space min corner (meters)
    int root_size_log2 = 9;   // 512 m
    int voxel_size_log2 = -7; // 1/128 m

    [[nodiscard]] int voxel_bits() const noexcept { return root_size_log2 - voxel_size_log2; }
    [[nodiscard]] int max_brick_level() const noexcept { return voxel_bits() - 3; }
    [[nodiscard]] float root_edge() const noexcept { return std::ldexp(1.0f, root_size_log2); }
    [[nodiscard]] float finest_voxel_edge() const noexcept { return std::ldexp(1.0f, voxel_size_log2); }
    [[nodiscard]] float level_edge(int level) const noexcept {
        return std::ldexp(1.0f, root_size_log2 - level);
    }
    [[nodiscard]] float level_voxel_edge(int level) const noexcept {
        return std::ldexp(1.0f, root_size_log2 - level - kBrickLog2);
    }
    [[nodiscard]] glm::vec3 max_corner() const noexcept { return origin + glm::vec3{root_edge()}; }
    [[nodiscard]] bool contains(const glm::vec3& p) const noexcept {
        const glm::vec3 m = max_corner();
        return p.x >= origin.x && p.y >= origin.y && p.z >= origin.z && p.x < m.x && p.y < m.y && p.z < m.z;
    }

    static constexpr int kBrickLog2 = 3;
};

// Float has a 24-bit mantissa: integer voxel coordinates and root-normalized positions stay exact
// only while V <= 24, and the traversal's fixed stack is sized from this too.
inline constexpr int kMaxVoxelBits = 24;
inline constexpr int kMaxLevels = kMaxVoxelBits - 3 + 1; // node levels 0..max_brick_level

} // namespace world::svo
