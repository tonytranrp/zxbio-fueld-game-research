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
// Layout v2 (docs/goals.md Group Z, research/lin-look-log.md §2): internal nodes and brick leaves
// carry one ATTRIBUTE word after the header -- the node's area-weighted average surface normal
// (three int8 snorm components) and its volume coverage (uint8) -- so a hit can be shaded with a
// normal averaged over any ancestor's extent (the anti-moiré "smooth normal" at distance) and a
// LOD cube knows how solid it really is:
//
//   internal:   [header][attributes][child pointer per set mask bit ...]
//   brick leaf: [header][brick index][attributes]
//   solid leaf: [header]                    (a solid cube's normal is whichever face is hit)
//
// A child slot holds the word offset of the child's header inside the same node array. Air is not
// a node at all -- an unset mask bit. Octant bit layout: bit0 = +x half, bit1 = +y, bit2 = +z.
inline constexpr std::uint32_t kNodeKindInternal = 0u;
inline constexpr std::uint32_t kNodeKindBrick = 1u;
inline constexpr std::uint32_t kNodeKindSolid = 2u;
inline constexpr std::uint32_t kNoNode = 0xFFFFFFFFu; // "absent child" sentinel inside the builder only

// Word offsets relative to the header (mirrored by svo_march.psh.hlsl -- update both together).
inline constexpr std::uint32_t kNodeAttrSlotInternal = 1u;
inline constexpr std::uint32_t kNodeBrickIndexSlot = 1u;
inline constexpr std::uint32_t kNodeAttrSlotBrick = 2u;
inline constexpr std::uint32_t kNodeFirstChildSlot = 2u;
inline constexpr std::uint32_t kNodeWordsBrick = 3u;
inline constexpr std::uint32_t kNodeWordsSolid = 1u;

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

// LAYOUT v3 (Prompt 005 goal 278): the node's AREA-WEIGHTED AVERAGE ALBEDO, packed R4 G6 B4 into
// the header's previously unused bits 10-15 and 24-31.
//
// WHY THIS EXISTS. Goal 277 measured discrete sampling of the per-hit MATERIAL as 59% of the
// moire -- Laine & Karras' "blockiness caused by discrete sampling of shading attributes" -- and
// the obvious zero-storage fix, shading from the smoothing ancestor's REPRESENTATIVE material,
// was tried and REJECTED with a viewed capture: a representative is a majority vote, so it is a
// coarser QUANTISER, not a filter. It moved the metric from 3.127 to 1.795 while turning fine red
// speckle into large salmon blotches, and no ancestor span fixed it (2.41 / 1.95 / 1.80 / 2.04 at
// 2 / 4 / 6 / 12 px, non-monotonic). Only a genuine average is a filter.
//
// WHY IN THE HEADER RATHER THAN A SECOND ATTRIBUTE WORD. A second word costs 4 bytes on every
// internal node and brick leaf -- about 5.0 MB on the shipping tree. These 14 bits cost nothing,
// they sit beside the representative material they band-limit, and -- the part that matters most --
// SOLID LEAVES HAVE A HEADER AND NO ATTRIBUTE WORD, so this is the only place that reaches the
// 804,157 of them the prompt flags as the largest hole in the current filtering.
//
// WHY R4 G6 B4. This is a band-limited term by construction: it is an average over a node, so it
// carries no high-frequency content and 16/64/16 levels are far more than a low-frequency colour
// needs. Green gets the extra two bits because luminance is mostly green.
inline constexpr std::uint32_t kNodeAlbedoRShift = 24; // 4 bits
inline constexpr std::uint32_t kNodeAlbedoGShift = 10; // 6 bits
inline constexpr std::uint32_t kNodeAlbedoBShift = 28; // 4 bits

[[nodiscard]] constexpr std::uint32_t pack_node_albedo(float r, float g, float b) noexcept {
    const auto q = [](float v, std::uint32_t bits) {
        const float clamped = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        const auto maxv = static_cast<float>((1u << bits) - 1u);
        return static_cast<std::uint32_t>(clamped * maxv + 0.5f);
    };
    return (q(r, 4) << kNodeAlbedoRShift) | (q(g, 6) << kNodeAlbedoGShift) |
           (q(b, 4) << kNodeAlbedoBShift);
}

