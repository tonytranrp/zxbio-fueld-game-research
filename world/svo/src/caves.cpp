// Prompt 006 goal 313. See caves.hpp for the occupancy-rule plan.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "world/svo/caves.hpp"

namespace world::svo {
namespace {

/// A cheap value-noise gradient field, written here rather than pulled from FastNoise2.
///
/// FastNoise2 lives behind `world::generation::HeightmapGenerator` and nowhere else, by the
/// project's own rule ("FastNoise2 usage lives here, nowhere else"). Reaching it from `world/svo`
/// would either break that or route every cave sample through a heightmap call. A 3D value noise is
/// forty lines and deterministic, which is what this needs.
[[nodiscard]] float hash3(std::int32_t x, std::int32_t y, std::int32_t z, std::uint32_t seed) {
    std::uint32_t h = seed;
    h ^= static_cast<std::uint32_t>(x) * 0x9e3779b9u;
    h ^= static_cast<std::uint32_t>(y) * 0x85ebca6bu;
    h ^= static_cast<std::uint32_t>(z) * 0xc2b2ae35u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h) / 2147483648.0f - 1.0f; // [-1, 1)
}

[[nodiscard]] float smootherstep(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

[[nodiscard]] float value_noise(float x, float y, float z, std::uint32_t seed) {
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const float fz = std::floor(z);
    const auto ix = static_cast<std::int32_t>(fx);
    const auto iy = static_cast<std::int32_t>(fy);
    const auto iz = static_cast<std::int32_t>(fz);
    const float tx = smootherstep(x - fx);
    const float ty = smootherstep(y - fy);
    const float tz = smootherstep(z - fz);
    const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float c000 = hash3(ix, iy, iz, seed);
    const float c100 = hash3(ix + 1, iy, iz, seed);
    const float c010 = hash3(ix, iy + 1, iz, seed);
    const float c110 = hash3(ix + 1, iy + 1, iz, seed);
    const float c001 = hash3(ix, iy, iz + 1, seed);
    const float c101 = hash3(ix + 1, iy, iz + 1, seed);
    const float c011 = hash3(ix, iy + 1, iz + 1, seed);
    const float c111 = hash3(ix + 1, iy + 1, iz + 1, seed);
    return lerp(lerp(lerp(c000, c100, tx), lerp(c010, c110, tx), ty),
                lerp(lerp(c001, c101, tx), lerp(c011, c111, tx), ty), tz);
}

} // namespace

bool cave_at(const CaveParams& p, const glm::vec3& position, float surfaceHeight) {
    if (!p.enabled()) {
        return false;
    }
    // The band and the water table, exactly as `caves_possible_in_band` promises. These two tests
    // must stay in agreement with it or the conservative bound stops being conservative -- which is
    // the one way this feature can put a hole in the world.
    const float depth = surfaceHeight - position.y;
    if (depth < p.min_depth_m || depth > p.max_depth_m) {
        return false;
    }
    if (position.y <= p.water_table_m) {
        return false;
    }

    // TWO crossed tunnel fields, which is what makes passages rather than blobs. A single |noise| <
    // t test carves a shell around every zero-crossing surface -- a sheet, not a tunnel. The
    // INTERSECTION of two such shells is a curve, and a curve thickened by the threshold is a
    // passage. (Part 4 §4's "spaghetti" morphology; the research's §11 recipe names the same trick.)
    const float nx = position.x / p.scale_xz_m;
    const float ny = position.y / p.scale_y_m;
    const float nz = position.z / p.scale_xz_m;
    const float a = value_noise(nx, ny, nz, p.seed);
    const float b = value_noise(nx + 41.7f, ny + 13.1f, nz - 27.3f, p.seed ^ 0x5bd1e995u);
    if (std::abs(a) > p.threshold || std::abs(b) > p.threshold) {
        return false;
    }

    // Taper to nothing at the band's edges, so a passage closes rather than being sliced flat by
    // the depth test -- a flat ceiling at exactly min_depth everywhere would read as a bug.
    const float edge = std::min(depth - p.min_depth_m, p.max_depth_m - depth);
    const float taper = std::min(1.0f, edge / 4.0f);
    const float room = p.threshold * taper;
    return std::abs(a) <= room && std::abs(b) <= room;
}

CaveStats measure_caves(const CaveParams& p, const glm::vec3& min, const glm::vec3& max, float step,
                        float (*surfaceAt)(float, float, void*), void* user) {
    CaveStats stats;
    if (step <= 0.0f || surfaceAt == nullptr) {
        return stats;
    }
    double widthSum = 0.0;
    double heightSum = 0.0;

    for (float z = min.z; z <= max.z; z += step) {
        for (float x = min.x; x <= max.x; x += step) {
            const float surface = surfaceAt(x, z, user);
            // Horizontal run lengths along X give passage WIDTH; vertical runs give HEIGHT. Both
            // are measured on the same column set so the two statistics describe the same passages.
            float runStart = 0.0f;
            bool inRun = false;
            for (float y = min.y; y <= max.y; y += step) {
                ++stats.samples;
                const bool voidHere = cave_at(p, glm::vec3{x, y, z}, surface);
                if (voidHere) {
                    ++stats.void_samples;
                    if (y <= p.water_table_m) {
                        ++stats.below_water_table;
                    }
                }
                if (voidHere && !inRun) {
                    runStart = y;
                    inRun = true;
                } else if (!voidHere && inRun) {
                    heightSum += static_cast<double>(y - runStart);
                    ++stats.passages_measured;
                    inRun = false;
                }
            }
        }
    }
    // Width: the same scan along X at a few heights, so the number is a passage width rather than a
    // re-measured height.
    std::size_t widthRuns = 0;
    for (float z = min.z; z <= max.z; z += step * 4.0f) {
        for (float y = min.y; y <= max.y; y += step * 4.0f) {
            float runStart = 0.0f;
            bool inRun = false;
            for (float x = min.x; x <= max.x; x += step) {
                const float surface = surfaceAt(x, z, user);
                const bool voidHere = cave_at(p, glm::vec3{x, y, z}, surface);
                if (voidHere && !inRun) {
                    runStart = x;
                    inRun = true;
                } else if (!voidHere && inRun) {
                    widthSum += static_cast<double>(x - runStart);
                    ++widthRuns;
                    inRun = false;
                }
            }
        }
    }
    if (stats.samples != 0) {
        stats.void_fraction = static_cast<double>(stats.void_samples) / static_cast<double>(stats.samples);
    }
    if (stats.passages_measured != 0) {
        stats.mean_passage_height_m = heightSum / static_cast<double>(stats.passages_measured);
    }
    if (widthRuns != 0) {
        stats.mean_passage_width_m = widthSum / static_cast<double>(widthRuns);
    }
    return stats;
}

} // namespace world::svo
