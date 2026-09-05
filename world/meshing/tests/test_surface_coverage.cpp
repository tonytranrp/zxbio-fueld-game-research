#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"

// TERRAIN_FIXES follow-up, still real after Group Q's blocky-meshing rewrite (research/
// voxel-representation-redesign.md SS2): after meshing a full column stack, EVERY above-sea land
// column must be covered by some real top-facing surface geometry near its analytic height.
//
// Group Q note: this can no longer be "is some VERTEX near this column's center" the way it was
// under Surface Nets (which placed a vertex at essentially every active cell). Greedy meshing
// merges a large flat run into a handful of big quads whose only vertices sit at the run's own
// outer corners -- a column deep in the interior of one huge merged quad legitimately has no
// nearby vertex at all, and that is correct, not a coverage gap. The real property is "the column
// falls inside some top-facing quad's 2D footprint," checked by reconstructing each top face's
// world-space rectangle from its own 4 corners, not by vertex proximity.
TEST_CASE("every land column in a full stack has surface geometry", "[meshing][coverage]") {
    const world::generation::HeightmapGenerator generator(1337);
    world::chunk::ChunkStore store;
    // One full column stack (with halo) at the origin chunk column.
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (std::int32_t cy = -4; cy <= 3; ++cy) {
                world::generation::fill_terrain(store.get_or_create({dx, cy, dz}), generator);
            }
        }
    }

    struct TopFace {
        float xMin, xMax, zMin, zMax, y;
    };
    std::vector<TopFace> topFaces;
    for (std::int32_t cy = -3; cy <= 2; ++cy) {
        const auto mesh = world::meshing::extract_mesh(store, {0, cy, 0});
        // Every emitted face is one contiguous group of 4 vertices sharing one flat normal (Group
        // Q's own contract -- see mesh_extractor.cpp's emit_quad); only +Y ("top") faces are
        // relevant to "does this column have a walkable surface."
        for (std::size_t i = 0; i + 3 < mesh.vertices.size(); i += 4) {
            if (mesh.vertices[i].normal != glm::vec3{0.0f, 1.0f, 0.0f}) {
                continue;
            }
            float xMin = mesh.vertices[i].position.x;
            float xMax = xMin;
            float zMin = mesh.vertices[i].position.z;
            float zMax = zMin;
            for (std::size_t k = 1; k < 4; ++k) {
                const glm::vec3& p = mesh.vertices[i + k].position;
                xMin = std::min(xMin, p.x);
                xMax = std::max(xMax, p.x);
                zMin = std::min(zMin, p.z);
                zMax = std::max(zMax, p.z);
            }
            topFaces.push_back(
                {xMin, xMax, zMin, zMax, mesh.vertices[i].position.y + static_cast<float>(cy) * 32.0f});
        }
    }

    std::vector<std::pair<std::int32_t, std::int32_t>> uncovered;
    for (std::int32_t lz = 0; lz < 32; ++lz) {
        for (std::int32_t lx = 0; lx < 32; ++lx) {
            const float h = generator.height_at(static_cast<float>(lx) + 0.5f, static_cast<float>(lz) + 0.5f);
            if (h <= 1.0f) {
                continue; // underwater/beach columns render as the water surface -- out of scope here
            }
            const float cx = static_cast<float>(lx) + 0.5f;
            const float cz = static_cast<float>(lz) + 0.5f;
            bool covered = false;
            for (const TopFace& f : topFaces) {
                if (cx >= f.xMin && cx <= f.xMax && cz >= f.zMin && cz <= f.zMax &&
                    std::abs(f.y - h) <= 6.0f) {
                    covered = true;
                    break;
                }
            }
            if (!covered) {
                uncovered.emplace_back(lx, lz);
            }
        }
    }

    if (!uncovered.empty()) {
        std::printf("%zu uncovered land columns; first few (lx,lz,h):\n", uncovered.size());
        for (std::size_t i = 0; i < std::min<std::size_t>(uncovered.size(), 12); ++i) {
            const auto [lx, lz] = uncovered[i];
            std::printf("  (%d,%d) h=%.2f\n", lx, lz,
                        static_cast<double>(generator.height_at(static_cast<float>(lx) + 0.5f,
                                                                static_cast<float>(lz) + 0.5f)));
        }
    }
    CHECK(uncovered.empty());
}
