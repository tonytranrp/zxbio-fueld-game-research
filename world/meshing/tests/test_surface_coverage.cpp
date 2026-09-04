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

// TERRAIN_FIXES follow-up: the post-Q visual capture still shows terrain as elevation-striped
// ribbons, so surface geometry is MISSING for many columns even with the full band loaded. This
// test states the invariant the renderer visibly violates: after meshing a full column stack,
// EVERY above-sea land column has at least one surface vertex near it at roughly its height.
// Whatever stripe pattern fails here is the bug's fingerprint.
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

    struct WorldVertex {
        float x, y, z;
    };
    std::vector<WorldVertex> vertices;
    for (std::int32_t cy = -3; cy <= 2; ++cy) {
        const auto mesh = world::meshing::extract_mesh(store, {0, cy, 0});
        for (const auto& v : mesh.vertices) {
            vertices.push_back({v.position.x, v.position.y + static_cast<float>(cy) * 32.0f, v.position.z});
        }
    }

    std::vector<std::pair<std::int32_t, std::int32_t>> uncovered;
    for (std::int32_t lz = 0; lz < 32; ++lz) {
        for (std::int32_t lx = 0; lx < 32; ++lx) {
            const float h = generator.height_at(static_cast<float>(lx) + 0.5f, static_cast<float>(lz) + 0.5f);
            if (h <= 1.0f) {
                continue; // underwater/beach columns render as the water surface -- out of scope here
            }
            bool covered = false;
            for (const WorldVertex& v : vertices) {
                if (std::abs(v.x - (static_cast<float>(lx) + 0.5f)) <= 1.5f &&
                    std::abs(v.z - (static_cast<float>(lz) + 0.5f)) <= 1.5f && std::abs(v.y - h) <= 6.0f) {
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
