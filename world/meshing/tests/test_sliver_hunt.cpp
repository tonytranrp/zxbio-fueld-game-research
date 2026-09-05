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
//
// Group Q correction (research/voxel-representation-redesign.md SS2): a blocky SIDE (riser) face
// sits its vertices EXACTLY on the boundary plane between its own occupied voxel and the exposed
// air beyond it -- checking height_at() at that boundary's own (x,z) can land on whichever
// neighboring column's height happens to be smaller, not the column that actually produced the
// solid geometry. A real repro from this exact test, seed 1337: a +Z riser at world (48,61,13)
// read height_at(48,13)=58.49 (the SHORT neighbor one step past the boundary) while
// height_at(48,12)=61.24 (the TALL source column the voxel is actually part of) -- a perfectly
// legitimate ~3-voxel step in this terrain's fBm noise, not a bug. Surface Nets never exposed this
// ambiguity because it pulls a shared vertex toward the crossing corner instead of placing it
// cleanly on the boundary. The fix: check height at a point stepped one unit along -normal (back
// into the solid interior) instead of at the vertex's own coordinate -- for a TOP face (normal is
// vertical, no horizontal component) this is the exact same point, so the fix changes nothing for
// the top-face case the tolerance was originally tuned against.
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
                        // Step back along -normal into the solid interior before sampling height --
                        // see the class comment above for why the vertex's own (boundary) position
                        // is the wrong column to check for a side face.
                        const float surface = generator.height_at(wx - v.normal.x, wz - v.normal.z);
                        // 2.5 voxels of tolerance: neither banding nor a single legitimate riser
                        // puts a real surface vertex that far above its own SOURCE column's height.
                        allFloat = wy > surface + 2.5f && wy > 2.5f; // (and not a water surface)
                    }
                    if (allFloat) {
                        if (floating < 8) {
                            const Vertex& v = mesh.vertices[mesh.indices[i]];
                            std::printf("floating tri in chunk[%d,%d,%d] at local (%.1f,%.1f,%.1f) mat=%d "
                                        "normal(%.0f,%.0f,%.0f)\n",
                                        coord.x, coord.y, coord.z, static_cast<double>(v.position.x),
                                        static_cast<double>(v.position.y), static_cast<double>(v.position.z),
                                        static_cast<int>(v.material), static_cast<double>(v.normal.x),
                                        static_cast<double>(v.normal.y), static_cast<double>(v.normal.z));
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
