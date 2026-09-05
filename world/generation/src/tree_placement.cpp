#include "world/generation/tree_placement.hpp"

#include <algorithm>
#include <cmath>

#include "world/chunk/chunk_voxels.hpp" // kChunkSize

namespace world::generation {

namespace {

using world::chunk::kChunkSize;
using world::chunk::MaterialID;

// splitmix64: a well-mixed, dependency-free hash for deterministic placement keys.
std::uint64_t mix(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

std::uint64_t placement_key(int seed, std::int32_t cx, std::int32_t cz, std::int32_t gx, std::int32_t gz) {
    std::uint64_t k = static_cast<std::uint32_t>(seed);
    k = mix(k ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32 |
                 static_cast<std::uint32_t>(cz)));
    k = mix(k ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(gx)) << 32 |
                 static_cast<std::uint32_t>(gz)));
    return k;
}

bool inside_lobe(const CanopyLobe& lobe, const glm::vec3& p) {
    const float dx = std::abs(p.x - lobe.center.x);
    const float dy = std::abs(p.y - lobe.center.y);
    const float dz = std::abs(p.z - lobe.center.z);
    // L1 "diamond" -- the octahedron's exact interior.
    return dx / lobe.rh + dy / lobe.rv + dz / lobe.rh <= 1.0f;
}

} // namespace

std::vector<TreePlacement> compute_tree_placements(std::int32_t chunkX, std::int32_t chunkZ, int seed,
                                                   const HeightmapGenerator& heightmap) {
    std::vector<TreePlacement> placements;
    constexpr std::int32_t kCell = 8; // 4x4 candidate grid per 32-voxel column
    for (std::int32_t gz = 0; gz < kChunkSize / kCell; ++gz) {
        for (std::int32_t gx = 0; gx < kChunkSize / kCell; ++gx) {
            const std::uint64_t key = placement_key(seed, chunkX, chunkZ, gx, gz);
            if ((key & 0xFFu) >= 96u) { // ~37% of cells host a candidate (pre-mask)
                continue;
            }
            // Jitter within [2, cell-2): with cell size 8, adjacent cells' trees are always
            // >= kTreeMinSpacing (4) apart -- spacing holds by construction, tested anyway.
            const auto jx = static_cast<float>(2 + ((key >> 8) & 0xFFu) % 5u);
            const auto jz = static_cast<float>(2 + ((key >> 16) & 0xFFu) % 5u);
            const float worldX = static_cast<float>(chunkX * kChunkSize + gx * kCell) + jx;
            const float worldZ = static_cast<float>(chunkZ * kChunkSize + gz * kCell) + jz;

            const float h = heightmap.height_at(worldX, worldZ);
            if (h < kTreeMinHeight || h > kTreeMaxHeight) {
                continue; // no water/beach trees, no trees above the tree line
            }
            const float slopeX = std::abs(heightmap.height_at(worldX + 1.0f, worldZ) -
                                          heightmap.height_at(worldX - 1.0f, worldZ)) *
                                 0.5f;
            const float slopeZ = std::abs(heightmap.height_at(worldX, worldZ + 1.0f) -
                                          heightmap.height_at(worldX, worldZ - 1.0f)) *
                                 0.5f;
            if (std::max(slopeX, slopeZ) > kTreeMaxSlope) {
                continue; // too steep
            }

            TreePlacement tree;
            tree.world_x = worldX;
            tree.world_z = worldZ;
            tree.base_height = h;
            tree.trunk_height = 4.0f + static_cast<float>((key >> 24) & 0x3u);  // 4..7
            tree.canopy_radius = 2.0f + static_cast<float>((key >> 26) & 0x1u); // 2..3
            // Goal 36: silhouette selector from spare key bits (~3/8 round, ~3/8 conifer,
            // ~1/4 shrub); goal 38: brightness jitter in [0.80, 1.0] from another byte.
            const std::uint64_t shapeSel = (key >> 32) & 0xFFu;
            tree.shape = shapeSel < 96u    ? TreeShape::Round
                         : shapeSel < 192u ? TreeShape::Conifer
                                           : TreeShape::Shrub;
            tree.color_jitter = 0.80f + 0.20f * static_cast<float>((key >> 40) & 0xFFu) / 255.0f;
            if (tree.shape == TreeShape::Conifer) {
                tree.trunk_height += 2.0f; // taller...
                tree.canopy_radius = 2.0f; // ...and consistently narrow
            } else if (tree.shape == TreeShape::Shrub) {
                tree.trunk_height = 0.0f;
                tree.canopy_radius = 1.5f;
            }
            placements.push_back(tree);
        }
    }
    return placements;
}

bool tree_trunk(const TreePlacement& tree, TrunkBox& out) {
    if (tree.shape == TreeShape::Shrub) {
        return false;
    }
    out.y0 = tree.base_height - kTrunkSink;
    out.y1 = tree.base_height + tree.trunk_height;
    out.half_width = tree.shape == TreeShape::Conifer ? kTrunkHalfWidthConifer : kTrunkHalfWidthRound;
    return true;
}

