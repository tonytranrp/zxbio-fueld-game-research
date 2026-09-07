#pragma once

// Prompt 004 goal 257: which detail band a cell belongs to, and therefore whether a camera move
// has invalidated it.
//
// THE PROBLEM THIS SOLVES. Goal 255 built the cell grid and goal 256 shipped it on the GPU, and
// then research section 18 found the blocker for incremental rebuild: with the builder's CONTINUOUS
// distance LOD, a node's level is a function of its distance from the camera, so moving the camera
// one metre changes the correct content of every cell in the grid. "Rebuild only what changed" then
// means "rebuild everything", and a grid buys nothing over one tree.
//
// THE FIX. Quantise a cell's detail to its distance BAND. Band 0 is full resolution, band 1 is
// voxels twice as large, band 2 four times, and the boundaries sit at `lod_radius`, 2x, 4x, ... So
// a cell's content is a STEP function of camera distance: it does not change at all until the
// camera crosses a boundary, and a camera move re-levels only the thin shell of cells that did.
//
// WHAT IT COSTS, stated up front rather than discovered later: the detail ramp becomes a staircase.
// Where the continuous rule shrinks voxels smoothly with distance, this holds one size across a
// whole band and then halves it. Whether that staircase is visible is a question for a viewed
// capture, and section 20 has one.

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine/core/math.hpp"

namespace world::svo {

/// Band 0 is full resolution; each band beyond doubles the voxel edge.
///
/// `distance` is from the camera to the NEAREST point of the cell, not its centre: a cell the
/// camera is standing inside must be band 0 however large it is, and its centre could be 16 m away.
[[nodiscard]] inline int lod_band(float distance, float lod_radius) noexcept {
    const float r = std::max(lod_radius, 1.0e-3f);
    if (!(distance > r)) {
        return 0; // also catches NaN, which must not become a huge band
    }
    return static_cast<int>(std::floor(std::log2(distance / r))) + 1;
}

/// The voxel edge a cell in `band` is built at, clamped so a very distant cell cannot ask for a
/// voxel coarser than the cell itself.
[[nodiscard]] inline float band_voxel_edge(int band, float finest_voxel_edge, float cell_edge) noexcept {
    const float coarsest = std::max(finest_voxel_edge, cell_edge);
    return std::min(coarsest, std::ldexp(finest_voxel_edge, std::max(band, 0)));
}

/// Distance from `point` to the nearest point of the axis-aligned cell at `coord`.
[[nodiscard]] inline float distance_to_cell(const glm::vec3& point, glm::ivec3 coord,
                                            float cell_edge) noexcept {
    const glm::vec3 lo = glm::vec3{coord} * cell_edge;
    const glm::vec3 hi = lo + glm::vec3{cell_edge};
    const glm::vec3 nearest = glm::clamp(point, lo, hi);
    return glm::length(nearest - point);
}

/// The band a cell belongs to for a camera at `camera`. One call, so nothing can compute it two
/// slightly different ways -- which is the bug that would present as a cell rebuilding forever.
[[nodiscard]] inline int cell_band(const glm::vec3& camera, glm::ivec3 coord, float cell_edge,
                                   float lod_radius) noexcept {
    return lod_band(distance_to_cell(camera, coord, cell_edge), lod_radius);
}

} // namespace world::svo
