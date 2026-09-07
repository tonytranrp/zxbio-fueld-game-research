#include "world/svo/beam.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "world/svo/tree_layout.hpp"

namespace world::svo {
namespace {

struct Walk {
    const std::uint32_t* nodes = nullptr;
    glm::vec3 origin{0.0f}; // the beam apex, relative to the tree's own origin
    glm::vec3 dir{0.0f};
    glm::vec3 invd{0.0f};
    float tanHalf = 0.0f;
    float maxT = 0.0f;
    bool stopBelowCone = true;
    int maxDepth = kMaxLevels;
    float best = std::numeric_limits<float>::infinity();
    int deepest = 0;              // diagnostics: how deep this actually went
    std::uint32_t visited = 0;    // diagnostics: nodes tested
};

// Slab test of the centre ray against [lo, hi]. Returns false on a miss; otherwise `tNear` is the
// entry distance clamped at zero (an apex inside the box enters at 0).
bool slab(const Walk& w, const glm::vec3& lo, const glm::vec3& hi, float& tNear) noexcept {
    float enter = 0.0f;
    float exit = w.maxT;
    for (int a = 0; a < 3; ++a) {
        // An axis the ray does not travel along constrains nothing -- unless the apex is already
        // outside the slab on that axis, in which case nothing on it is reachable at all. Handled
        // explicitly rather than through a large finite `invd`: with dir[a] == 0 and the apex
        // exactly on a face, (hi - o) * 1e30 evaluates to 0 and reports the node as entered at
        // t = 0. That is still conservative, but it collapses the bound to the root's own entry
        // for the very common case of a camera looking straight down an axis -- which is exactly
        // how this was found (the zero-angle test measured a bound 14 m early on a 64 m root).
        if (std::abs(w.dir[a]) < 1.0e-20f) {
            if (w.origin[a] < lo[a] || w.origin[a] > hi[a]) {
                return false;
            }
            continue;
        }
        const float t0 = (lo[a] - w.origin[a]) * w.invd[a];
        const float t1 = (hi[a] - w.origin[a]) * w.invd[a];
        enter = std::max(enter, std::min(t0, t1));
        exit = std::min(exit, std::max(t0, t1));
    }
    if (exit < enter) {
        return false;
    }
    tNear = enter;
    return true;
}

// The distance from the apex to the farthest corner of [lo, hi] -- the largest `t` at which the
// cone's radius could still matter for this node, and therefore a safe radius to expand by.
float far_corner_distance(const Walk& w, const glm::vec3& lo, const glm::vec3& hi) noexcept {
    glm::vec3 d{0.0f};
    for (int a = 0; a < 3; ++a) {
        d[a] = std::max(std::abs(lo[a] - w.origin[a]), std::abs(hi[a] - w.origin[a]));
    }
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

void descend(Walk& w, std::uint32_t node, const glm::vec3& lo, float edge, int depth) noexcept {
    const glm::vec3 hi = lo + glm::vec3{edge};

    // Expand by the cone's radius at this node's farthest reach, then test the centre ray. See the
    // header for why `tNear` from THIS box is a lower bound on when any ray in the cone can reach
    // any geometry inside the unexpanded one.
    const float radius = w.tanHalf * far_corner_distance(w, lo, hi);
    float tNear = 0.0f;
    if (!slab(w, lo - glm::vec3{radius}, hi + glm::vec3{radius}, tNear)) {
        return;
    }
    if (tNear >= w.best) {
        return; // nothing in here can beat what we already have
    }

    ++w.visited;
    w.deepest = std::max(w.deepest, depth);

    // Below the cone's own cross-section, or past the depth ceiling, the node's entry IS the answer.
    // Conservative either way: every point of geometry inside this node is at t >= tNear.
    if (depth >= w.maxDepth || (w.stopBelowCone && edge < radius)) {
        w.best = std::min(w.best, tNear);
        return;
    }

    const std::uint32_t header = w.nodes[node];
    const std::uint32_t kind = node_kind(header);
    if (kind == kNodeKindSolid || kind == kNodeKindBrick) {
        // A brick is eight finest voxels across; descending into its DDA to tighten the bound by at
        // most one brick costs far more than it saves, so the node's own entry is the bound.
        w.best = std::min(w.best, tNear);
        return;
    }

    // Internal: visit children nearest-first so the pruning above has something to prune against
    // as early as possible.
    const std::uint32_t mask = node_child_mask(header);
    const float half = 0.5f * edge;
    struct Child {
        std::uint32_t node;
        glm::vec3 lo;
        float tNear;
    };
    std::array<Child, 8> children{};
    std::size_t count = 0;
    for (int octant = 0; octant < 8; ++octant) {
        if ((mask & (1u << octant)) == 0u) {
            continue; // air is not a node at all
        }
        const glm::vec3 childLo{lo.x + ((octant & 1) != 0 ? half : 0.0f),
                                lo.y + ((octant & 2) != 0 ? half : 0.0f),
                                lo.z + ((octant & 4) != 0 ? half : 0.0f)};
        const glm::vec3 childHi = childLo + glm::vec3{half};
        const float childRadius = w.tanHalf * far_corner_distance(w, childLo, childHi);
        float childNear = 0.0f;
        if (!slab(w, childLo - glm::vec3{childRadius}, childHi + glm::vec3{childRadius}, childNear)) {
            continue;
        }
        children[count++] = Child{w.nodes[node + node_child_slot(header, octant)], childLo, childNear};
    }
    std::sort(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(count),
              [](const Child& a, const Child& b) { return a.tNear < b.tNear; });
    for (std::size_t i = 0; i < count; ++i) {
        if (children[i].tNear >= w.best) {
            break; // sorted, so every remaining child is farther still
        }
        descend(w, children[i].node, children[i].lo, half, depth + 1);
    }
}

} // namespace

float beam_start_t(const BrickTree& tree, const Beam& beam, BeamStats* stats) noexcept {
    if (tree.empty()) {
        return std::numeric_limits<float>::infinity();
    }
    const float rootEdge = tree.geometry.root_edge();

    Walk w;
    w.nodes = tree.nodes.data();
    w.origin = beam.origin - tree.geometry.origin;
    w.dir = beam.dir;
    for (int a = 0; a < 3; ++a) {
        w.invd[a] = std::abs(beam.dir[a]) > 1.0e-20f ? 1.0f / beam.dir[a]
                                                     : (beam.dir[a] >= 0.0f ? 1.0e30f : -1.0e30f);
    }
    w.tanHalf = std::max(beam.tan_half_angle, 0.0f);
    w.maxT = beam.max_t;
    w.stopBelowCone = beam.stop_below_cone;
    w.maxDepth = std::max(beam.max_depth, 1);

    descend(w, tree.root, glm::vec3{0.0f}, rootEdge, 0);
    if (stats != nullptr) {
        stats->deepest_level = w.deepest;
        stats->nodes_visited = w.visited;
    }
    return w.best;
}

} // namespace world::svo
