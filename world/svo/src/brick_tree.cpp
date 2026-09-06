#include "world/svo/brick_tree.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>
#include <vector>

namespace world::svo {

world::chunk::MaterialID BrickTree::material_at(const glm::vec3& worldPos) const noexcept {
    using world::chunk::MaterialID;
    if (empty() || !geometry.contains(worldPos)) {
        return MaterialID::Air;
    }
    const int V = geometry.voxel_bits();
    const float cellsPerAxis = std::ldexp(1.0f, V);
    const glm::vec3 local = (worldPos - geometry.origin) / geometry.root_edge();
    const auto toCell = [&](float v) {
        const float f = std::floor(v * cellsPerAxis);
        return static_cast<std::int32_t>(std::clamp(f, 0.0f, cellsPerAxis - 1.0f));
    };
    const std::int32_t cx = toCell(local.x);
    const std::int32_t cy = toCell(local.y);
    const std::int32_t cz = toCell(local.z);

    std::uint32_t node = root;
    int level = 0;
    for (;;) {
        const std::uint32_t header = nodes[node];
        const std::uint32_t kind = node_kind(header);
        if (kind == kNodeKindSolid) {
            return node_material(header);
        }
        if (kind == kNodeKindBrick) {
            const int shift = V - level - TreeGeometry::kBrickLog2;
            const std::size_t index =
                brick_voxel_index((cx >> shift) & 7, (cy >> shift) & 7, (cz >> shift) & 7);
            return brick_word_material(brick_words(nodes[node + kNodeBrickIndexSlot]), index);
        }
        const int shift = V - level - 1;
        const int octant = octant_of(cx >> shift, cy >> shift, cz >> shift);
        if ((node_child_mask(header) & (1u << octant)) == 0u) {
            return MaterialID::Air;
        }
        node = nodes[node + node_child_slot(header, octant)];
        ++level;
    }
}

int BrickTree::leaf_level_at(const glm::vec3& worldPos) const noexcept {
    if (empty() || !geometry.contains(worldPos)) {
        return -1;
    }
    const int V = geometry.voxel_bits();
    const float cellsPerAxis = std::ldexp(1.0f, V);
    const glm::vec3 local = (worldPos - geometry.origin) / geometry.root_edge();
    const auto toCell = [&](float v) {
        const float f = std::floor(v * cellsPerAxis);
        return static_cast<std::int32_t>(std::clamp(f, 0.0f, cellsPerAxis - 1.0f));
    };
    const std::int32_t cx = toCell(local.x);
    const std::int32_t cy = toCell(local.y);
    const std::int32_t cz = toCell(local.z);
    std::uint32_t node = root;
    int level = 0;
    for (;;) {
        const std::uint32_t header = nodes[node];
        if (node_kind(header) != kNodeKindInternal) {
            return level;
        }
        const int shift = V - level - 1;
        const int octant = octant_of(cx >> shift, cy >> shift, cz >> shift);
        if ((node_child_mask(header) & (1u << octant)) == 0u) {
            return level;
        }
        node = nodes[node + node_child_slot(header, octant)];
        ++level;
    }
}

std::uint32_t BrickTree::attributes_at(const glm::vec3& worldPos, int wantedLevel) const noexcept {
    if (empty() || !geometry.contains(worldPos) || wantedLevel < 0) {
        return 0u;
    }
    const int V = geometry.voxel_bits();
    const float cellsPerAxis = std::ldexp(1.0f, V);
    const glm::vec3 local = (worldPos - geometry.origin) / geometry.root_edge();
    const auto toCell = [&](float v) {
        const float f = std::floor(v * cellsPerAxis);
        return static_cast<std::int32_t>(std::clamp(f, 0.0f, cellsPerAxis - 1.0f));
    };
    const std::int32_t cx = toCell(local.x);
    const std::int32_t cy = toCell(local.y);
    const std::int32_t cz = toCell(local.z);
    std::uint32_t node = root;
    int level = 0;
    for (;;) {
        const std::uint32_t header = nodes[node];
        if (level == wantedLevel || node_kind(header) != kNodeKindInternal) {
            const std::uint32_t slot = node_attr_slot(header);
            return slot == kNoNode ? 0u : nodes[node + slot];
        }
        const int shift = V - level - 1;
        const int octant = octant_of(cx >> shift, cy >> shift, cz >> shift);
        if ((node_child_mask(header) & (1u << octant)) == 0u) {
            return 0u;
        }
        node = nodes[node + node_child_slot(header, octant)];
        ++level;
    }
}

BrickTree::Stats BrickTree::stats() const {
    Stats s;
    if (empty()) {
        return s;
    }
    std::size_t usedWords = 0;
    std::vector<std::pair<std::uint32_t, int>> stack;
    stack.emplace_back(root, 0);
    while (!stack.empty()) {
        const auto [node, level] = stack.back();
        stack.pop_back();
        s.deepest_level = std::max(s.deepest_level, level);
        const std::uint32_t header = nodes[node];
        switch (node_kind(header)) {
        case kNodeKindSolid:
            ++s.solid_leaves;
            usedWords += kNodeWordsSolid;
            break;
        case kNodeKindBrick:
            ++s.brick_leaves;
            ++s.bricks_per_level[static_cast<std::size_t>(level)];
            usedWords += kNodeWordsBrick;
            break;
        default: {
            ++s.internal_nodes;
            const std::uint32_t mask = node_child_mask(header);
            usedWords += node_words_internal(header);
            for (int octant = 0; octant < 8; ++octant) {
                if ((mask & (1u << octant)) != 0u) {
                    stack.emplace_back(nodes[node + node_child_slot(header, octant)], level + 1);
                }
            }
            break;
        }
        }
    }
    s.padding_words = nodes.size() - usedWords;
    return s;
}

} // namespace world::svo
