#pragma once

// Prompt 004 goal 266: the coarse start-t pre-pass, as a CONSERVATIVE cone bound.
//
// Research section 4.7 lists four shipped or published systems doing this under four names --
// ESVO's beam optimization, Teardown's per-object linear-depth manual early-out with front-to-back
// ordering, Aokana's Hi-Z + visibility buffer, GigaVoxels' proxy geometry with Early-Z. None of
// them publishes a speedup this engine can borrow: the prompt records that ESVO's own beam
// resolution and measured speedup could not be confirmed from primary sources and that two
// contradictory numbers circulate. So the number is measured here, on this content, and only
// this engine's own measurements are quoted.
//
// THE IDEA. A tile of screen pixels is a frustum. If nothing solid lies inside that frustum before
// some distance, then NO ray in the tile can hit anything before it, so every one of them may skip
// straight there. `TraceParams::t_start` is where the answer goes.
//
// THE BOUND, AND WHY IT IS SAFE. The frustum is over-approximated by a cone: the centre direction
// plus the largest angle to any corner of the tile. For a node's AABB, let `tFar` be the distance
// from the apex to the AABB's farthest corner. Then for ANY ray R in the cone and ANY point P of
// geometry inside the node, with t_R the parameter at which R reaches P:
//
//   * t_R = |P - apex| <= tFar, because P lies in the AABB; and
//   * the CENTRE ray at parameter t_R is within t_R * tan(theta) of P, because R's direction is
//     within theta of the centre's and |R.dir - dir| <= 2 sin(theta/2) <= tan(theta).
//
// So the centre ray at t_R lies inside the node's AABB expanded by `tFar * tan(theta)`, and a slab
// test of the centre ray against that expanded box therefore returns a `tNear` with tNear <= t_R.
// **`tNear` is a lower bound on when any ray in the cone can reach any geometry in this node.**
// Taking the minimum over every node the expanded test admits gives a bound for the whole tile,
// and a node whose tNear already exceeds the running best can be pruned outright -- which is what
// makes this cheap.
//
// The bound is never tighter than the truth, which is the whole point: seeding a ray PAST real
// geometry makes it miss, and that is the single correctness risk of this optimisation.

#include <cmath>
#include <cstdint>
#include <limits>

#include "engine/core/math.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/tree_layout.hpp"

namespace world::svo {

/// A screen tile's frustum, over-approximated as a cone from a common apex.
struct Beam {
    glm::vec3 origin{0.0f};
    glm::vec3 dir{0.0f, 0.0f, -1.0f}; ///< Centre direction; must be normalized.
    /// tan of the cone's half-angle: the largest angle from `dir` to any ray in the tile. 0 makes
    /// the cone a single ray, which is a legitimate (and exact) degenerate case.
    float tan_half_angle = 0.0f;
    float max_t = std::numeric_limits<float>::infinity();
    /// Stop descending once a node is smaller than the cone's own radius there. Refining below the
    /// cone's cross-section cannot tighten the bound much -- the expanded box is dominated by the
    /// radius, not the node -- and stopping early is CONSERVATIVE by construction, since any
    /// geometry inside the node is at t >= the node's entry. It is what bounds the descent depth,
    /// which is what lets the same algorithm run on a GPU with a fixed stack.
    bool stop_below_cone = true;
    /// A hard ceiling on descent depth, for the same reason. Stopping early only loosens the bound.
    int max_depth = kMaxLevels;
};

/// A conservative lower bound on the distance at which any ray inside `beam` can first meet solid
/// geometry, or infinity when the cone meets nothing at all within `max_t`.
///
/// Feed the result to `TraceParams::t_start`. Bricks are bounded at their own node's entry rather
/// than descended into: a brick is eight finest voxels across, so refining inside one buys almost
/// nothing and costs a DDA per node.
/// How hard the descent worked -- how deep it went and how many nodes it tested. The depth is what
/// sizes the shader mirror's fixed stack, so it is measured rather than guessed.
struct BeamStats {
    int deepest_level = 0;
    std::uint32_t nodes_visited = 0;
};

[[nodiscard]] float beam_start_t(const BrickTree& tree, const Beam& beam,
                                 BeamStats* stats = nullptr) noexcept;

/// The cone that bounds a `tile_w` x `tile_h` block of pixels, given the per-pixel angular size
/// `pixel_angle` (radians). The half-angle spans from the tile's centre to its corner, plus a
/// one-pixel margin.
///
/// THE MARGIN IS NOT SLOP. Half of it covers the fact that a pixel is SAMPLED at its centre but
/// COVERS its whole footprint. The other half covers TAA's sub-pixel jitter, which shifts every
/// primary ray by up to half a pixel in each axis -- 0.707 px diagonally -- while the shader still
/// indexes the tile by the ray's UNJITTERED pixel. Without it a jittered edge ray can lean out of
/// the cone that bounded it, and the bound stops being conservative for that ray.
[[nodiscard]] inline float tile_tan_half_angle(float pixel_angle, float tile_w, float tile_h) noexcept {
    const float halfW = 0.5f * tile_w * pixel_angle;
    const float halfH = 0.5f * tile_h * pixel_angle;
    const float diagonal = std::sqrt(halfW * halfW + halfH * halfH) + pixel_angle;
    return std::tan(diagonal);
}

} // namespace world::svo
