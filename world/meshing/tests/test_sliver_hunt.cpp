#include <cmath>
#include <cstdint>
#include <cstdio>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"

using namespace world::chunk;
using namespace world::meshing;

// Diagnostic for the floating-sliver artifact found by Stage 3's captures (see
// research/water-foliage-design.md "NEW ISSUE"): thin unlit-stone curtains hanging in the air
// near (100-130, 32-64, 60-90) at seed 1337. If the slivers are mesh-data real (not a runtime
// upload/draw artifact), extraction of the real-seed chunks must contain triangles floating well
// above the analytic surface. This test localizes them and fails loudly with coordinates.
TEST_CASE("no mesh triangle floats far above the analytic surface", "[meshing][sliver]") {
    const world::generation::HeightmapGenerator generator(1337);
    ChunkStore store;

    // Suspect region: chunk columns x in [2,4], z in [1,3], full y band [-4,3] plus halo margin.
    for (std::int32_t cz = -1; cz <= 5; ++cz) {
        for (std::int32_t cx = 0; cx <= 6; ++cx) {
            for (std::int32_t cy = -4; cy <= 3; ++cy) {
                world::generation::fill_terrain(store.get_or_create({cx, cy, cz}), generator);
            }
        }
    }

    std::size_t floating = 0;
    for (std::int32_t cz = 0; cz <= 4; ++cz) {
        for (std::int32_t cx = 1; cx <= 5; ++cx) {
            for (std::int32_t cy = 0; cy <= 2; ++cy) {
                const ChunkCoord coord{cx, cy, cz};
                // Replicate the RUNTIME path exactly: the mesh job never reads the live store --
                // it reads a 27-chunk snapshot built by copy-assigning voxels() into a fresh
                // ChunkStore (chunk_streaming.cpp). If copy-assignment corrupts paletted data,
                // only this form shows it.
                ChunkStore snapshot;
                for (std::int32_t dz = -1; dz <= 1; ++dz) {
                    for (std::int32_t dy = -1; dy <= 1; ++dy) {
                        for (std::int32_t dx = -1; dx <= 1; ++dx) {
                            const ChunkCoord n{coord.x + dx, coord.y + dy, coord.z + dz};
                            snapshot.get_or_create(n).voxels() = store.find(n)->voxels();
                        }
                    }
                }
                const MeshData mesh = extract_mesh(snapshot, coord);
                const float ox = static_cast<float>(coord.x) * 32.0f;
                const float oy = static_cast<float>(coord.y) * 32.0f;
                const float oz = static_cast<float>(coord.z) * 32.0f;
                for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                    bool allFloat = true;
                    for (std::size_t k = 0; k < 3 && allFloat; ++k) {
                        const Vertex& v = mesh.vertices[mesh.indices[i + k]];
                        const float wx = ox + v.position.x;
                        const float wy = oy + v.position.y;
                        const float wz = oz + v.position.z;
                        const float surface = generator.height_at(wx, wz);
                        // 2.5 voxels of tolerance: surface-nets placement + banding never puts a
                        // legitimate SURFACE vertex that far above its own column's height.
                        allFloat = wy > surface + 2.5f && wy > 2.5f; // (and not a water surface)
                    }
                    if (allFloat) {
                        if (floating < 12) {
                            const Vertex& v = mesh.vertices[mesh.indices[i]];
                            std::printf("floating tri in chunk[%d,%d,%d] at local (%.1f,%.1f,%.1f) mat=%d\n",
                                        coord.x, coord.y, coord.z, static_cast<double>(v.position.x),
                                        static_cast<double>(v.position.y), static_cast<double>(v.position.z),
                                        static_cast<int>(v.material));
                        }
                        ++floating;
                    }
                }
            }
        }
    }
    std::printf("total floating triangles: %zu\n", floating);
    CHECK(floating == 0);
}