/// The three components, decoded to 0..1. A header written before this existed decodes to black,
/// which is why every consumer must gate on `node_has_albedo`.
[[nodiscard]] constexpr glm::vec3 node_albedo(std::uint32_t header) noexcept {
    return glm::vec3{static_cast<float>((header >> kNodeAlbedoRShift) & 0xFu) / 15.0f,
                     static_cast<float>((header >> kNodeAlbedoGShift) & 0x3Fu) / 63.0f,
                     static_cast<float>((header >> kNodeAlbedoBShift) & 0xFu) / 15.0f};
}

/// False for a node whose albedo bits are all zero -- either never written, or a genuinely black
/// node. Treating black as "absent" is deliberate and safe: this world has no black material, and
/// the fallback is the unfiltered albedo, which is what the renderer did before this word existed.
[[nodiscard]] constexpr bool node_has_albedo(std::uint32_t header) noexcept {
    return (header & ((0xFu << kNodeAlbedoRShift) | (0x3Fu << kNodeAlbedoGShift) |
                      (0xFu << kNodeAlbedoBShift))) != 0u;
}
// Word offset (relative to the header) of octant `octant`'s child pointer: pointers are packed in
// octant order behind the attribute word, so it is 2 + the number of present octants below it.
[[nodiscard]] constexpr std::uint32_t node_child_slot(std::uint32_t header, int octant) noexcept {
    const std::uint32_t below = node_child_mask(header) & ((1u << octant) - 1u);
    return kNodeFirstChildSlot + static_cast<std::uint32_t>(std::popcount(below));
}
// Words an internal node occupies: header + attributes + one pointer per present child.
[[nodiscard]] constexpr std::uint32_t node_words_internal(std::uint32_t header) noexcept {
    return kNodeFirstChildSlot + static_cast<std::uint32_t>(std::popcount(node_child_mask(header)));
}
// Offset of the attribute word for a header of the given kind; kNoNode for solid leaves.
[[nodiscard]] constexpr std::uint32_t node_attr_slot(std::uint32_t header) noexcept {
    const std::uint32_t kind = node_kind(header);
    return kind == kNodeKindInternal ? kNodeAttrSlotInternal
                                     : (kind == kNodeKindBrick ? kNodeAttrSlotBrick : kNoNode);
}

// The attribute word: bits 0..7 / 8..15 / 16..23 = normal x/y/z as int8 in [-127, 127] (snorm,
// decode = value / 127), bits 24..31 = coverage as uint8 (decode = value / 255). A zero normal
// (nothing exposed) decodes to the zero vector; consumers fall back to the face normal then.
[[nodiscard]] constexpr std::uint32_t pack_snorm8(float v) noexcept {
    const float c = v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
    const int i = static_cast<int>(c * 127.0f + (c >= 0.0f ? 0.5f : -0.5f));
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(static_cast<std::int8_t>(i)));
}
[[nodiscard]] constexpr float unpack_snorm8(std::uint32_t byte) noexcept {
    return static_cast<float>(static_cast<std::int8_t>(static_cast<std::uint8_t>(byte & 0xFFu))) / 127.0f;
}
[[nodiscard]] inline std::uint32_t make_node_attributes(const glm::vec3& normal, float coverage) noexcept {
    const float c = coverage < 0.0f ? 0.0f : (coverage > 1.0f ? 1.0f : coverage);
    const auto cov = static_cast<std::uint32_t>(std::lround(c * 255.0f));
    return pack_snorm8(normal.x) | (pack_snorm8(normal.y) << 8) | (pack_snorm8(normal.z) << 16) | (cov << 24);
}
[[nodiscard]] inline glm::vec3 node_attr_normal(std::uint32_t attr) noexcept {
    return glm::vec3{unpack_snorm8(attr), unpack_snorm8(attr >> 8), unpack_snorm8(attr >> 16)};
}
[[nodiscard]] constexpr float node_attr_coverage(std::uint32_t attr) noexcept {
    return static_cast<float>((attr >> 24) & 0xFFu) / 255.0f;
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
