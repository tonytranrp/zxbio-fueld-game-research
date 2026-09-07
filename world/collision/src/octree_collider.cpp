#include "world/collision/octree_collider.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "world/materials/materials.hpp"
#include "world/svo/brick.hpp"
#include "world/svo/tree_layout.hpp"

namespace world::collision {

namespace {

using world::svo::BrickTree;
using world::svo::kBrickEdge;
using world::svo::TreeGeometry;

// An integer voxel-coordinate box, half-open [lo, hi). Everything below works in these rather than
// in metres: TreeGeometry's edges are all powers of two, so voxel coordinates are exact integers
// and every node boundary is representable -- which is the whole reason the traversal can use
// plain shifts instead of a float epsilon.
struct VoxelBox {
    std::int64_t lo[3]{};
    std::int64_t hi[3]{}; // exclusive

    [[nodiscard]] bool empty() const noexcept { return lo[0] >= hi[0] || lo[1] >= hi[1] || lo[2] >= hi[2]; }
};

[[nodiscard]] bool material_is_solid(world::chunk::MaterialID id) noexcept {
    // world/materials answers, not an ID comparison (Prompt 003 §3 rule 6). Water is is_occupied()
    // but NOT is_solid(), which is exactly why the swimmer swims instead of standing on the sea.
    return world::materials::properties_of(id).is_solid();
}

// Voxel-space box for a world-space AABB, clamped to the tree. Uses floor/ceil rather than a
// rounded centre: a box that grazes a voxel boundary must include that voxel, because the sweep
// asks "would the body be inside anything HERE", and answering "no" for a body flush against a
// wall is exactly the escape the previous pass had to fix with a 1 mm skin.
[[nodiscard]] VoxelBox to_voxel_box(const TreeGeometry& g, const Aabb& box) noexcept {
    const float inv = 1.0f / g.finest_voxel_edge();
    const std::int64_t span = std::int64_t{1} << g.voxel_bits();
    VoxelBox out;
    for (int a = 0; a < 3; ++a) {
        const float lo = (box.min[a] - g.origin[a]) * inv;
        const float hi = (box.max[a] - g.origin[a]) * inv;
        out.lo[a] = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor(lo)));
        out.hi[a] = std::min<std::int64_t>(span, static_cast<std::int64_t>(std::ceil(hi)));
    }
    return out;
}

