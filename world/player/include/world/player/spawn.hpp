#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>

#include "world/player/tuning.hpp"

namespace world::player {

// The one thing placing a body needs from a world: a column's analytic surface height. A CONCEPT
// rather than a link-time dependency on `world_generation`, so `world_player` keeps its current
// dependency set and the caller keeps ownership of the generator -- the same shape as
// `world::collision::SolidQuery` and `world::svo::VoxelSampler`, for the same reason.
template <typename H>
concept HeightSampler = requires(const H& h, float x, float z) {
    { h.height_at(x, z) } -> std::convertible_to<float>;
};

// Where a body's FEET must start at (x, z) so it stands ON the voxel surface rather than inside it.
//
// This arithmetic was derived by the harness's `pose_ground` resolver (Prompt 003 goal 228) and
// lives here now because the APP needs the same answer and a second copy of it would drift -- which
// is goal 340's rule: two tiers of the same thing call one function, not two matching copies. Each
// of the four steps below is load-bearing and each one was a bug before it was a step:
//
//  1. The MAXIMUM over the body's footprint, not the height at its centre. The body is a 0.6 m box;
//     where the terrain falls ~0.6 m per metre the uphill corner sits ~0.19 m above the centre
//     column, and a body spawned at the centre's surface is 0.19 m INSIDE the uphill ground.
//  2. Clamped to sea level, so "on the ground" over water means on the water, not on a sea floor
//     60 m down.
//  3. SNAPPED UP TO THE VOXEL GRID. The sampler's rule is "a voxel is solid iff its BOTTOM is at or
//     below the column's surface height", so the voxel containing the analytic height is SOLID and
//     its top is above that height. Feet at the analytic height are inside the top solid voxel --
//     which is what goal 228's counter reported the moment the analytic backstop stopped hiding it:
//     `spawn_stand`, a scenario in which nothing moves, logged 181 ticks with the body in solid.
//  4. Plus TWO voxels of clearance. One is not enough, and the reason is step 3's own rule: the
//     column height is sampled at each VOXEL's min corner, so no point sample can predict the voxel
//     top of a neighbouring column. The measured miss was exactly one voxel (feet 66.3438, uphill
//     corner voxel top 66.3516, edge 0.0078). The second voxel is float margin, and the body
//     settles the remaining centimetre under gravity, which is what walk mode is for.
template <HeightSampler H>
[[nodiscard]] float ground_feet_height(const H& heightmap, float x, float z, float voxelEdge,
                                       float halfWidth = kDefaultTuning.body_half_width) noexcept {
    float analytic = -std::numeric_limits<float>::infinity();
    for (int corner = 0; corner < 5; ++corner) {
        const float dx = corner == 4 ? 0.0f : ((corner & 1) != 0 ? halfWidth : -halfWidth);
        const float dz = corner == 4 ? 0.0f : ((corner & 2) != 0 ? halfWidth : -halfWidth);
        analytic = std::max(analytic, heightmap.height_at(x + dx, z + dz));
    }
    analytic = std::max(analytic, kSeaLevelWorld);
    return std::ceil(analytic / voxelEdge) * voxelEdge + 2.0f * voxelEdge;
}

} // namespace world::player
