#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/chunk/chunk_voxels.hpp"
#include "world/meshing/mesh_extractor.hpp"

using namespace world::chunk;
using namespace world::meshing;

namespace {

// Ensures the 26-neighbor halo exists (empty/air) so extraction preconditions hold.
void create_halo(ChunkStore& store, ChunkCoord base) {
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                store.get_or_create(ChunkCoord{base.x + dx, base.y + dy, base.z + dz});
            }
        }
    }
}

void set_solid(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    chunk.voxels().set(local_index(x, y, z), MaterialID::Stone);
}

// Group Q note (research/voxel-representation-redesign.md SS2): greedy meshing merges a flat,
// unoccluded run into very few large quads, so vertices only reliably exist at real geometric
// FEATURES (an AO discontinuity forces a merge boundary there) -- not at arbitrary interior points
// of an open area, which the pre-Group-Q "nearest vertex to point P" probing style relied on. These
// tests instead check bounding regions around real features (generous enough to tolerate not
// knowing the exact merged-quad shape) and the mesh's overall set of distinct AO values.
struct Box {
    glm::vec3 min;
    glm::vec3 max;
};

bool any_vertex_in(const MeshData& mesh, Box box, float expectedAo, float eps = 1e-5f) {
    for (const auto& v : mesh.vertices) {
        if (v.position.x >= box.min.x && v.position.x <= box.max.x && v.position.y >= box.min.y &&
            v.position.y <= box.max.y && v.position.z >= box.min.z && v.position.z <= box.max.z &&
            std::abs(v.ao - expectedAo) < eps) {
            return true;
        }
    }
    return false;
}

} // namespace

// research/baked-ao-design.md's mapping table, now genuinely per-face-corner (goal 10's original
// design, only approximated before Group Q's blocky-meshing rewrite made unshared per-face
// vertices the natural representation -- see corner_ao's own comment in mesh_extractor.cpp).
TEST_CASE("baked AO maps enclosure to the design table's levels", "[meshing][ao]") {
    ChunkStore store;
    create_halo(store, {0, 0, 0});
    Chunk& chunk = store.get_or_create({0, 0, 0});

    // A flat floor: y <= 7 solid across the whole chunk interior.
    for (std::int32_t z = 0; z < kChunkSize; ++z) {
        for (std::int32_t x = 0; x < kChunkSize; ++x) {
            for (std::int32_t y = 0; y <= 7; ++y) {
                set_solid(chunk, x, y, z);
            }
        }
    }
    // A wall along x == 20 (rising from the floor): makes a two-plane crease at its base.
    for (std::int32_t z = 4; z < kChunkSize - 4; ++z) {
        for (std::int32_t y = 8; y <= 14; ++y) {
            set_solid(chunk, 20, y, z);
        }
    }
    // A second wall along z == 10 meeting the first: their intersection base is a three-plane
    // concave pit corner (floor + two walls).
    for (std::int32_t x = 12; x <= 20; ++x) {
        for (std::int32_t y = 8; y <= 14; ++y) {
            set_solid(chunk, x, y, 10);
        }
    }

    const MeshData mesh = extract_mesh(store, {0, 0, 0});
    REQUIRE(!mesh.vertices.empty());

    // A merge-shape-independent check: every AO value that appears anywhere is one of the 4 valid
    // design-table levels (never some other, formula-broken number), and both the darkest and the
    // brightest are genuinely reached by this scene (the pit corner and the open floor respectively
    // -- confirmed spatially below). Not asserting the exact SET size: which of the two middle
    // levels a given wall/corner configuration lands on depends on merge-shape details not worth
    // hand-predicting here, but the formula's valid output range is worth asserting precisely.
    std::vector<float> levels;
    for (const auto& v : mesh.vertices) {
        if (std::find_if(levels.begin(), levels.end(), [&](float l) { return std::abs(l - v.ao) < 1e-5f; }) ==
            levels.end()) {
            levels.push_back(v.ao);
        }
    }
    std::sort(levels.begin(), levels.end());
    REQUIRE(levels.size() >= 2);
    for (float level : levels) {
        const bool isValidDesignLevel = std::abs(level - 0.55f) < 1e-5f || std::abs(level - 0.70f) < 1e-5f ||
                                        std::abs(level - 0.85f) < 1e-5f || std::abs(level - 1.0f) < 1e-5f;
        CHECK(isValidDesignLevel);
    }
    CHECK(std::abs(levels.front() - 0.55f) < 1e-5f); // darkest reached: the 3-plane pit corner
    CHECK(std::abs(levels.back() - 1.0f) < 1e-5f);   // brightest reached: flat, unoccluded ground

    // The darkest level must appear specifically near the real pit corner (floor + both walls
    // meet, at local (20,8,10)) -- a generous box around it, since AO's neighbor reach is only
    // ~1-2 voxels but the exact merged-quad shape there is not worth predicting precisely.
    CHECK(any_vertex_in(mesh, {{15.0f, 6.0f, 5.0f}, {25.0f, 10.0f, 15.0f}}, 0.55f));

    // A convex top edge of wall1 must NOT darken. Group Q note: greedy merging can fold wall1's
    // whole clean top strip into one big quad whose only vertices sit at its far ends -- span the
    // box across the wall's FULL length (not a hand-picked "away from wall2" interior slice) so it
    // catches whatever corners a merge actually produced, wherever they land.
    CHECK(any_vertex_in(mesh, {{19.0f, 14.0f, 0.0f}, {22.0f, 16.0f, 32.0f}}, 1.0f));
}

