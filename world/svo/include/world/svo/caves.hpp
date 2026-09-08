#pragma once

// Prompt 006 goal 313: caves, and the reopening of goal 80.
//
// GOAL 80 DECIDED AGAINST 3D DENSITY TERRAIN. The research reopened it: Part 4 §11 is written as "a
// cave-generation recipe for a voxel engine", §4 gives passage geometry and scale statistics, and
// Part 7 §7.5 argues for the heightfield-first hybrid this file implements -- d = h - y + N3, with
// cave channels subtracted from a heightfield world rather than the whole world becoming a density
// field. That hybrid is what makes the reopening affordable: the surface stays 2.5D and only a
// bounded band below it becomes 3D.
//
// ================================ THE OCCUPANCY-RULE PLAN ================================
//
// The sampler's rule is "solid iff the voxel bottom is at or below the column surface". That is a
// 2.5D world by construction, and `TerrainSampler::classify` leans on it hard: a box entirely below
// every column's surface returns Solid WITHOUT SUBDIVIDING. A cave inside such a box would never be
// looked for, and the world would render solid over it.
//
// So the change is not "subtract a noise field". It is: every place that concludes SOLID FOR A
// WHOLE BOX must first prove no cave can intersect that box. Four call sites, and the plan for
// each:
//
//   1. `classify`'s Solid fast paths (stone, soil, sand). Guarded by `caves_possible_in_band`
//      below. A box whose Y range misses the cave band keeps its fast path unchanged; a box that
//      touches the band returns Mixed and is subdivided. This is CONSERVATIVE BY CONSTRUCTION --
//      it never claims cave-free where a cave might be -- and it costs subdivision only inside the
//      band.
//   2. `fill_brick` / `fill_columns`, which write the actual voxels: carve per voxel.
//   3. `material_at`, the pointwise truth the crosshair and the tests use: carve per voxel.
//   4. `TerrainCollider`, which reimplements the occupancy rule analytically. Prompt 003 replaced
//      the body's ground query with an OCTREE query, so it reads the built world and inherits the
//      caves for free -- this is a verification, not a change (goal 322).
//
// WHY A BAND RATHER THAN A NOISE BOUND. Bounding a 3D noise field over an arbitrary box either
// needs interval arithmetic through FastNoise2 (which it does not offer) or a Lipschitz bound
// (which would be so loose that every box in the band returns Mixed anyway). The band is exact,
// costs one comparison, and confines the subdivision to where caves actually are. `HeightField`'s
// existing per-box surface range is what makes it tight.
//
// The 10,000-random-box check goal 313's Check requires is `test_caves.cpp`'s job: classify must
// never say "uniform" for a box that pointwise sampling finds mixed.

#include <cstdint>

#include <glm/vec3.hpp>

namespace world::svo {

struct CaveParams {
    /// Caves live in a depth band below the surface. The floor is what keeps them from breaching
    /// the surface everywhere (a cave mouth is the rare place the band reaches daylight), and the
    /// ceiling bounds how far `classify` has to subdivide.
    float min_depth_m = 6.0f;
    float max_depth_m = 55.0f;

    /// Research Part 7 §7.5: "cave density suppressed near the water table". Below this the rock is
    /// saturated and passages are flooded rather than open, so nothing is carved -- which also
    /// keeps the sea from draining into a tunnel network.
    float water_table_m = 2.0f;

    /// Tunnel radius as a fraction of the noise field's own scale. Part 4 §4's passage widths run
    /// from under a metre to tens of metres; this generator targets the walkable part of that.
    float threshold = 0.16f;

    /// Horizontal and vertical scale of the tunnel field, metres. Vertical is shorter so passages
    /// run more horizontally than vertically, which is what a water-table-organised cave does.
    float scale_xz_m = 90.0f;
    float scale_y_m = 42.0f;

    std::uint32_t seed = 1337;

    [[nodiscard]] constexpr bool enabled() const noexcept { return threshold > 0.0f; }
};

/// True when a cave MAY intersect the Y range [yMin, yMax] under columns whose surfaces span
/// [surfaceMin, surfaceMax]. Conservative: false means provably cave-free.
///
/// This is the whole safety property. `classify`'s Solid fast paths are valid exactly when this
/// returns false, and every caller that concludes "uniformly solid" over a box must consult it.
[[nodiscard]] constexpr bool caves_possible_in_band(const CaveParams& p, float yMin, float yMax,
                                                    float surfaceMin, float surfaceMax) noexcept {
    if (!p.enabled()) {
        return false;
    }
    // The band, over the whole footprint: from the deepest reach under the lowest column to the
    // shallowest reach under the highest one.
    const float bandLow = surfaceMin - p.max_depth_m;
    const float bandHigh = surfaceMax - p.min_depth_m;
    if (yMax < bandLow || yMin > bandHigh) {
        return false;
    }
    // Nothing is carved at or below the water table.
    return yMax > p.water_table_m;
}

/// True when the point is inside a cave void. Pointwise truth; everything else must agree with it.
[[nodiscard]] bool cave_at(const CaveParams& p, const glm::vec3& position, float surfaceHeight);

/// Diagnostics for goal 313's Check: passage width and height distributions, sampled over a region.
struct CaveStats {
    std::size_t samples = 0;
    std::size_t void_samples = 0;
    double void_fraction = 0.0;
    double mean_passage_width_m = 0.0;
    double mean_passage_height_m = 0.0;
    std::size_t passages_measured = 0;
    std::size_t below_water_table = 0; ///< must be zero
};

/// Walks a grid over the region, measuring passage extents by scanning runs of void along each
/// axis. The scan step is the resolution the statistics are quoted at.
[[nodiscard]] CaveStats measure_caves(const CaveParams& p, const glm::vec3& min, const glm::vec3& max,
                                      float step, float (*surfaceAt)(float, float, void*), void* user);

} // namespace world::svo