std::size_t tree_canopy_lobes(const TreePlacement& tree, std::array<CanopyLobe, kMaxCanopyLobes>& out) {
    const float x = tree.world_x;
    const float z = tree.world_z;
    const float y0 = tree.base_height - kTrunkSink;
    const float y1 = tree.base_height + tree.trunk_height;
    const float r = tree.canopy_radius;
    switch (tree.shape) {
    case TreeShape::Round:
        out[0] = CanopyLobe{{x, y1 + r, z}, r, r};
        return 1;
    case TreeShape::Conifer:
        // Three stacked shrinking octahedra overlapping into a fir silhouette (the exact
        // proportions app's mesh emitter has always used).
        out[0] = CanopyLobe{{x, y1 - 0.5f * r, z}, r, r * 1.1f};
        out[1] = CanopyLobe{{x, y1 + 0.7f * r, z}, r * 0.72f, r * 0.95f};
        out[2] = CanopyLobe{{x, y1 + 1.7f * r, z}, r * 0.45f, r * 0.85f};
        return 3;
    case TreeShape::Shrub:
        // No trunk: one squashed octahedron sitting on (slightly into) the ground.
        out[0] = CanopyLobe{{x, y0 + 0.6f * r, z}, r, r * 0.7f};
        return 1;
    }
    return 0;
}

TreeBounds tree_bounds(const TreePlacement& tree) {
    TreeBounds b;
    b.min = glm::vec3{tree.world_x, tree.base_height - kTrunkSink, tree.world_z};
    b.max = b.min;
    TrunkBox trunk;
    if (tree_trunk(tree, trunk)) {
        b.min = glm::min(
            b.min, glm::vec3{tree.world_x - trunk.half_width, trunk.y0, tree.world_z - trunk.half_width});
        b.max = glm::max(
            b.max, glm::vec3{tree.world_x + trunk.half_width, trunk.y1, tree.world_z + trunk.half_width});
    }
    std::array<CanopyLobe, kMaxCanopyLobes> lobes{};
    const std::size_t count = tree_canopy_lobes(tree, lobes);
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3 ext{lobes[i].rh, lobes[i].rv, lobes[i].rh};
        b.min = glm::min(b.min, lobes[i].center - ext);
        b.max = glm::max(b.max, lobes[i].center + ext);
    }
    return b;
}

bool tree_intersects_box(const TreePlacement& tree, const glm::vec3& boxMin, const glm::vec3& boxMax) {
    TrunkBox trunk;
    if (tree_trunk(tree, trunk)) {
        const bool trunkHit =
            boxMax.x > tree.world_x - trunk.half_width && boxMin.x < tree.world_x + trunk.half_width &&
            boxMax.z > tree.world_z - trunk.half_width && boxMin.z < tree.world_z + trunk.half_width &&
            boxMax.y > trunk.y0 && boxMin.y < trunk.y1;
        if (trunkHit) {
            return true;
        }
    }
    std::array<CanopyLobe, kMaxCanopyLobes> lobes{};
    const std::size_t count = tree_canopy_lobes(tree, lobes);
    for (std::size_t i = 0; i < count; ++i) {
        // The lobe's weighted-L1 "diamond" norm is separable, so the box point nearest the center
        // in that norm is the per-axis clamp -- an exact convex test, not a bounding-box one.
        const glm::vec3 nearest = glm::clamp(lobes[i].center, boxMin, boxMax);
        const float d = std::abs(nearest.x - lobes[i].center.x) / lobes[i].rh +
                        std::abs(nearest.y - lobes[i].center.y) / lobes[i].rv +
                        std::abs(nearest.z - lobes[i].center.z) / lobes[i].rh;
        if (d <= 1.0f) {
            return true;
        }
    }
    return false;
}

bool tree_lobe_contains_box(const TreePlacement& tree, const glm::vec3& boxMin, const glm::vec3& boxMax) {
    std::array<CanopyLobe, kMaxCanopyLobes> lobes{};
    const std::size_t count = tree_canopy_lobes(tree, lobes);
    for (std::size_t i = 0; i < count; ++i) {
        bool all = true;
        for (int corner = 0; corner < 8 && all; ++corner) {
            const glm::vec3 p{(corner & 1) != 0 ? boxMax.x : boxMin.x,
                              (corner & 2) != 0 ? boxMax.y : boxMin.y,
                              (corner & 4) != 0 ? boxMax.z : boxMin.z};
            all = inside_lobe(lobes[i], p);
        }
        if (all) {
            return true;
        }
    }
    return false;
}

MaterialID tree_material_at(const TreePlacement& tree, const glm::vec3& p) {
    TrunkBox trunk;
    if (tree_trunk(tree, trunk) && p.y >= trunk.y0 && p.y < trunk.y1 &&
        std::abs(p.x - tree.world_x) <= trunk.half_width &&
        std::abs(p.z - tree.world_z) <= trunk.half_width) {
        return MaterialID::Wood;
    }
    std::array<CanopyLobe, kMaxCanopyLobes> lobes{};
    const std::size_t count = tree_canopy_lobes(tree, lobes);
    for (std::size_t i = 0; i < count; ++i) {
        if (inside_lobe(lobes[i], p)) {
            return MaterialID::Leaves;
        }
    }
    return MaterialID::Air;
}

} // namespace world::generation
