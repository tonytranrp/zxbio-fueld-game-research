#include <algorithm>
#include <cmath>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"

// TERRAIN_FIXES_BRIEF Group V task 23's check: the analytic ground query must agree with the
// actual generated MESH surface -- not just with itself. For several columns: query height_at,
// generate + Surface-Nets-mesh the chunk containing that height, and compare the topmost mesh
// vertex near the column against the query.
TEST_CASE("ground query matches the extracted mesh surface", "[generation][walk]") {
    const world::generation::HeightmapGenerator generator(1337);

    // Above-sea-level columns only (probed for this seed; includes negative coordinates): an
    // UNDERWATER column's terrain surface is a stone->water transition the mesher deliberately
    // does not emit (water renders as its own top surface), so query-vs-mesh comparison is only
    // meaningful on land. Walk mode handles water separately via swimming/buoyancy, not a clamp
    // (see spectator_camera.cpp; WorldLoader::ground_height is the shared analytic height query).
    const std::int32_t columns[][2] = {{16, 16}, {5, 25}, {40, 70}, {-10, 60}, {-100, 30}};
    for (const auto& column : columns) {
        const float worldX = static_cast<float>(column[0]);
        const float worldZ = static_cast<float>(column[1]);
        const float queried = generator.height_at(worldX, worldZ);

        // Mesh the y-stack around the chunk containing this height (the surface can sit exactly
        // on a chunk boundary, leaving the single middle chunk's own extraction empty), with the
        // full halo each extraction needs.
        const std::int32_t cx = world::chunk::world_to_chunk(column[0]);
        const std::int32_t cy = world::chunk::world_to_chunk(static_cast<std::int32_t>(std::floor(queried)));
        const std::int32_t cz = world::chunk::world_to_chunk(column[1]);
        world::chunk::ChunkStore store;
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dy = -2; dy <= 2; ++dy) {
                for (std::int32_t dx = -1; dx <= 1; ++dx) {
                    world::generation::fill_terrain(store.get_or_create({cx + dx, cy + dy, cz + dz}),
                                                    generator);
                }
            }
        }

        // The vertex horizontally NEAREST the column (not "topmost in a window"): steep columns
        // make any window's topmost vertex a biased estimate, and a cave-less heightfield
        // guarantees the nearest surface vertex tracks the column height.
        const float localX = worldX - static_cast<float>(cx) * 32.0f;
        const float localZ = worldZ - static_cast<float>(cz) * 32.0f;
        float bestDist = 1e9f;
        float surfaceY = -1e9f;
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            const auto mesh = world::meshing::extract_mesh(store, {cx, cy + dy, cz});
            for (const auto& vertex : mesh.vertices) {
                const float dx = vertex.position.x - localX;
                const float dz = vertex.position.z - localZ;
                const float dist = std::max(std::abs(dx), std::abs(dz));
                const float worldY = vertex.position.y + static_cast<float>(cy + dy) * 32.0f;
                if (dist < bestDist || (dist == bestDist && worldY > surfaceY)) {
                    bestDist = dist;
                    surfaceY = worldY;
                }
            }
        }
        REQUIRE(bestDist < 2.0f); // a surface vertex exists near the column somewhere in the stack
        // Tolerance = Surface Nets in-cell placement (~1) + local slope over the search offset
        // (max observed slope ~3.8/unit x up-to-2-unit offset). Coarse on purpose: this check
        // exists to catch meters-scale disagreement between query and mesh, not sub-voxel error.
        CHECK(std::abs(surfaceY - queried) < 1.0f + 4.0f * std::max(1.0f, bestDist));
    }
}
