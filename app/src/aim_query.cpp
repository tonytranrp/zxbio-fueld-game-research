#include "aim_query.hpp"

#include <algorithm>
#include <cmath>

#include "world/materials/materials.hpp"

namespace app {

namespace {

using world::materials::TerrainBands;
using world::materials::TerrainQuery;

constexpr float kSeaLevel = 0.0f;

// The SURFACE voxel of the column under (x, z), by the one band rule the chunk fill and the
// sparse-brick sampler use (materials::terrain_material at depth 0 in a 1 m voxel) -- the third
// hand-written copy of that rule and its constants lived here before Group AC. The slope is only
// computed when it can matter (a beach column is sand regardless).
world::chunk::MaterialID surface_material(const world::generation::HeightmapGenerator& heightmap, float x,
                                          float z, float surface) {
    const bool beach = TerrainBands::is_beach(surface, kSeaLevel);
    float slope = 0.0f;
    if (!beach) {
        const float slopeX =
            std::abs(heightmap.height_at(x + 1.0f, z) - heightmap.height_at(x - 1.0f, z)) * 0.5f;
        const float slopeZ =
            std::abs(heightmap.height_at(x, z + 1.0f) - heightmap.height_at(x, z - 1.0f)) * 0.5f;
        slope = std::max(slopeX, slopeZ);
    }
    const TerrainQuery query{surface, surface, 1.0f, kSeaLevel, beach, TerrainBands::is_grassy(beach, slope)};
    return world::materials::terrain_material(query);
}

} // namespace

AimHit query_aim(const world::generation::HeightmapGenerator& heightmap, glm::vec3 origin,
                 glm::vec3 direction, float maxDistance) {
    AimHit result;
    const float dirLength = glm::length(direction);
    if (dirLength <= 0.0f) {
        return result;
    }
    const glm::vec3 dir = direction / dirLength;

    // Fixed-step march with one bisection refinement: plenty for a debug crosshair readout.
    constexpr float kStep = 0.5f;
    glm::vec3 prev = origin;
    for (float t = kStep; t <= maxDistance; t += kStep) {
        const glm::vec3 p = origin + dir * t;
        const float surface = heightmap.height_at(p.x, p.z);
        if (p.y <= surface) {
            // Refine between prev and p, then classify by the fill's banding rule.
            glm::vec3 lo = prev;
            glm::vec3 hi = p;
            for (int i = 0; i < 8; ++i) {
                const glm::vec3 mid = (lo + hi) * 0.5f;
                (mid.y <= heightmap.height_at(mid.x, mid.z) ? hi : lo) = mid;
            }
            result.hit = true;
            result.position = hi;
            result.material = surface_material(heightmap, hi.x, hi.z, heightmap.height_at(hi.x, hi.z));
            return result;
        }
        // Crossing the water plane over a submerged column = the water surface is the hit.
        if (prev.y > kSeaLevel && p.y <= kSeaLevel && surface < kSeaLevel) {
            const float tPlane = (prev.y - kSeaLevel) / (prev.y - p.y);
            result.hit = true;
            result.position = prev + (p - prev) * tPlane;
            result.material = world::chunk::MaterialID::Water;
            return result;
        }
        prev = p;
    }
    return result;
}

const char* material_name(world::chunk::MaterialID material) noexcept {
    // The registry's display name (Group AC) -- this and tools/mesh_dump used to carry two switch
    // statements over the same eight strings.
    return world::materials::name_of(material);
}

} // namespace app
