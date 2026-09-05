#pragma once

#include <cstdint>
#include <vector>

#include "world/chunk/chunk_coord.hpp"

namespace world::streaming {

// Group S (Voxel Representation Redesign SS3): the world is now static and bounded instead of an
// unbounded window streamed around a moving camera. `radius_chunks` is a horizontal Chebyshev
// (X/Z) half-size around the origin column; every in-bounds column loads its FULL vertical band
// [y_min, y_max] regardless of where the camera ends up (the same ribbon-bug lesson the old
// StreamingConfig already encoded, just with no camera-relative radius left to get wrong).
struct WorldBounds {
    std::int32_t radius_chunks = 0;
    std::int32_t y_min = -3;
    std::int32_t y_max = 2;
};

// SS3.2's trial size: 48 columns per side (~1.5km at this project's ~1-voxel-per-meter, 32-voxel
// chunk scale) -- proven with a real measurement (goal 131) before scaling toward the original 8km
// ask, per SS3.2's explicit "measure, then scale" plan. Not 8km yet.
inline constexpr WorldBounds kDefaultWorldBounds{/*radius_chunks=*/48, /*y_min=*/-3, /*y_max=*/2};

// Every chunk column within `bounds.radius_chunks` (inclusive), full vertical band -- the set that
// gets MESHED and rendered. A separate, wider generation-only halo (radius_chunks + 1) exists so
// these columns' own 26-neighbor meshing precondition is satisfied at the world's edge; that halo
// is the loader's own concern (world_loader.hpp), not this pure shape query.
[[nodiscard]] inline std::vector<world::chunk::ChunkCoord> chunks_in_bounds(const WorldBounds& bounds) {
    std::vector<world::chunk::ChunkCoord> coords;
    const auto side = static_cast<std::size_t>(2 * bounds.radius_chunks + 1);
    const std::int32_t yCount = bounds.y_max - bounds.y_min + 1;
    coords.reserve(side * side *
                   static_cast<std::size_t>(yCount)); // NOLINT(bugprone-misplaced-widening-cast)
    for (std::int32_t cz = -bounds.radius_chunks; cz <= bounds.radius_chunks; ++cz) {
        for (std::int32_t cx = -bounds.radius_chunks; cx <= bounds.radius_chunks; ++cx) {
            for (std::int32_t cy = bounds.y_min; cy <= bounds.y_max; ++cy) {
                coords.push_back(world::chunk::ChunkCoord{cx, cy, cz});
            }
        }
    }
    return coords;
}

} // namespace world::streaming
