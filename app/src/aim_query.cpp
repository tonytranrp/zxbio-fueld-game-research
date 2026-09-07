#include "aim_query.hpp"

#include <algorithm>
#include <cmath>

#include "world/chunk/chunk_coord.hpp"
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

[[nodiscard]] std::int32_t column_of(float world) noexcept {
    return static_cast<std::int32_t>(std::floor(world / static_cast<float>(world::chunk::kChunkSize)));
}

} // namespace

const std::vector<world::generation::TreePlacement>& TreeLookup::column(std::int32_t cx,
                                                                        std::int32_t cz) const {
    for (const Column& c : cache_) {
        if (c.x == cx && c.z == cz) {
            return c.trees;
        }
    }
    cache_.push_back(Column{cx, cz, world::generation::compute_tree_placements(cx, cz, seed_, *heightmap_)});
    return cache_.back().trees;
}

world::chunk::MaterialID TreeLookup::material_at(const glm::vec3& p) const {
    const std::int32_t cx = column_of(p.x);
    const std::int32_t cz = column_of(p.z);
    // A canopy overhangs its own column, so the eight neighbours are in scope too. Cheap: the
    // placements are cached, and the bounds test rejects almost everything immediately.
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (const world::generation::TreePlacement& tree : column(cx + dx, cz + dz)) {
                const world::generation::TreeBounds bounds = world::generation::tree_bounds(tree);
                if (p.x < bounds.min.x || p.x > bounds.max.x || p.y < bounds.min.y || p.y > bounds.max.y ||
                    p.z < bounds.min.z || p.z > bounds.max.z) {
                    continue;
                }
                const world::chunk::MaterialID material = world::generation::tree_material_at(tree, p);
                if (material != world::chunk::MaterialID::Air) {
                    return material;
                }
            }
        }
    }
    return world::chunk::MaterialID::Air;
}

AimHit query_aim(const world::generation::HeightmapGenerator& heightmap, glm::vec3 origin,
                 glm::vec3 direction, float maxDistance, const TreeLookup* trees) {
    AimHit result;
    const float dirLength = glm::length(direction);
    if (dirLength <= 0.0f) {
        return result;
    }
    const glm::vec3 dir = direction / dirLength;

    // Fixed-step march with one bisection refinement: plenty for a debug crosshair readout. The
    // tree test rides the same steps -- a trunk is 0.5-0.7 m thick, so a 0.5 m step cannot skip
    // one, and a canopy is metres across.
    constexpr float kStep = 0.5f;
    glm::vec3 prev = origin;
    for (float t = kStep; t <= maxDistance; t += kStep) {
        const glm::vec3 p = origin + dir * t;
        const float surface = heightmap.height_at(p.x, p.z);

        // Trees first: they stand in FRONT of the terrain behind them, which is exactly what the
        // pre-A4 query got wrong (a trunk reported as the hillside it occludes).
        if (trees != nullptr) {
            const world::chunk::MaterialID treeMaterial = trees->material_at(p);
            if (treeMaterial != world::chunk::MaterialID::Air) {
                result.hit = true;
                result.material = treeMaterial;
                result.position = p;
                result.distance = t;
                return result;
            }
        }

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
            result.distance = glm::length(hi - origin);
            result.material = surface_material(heightmap, hi.x, hi.z, heightmap.height_at(hi.x, hi.z));
            return result;
        }
        // Crossing the water plane over a submerged column = the water surface is the hit.
        if (prev.y > kSeaLevel && p.y <= kSeaLevel && surface < kSeaLevel) {
            const float tPlane = (prev.y - kSeaLevel) / (prev.y - p.y);
            result.hit = true;
            result.position = prev + (p - prev) * tPlane;
            result.distance = glm::length(result.position - origin);
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