// Goal 59's boundary coverage for the water-depth encoding (the AO byte's second meaning): hand-
// built pools of known depth must produce exactly depth/8, capped at 8 voxels. water_depth_ao is a
// pure per-column vertical scan with zero lateral spread (unlike corner AO), so the deep/shallow
// boundary is an exact, unsmoothed voxel-column cutover -- probing immediately either side of it is
// safe regardless of how greedy merging shaped the rest of each pool.
TEST_CASE("water surface vertices carry column depth in the AO attribute", "[meshing][ao][water]") {
    ChunkStore store;
    create_halo(store, {0, 0, 0});
    Chunk& chunk = store.get_or_create({0, 0, 0});

    // Floor everywhere at y <= 2; a DEEP pool (water y 3..10, 8 deep) on one side and a SHALLOW
    // pool (water y 9..10, 2 deep on a raised shelf y <= 8) on the other, both surfacing at y=10.
    for (std::int32_t z = 0; z < kChunkSize; ++z) {
        for (std::int32_t x = 0; x < kChunkSize; ++x) {
            const bool shallowSide = x >= 16;
            const std::int32_t floorTop = shallowSide ? 8 : 2;
            for (std::int32_t y = 0; y <= floorTop; ++y) {
                set_solid(chunk, x, y, z);
            }
            for (std::int32_t y = floorTop + 1; y <= 10; ++y) {
                chunk.voxels().set(local_index(x, y, z), MaterialID::Water);
            }
        }
    }

    const MeshData mesh = extract_mesh(store, {0, 0, 0});
    REQUIRE(!mesh.vertices.empty());
    // Note: this scene's floor is also exposed on its bottom/sides against the (generated, empty)
    // halo chunks -- real Stone faces there, same as under the pre-Group-Q algorithm's identical
    // occupancy rule, so this test only checks the water TOP surface specifically, not every vertex.

    // Group Q note: each depth half is uniform, so greedy merging folds it into one giant quad
    // whose only vertices are its own 4 outer corners -- both pools' shared x=16 seam is one such
    // corner for BOTH quads (a real depth discontinuity there forces the merge to stop on both
    // sides), so probing right at that seam is safe regardless of how each pool's far corners land.
    CHECK(any_vertex_in(mesh, {{15.0f, 10.5f, -0.5f}, {17.0f, 11.5f, 0.5f}}, 1.0f)); // deep: capped at 8/8
    CHECK(any_vertex_in(mesh, {{15.0f, 10.5f, -0.5f}, {17.0f, 11.5f, 0.5f}}, 2.0f / 8.0f)); // shallow: 2/8
}