// The recursive descent. `node` is a word offset; the node covers voxel cube
// [origin, origin + size)^3 where size = 1 << (V - level). Returns true on the first solid voxel
// that overlaps `box` -- an O(depth) walk with early-out, not an n^3 point sample.
bool node_overlaps(const BrickTree& tree, std::uint32_t node, int level, const std::int64_t origin[3],
                   const VoxelBox& box, int V, std::size_t& visited) {
    ++visited;
    const std::uint32_t header = tree.nodes[node];
    const std::uint32_t kind = world::svo::node_kind(header);

    if (kind == world::svo::kNodeKindSolid) {
        // The caller only recursed here because this node's cube intersects the box, so a solid
        // leaf of a solid material is an immediate hit.
        return material_is_solid(world::svo::node_material(header));
    }

    if (kind == world::svo::kNodeKindBrick) {
        // 8^3 voxels, each of edge 1 << (V - level - 3) in voxel units. Iterate only the sub-range
        // the box actually touches.
        const int shift = V - level - TreeGeometry::kBrickLog2;
        const std::int64_t cell = std::int64_t{1} << shift;
        const std::uint32_t* words = tree.brick_words(tree.nodes[node + world::svo::kNodeBrickIndexSlot]);
        std::int64_t begin[3];
        std::int64_t end[3];
        for (int a = 0; a < 3; ++a) {
            const std::int64_t relLo = box.lo[a] - origin[a];
            const std::int64_t relHi = box.hi[a] - origin[a];
            begin[a] = std::clamp<std::int64_t>(relLo >= 0 ? relLo / cell : 0, 0, kBrickEdge - 1);
            // relHi is exclusive: the last touched cell is (relHi - 1) / cell.
            const std::int64_t lastTouched = (relHi - 1) / cell;
            end[a] = std::clamp<std::int64_t>(lastTouched, -1, kBrickEdge - 1);
        }
        for (std::int64_t z = begin[2]; z <= end[2]; ++z) {
            for (std::int64_t y = begin[1]; y <= end[1]; ++y) {
                for (std::int64_t x = begin[0]; x <= end[0]; ++x) {
                    const std::size_t index = world::svo::brick_voxel_index(
                        static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
                    if (!world::svo::brick_word_occupied(words, index)) {
                        continue; // the mask alone answers "anything here?" -- 64 bytes, not 512
                    }
                    if (material_is_solid(world::svo::brick_word_material(words, index))) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Internal: recurse into the children whose octant cube the box touches AND whose mask bit is
    // set. An absent child is Air, not a hole to check.
    const std::uint32_t mask = world::svo::node_child_mask(header);
    if (mask == 0u) {
        return false;
    }
    const std::int64_t half = std::int64_t{1} << (V - level - 1);
    for (int octant = 0; octant < 8; ++octant) {
        if ((mask & (1u << octant)) == 0u) {
            continue;
        }
        std::int64_t childOrigin[3];
        bool touches = true;
        for (int a = 0; a < 3; ++a) {
            const int bit = (octant >> a) & 1;
            childOrigin[a] = origin[a] + (bit != 0 ? half : 0);
            if (box.hi[a] <= childOrigin[a] || box.lo[a] >= childOrigin[a] + half) {
                touches = false;
                break;
            }
        }
        if (!touches) {
            continue;
        }
        if (node_overlaps(tree, tree.nodes[node + world::svo::node_child_slot(header, octant)], level + 1,
                          childOrigin, box, V, visited)) {
            return true;
        }
    }
    return false;
}

// The highest solid voxel Y (in voxel units, the voxel's own index) in one column at or below
// `yLimit`, or a sentinel when there is none. Descends children in +y-first order and takes the
// first hit, which is why it is O(depth) rather than a scan of the column.
constexpr std::int64_t kNoVoxel = std::numeric_limits<std::int64_t>::min();

std::int64_t column_top(const BrickTree& tree, std::uint32_t node, int level, const std::int64_t origin[3],
                        std::int64_t cx, std::int64_t cz, std::int64_t yLimit, int V) {
    const std::uint32_t header = tree.nodes[node];
    const std::uint32_t kind = world::svo::node_kind(header);
    const std::int64_t size = std::int64_t{1} << (V - level);

    if (kind == world::svo::kNodeKindSolid) {
        if (!material_is_solid(world::svo::node_material(header))) {
            return kNoVoxel;
        }
        const std::int64_t top = origin[1] + size - 1;
        return top <= yLimit ? top : (origin[1] <= yLimit ? yLimit : kNoVoxel);
    }

    if (kind == world::svo::kNodeKindBrick) {
        const int shift = V - level - TreeGeometry::kBrickLog2;
        const std::int64_t cell = std::int64_t{1} << shift;
        const std::uint32_t* words = tree.brick_words(tree.nodes[node + world::svo::kNodeBrickIndexSlot]);
        const int bx = static_cast<int>((cx - origin[0]) / cell);
        const int bz = static_cast<int>((cz - origin[2]) / cell);
        for (int by = kBrickEdge - 1; by >= 0; --by) {
            const std::int64_t voxelTop = origin[1] + (by + 1) * cell - 1;
            if (voxelTop > yLimit) {
                continue;
            }
            const std::size_t index = world::svo::brick_voxel_index(bx, by, bz);
            if (world::svo::brick_word_occupied(words, index) &&
                material_is_solid(world::svo::brick_word_material(words, index))) {
                return voxelTop;
            }
        }
        return kNoVoxel;
    }

    const std::uint32_t mask = world::svo::node_child_mask(header);
    const std::int64_t half = size / 2;
    // +y octants first: the first hit descending from the top IS the top.
    for (int yBit = 1; yBit >= 0; --yBit) {
        const int xBit = cx >= origin[0] + half ? 1 : 0;
        const int zBit = cz >= origin[2] + half ? 1 : 0;
        const int octant = xBit | (yBit << 1) | (zBit << 2);
        if ((mask & (1u << octant)) == 0u) {
            continue;
        }
        const std::int64_t childOrigin[3] = {origin[0] + (xBit != 0 ? half : 0),
                                             origin[1] + (yBit != 0 ? half : 0),
                                             origin[2] + (zBit != 0 ? half : 0)};
        if (childOrigin[1] > yLimit) {
            continue; // wholly above the limit
        }
        const std::int64_t hit =
            column_top(tree, tree.nodes[node + world::svo::node_child_slot(header, octant)], level + 1,
                       childOrigin, cx, cz, yLimit, V);
        if (hit != kNoVoxel) {
            return hit;
        }
    }
    return kNoVoxel;
}

} // namespace

bool OctreeCollider::overlaps_solid(const Aabb& box) const noexcept {
    lastNodesVisited_ = 0;
    ++queryCount_;
    if (tree_ == nullptr || tree_->empty()) {
        return false;
    }
    const VoxelBox vb = to_voxel_box(tree_->geometry, box);
    if (vb.empty()) {
        return false; // wholly outside the tree
    }
    const std::int64_t origin[3]{0, 0, 0};
    const bool hit =
        node_overlaps(*tree_, tree_->root, 0, origin, vb, tree_->geometry.voxel_bits(), lastNodesVisited_);
    nodeVisitTotal_ += lastNodesVisited_;
    return hit;
}

float OctreeCollider::voxel_top(float x, float z, float yStart) const noexcept {
    if (tree_ == nullptr || tree_->empty()) {
        return -std::numeric_limits<float>::infinity();
    }
    const TreeGeometry& g = tree_->geometry;
    const float edge = g.finest_voxel_edge();
    const std::int64_t span = std::int64_t{1} << g.voxel_bits();
    const auto cell = [&](float world, float originAxis) {
        const auto v = static_cast<std::int64_t>(std::floor((world - originAxis) / edge));
        return std::clamp<std::int64_t>(v, 0, span - 1);
    };
    const std::int64_t cx = cell(x, g.origin.x);
    const std::int64_t cz = cell(z, g.origin.z);
    // The limit is the voxel containing yStart: a body standing ON a surface must find the voxel
    // under its feet, not the one its feet are flush with the top of.
    const std::int64_t yLimit = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(std::floor((yStart - g.origin.y) / edge)), -1, span - 1);
    if (yLimit < 0) {
        return -std::numeric_limits<float>::infinity();
    }
    const std::int64_t origin[3]{0, 0, 0};
    const std::int64_t top = column_top(*tree_, tree_->root, 0, origin, cx, cz, yLimit, g.voxel_bits());
    if (top == kNoVoxel) {
        return -std::numeric_limits<float>::infinity();
    }
    // The TOP of that voxel, in world metres.
    return g.origin.y + static_cast<float>(top + 1) * edge;
}

} // namespace world::collision
