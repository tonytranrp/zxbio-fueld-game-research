#include "aim_query.hpp"

#include <algorithm>
#include <cmath>

namespace app {

namespace {

// Mirror of terrain_fill.cpp's surface-banding constants -- update together (the fill is the
// authority; this query re-derives its SURFACE material only).
constexpr float kBeachBand = 1.75f;
constexpr float kGrassMaxSlope = 1.9f;
constexpr float kSeaLevel = 0.0f;

world::chunk::MaterialID surface_material(const world::generation::HeightmapGenerator& heightmap, float x, float z,
                                          float surface) {
    if (surface <= kSeaLevel + kBeachBand) {
        return world::chunk::MaterialID::Sand;
    }
    const float slopeX = std::abs(heightmap.height_at(x + 1.0f, z) - heightmap.height_at(x - 1.0f, z)) * 0.5f;
    const float slopeZ = std::abs(heightmap.height_at(x, z + 1.0f) - heightmap.height_at(x, z - 1.0f)) * 0.5f;
    return std::max(slopeX, slopeZ) <= kGrassMaxSlope ? world::chunk::MaterialID::Grass
                                                      : world::chunk::MaterialID::Stone;
}

} // namespace

AimHit query_aim(const world::generation::HeightmapGenerator& heightmap, glm::vec3 origin, glm::vec3 direction,
                 float maxDistance) {
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
    switch (material) {
    case world::chunk::MaterialID::Air: return "Air";
    case world::chunk::MaterialID::Stone: return "Stone";
    case world::chunk::MaterialID::Dirt: return "Dirt";
    case world::chunk::MaterialID::Water: return "Water";
    case world::chunk::MaterialID::Wood: return "Wood";
    case world::chunk::MaterialID::Leaves: return "Leaves";
    case world::chunk::MaterialID::Sand: return "Sand";
    case world::chunk::MaterialID::Grass: return "Grass";
    }
    return "?";
}

} // namespace app
