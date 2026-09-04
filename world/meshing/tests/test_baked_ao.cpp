#include <cmath>
#include <cstdint>

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

// The mesh vertex nearest a given chunk-local point.
const Vertex& nearest_vertex(const MeshData& mesh, glm::vec3 p) {
    REQUIRE(!mesh.vertices.empty());
    std::size_t best = 0;
    float bestD = 1e30f;
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const glm::vec3 d = mesh.vertices[i].position - p;
        const float dist = d.x * d.x + d.y * d.y + d.z * d.z;
        if (dist < bestD) {
            bestD = dist;
            best = i;
        }
    }
    return mesh.vertices[best];
}

} // namespace

// research/baked-ao-design.md's mapping table, asserted on hand-constructed geometry: only the
// concave half of the solid-corner range darkens, in the exact 4 discrete levels.
TEST_CASE("baked AO maps enclosure to the design table's levels", "[meshing][ao]") {
    ChunkStore store;
    create_halo(store, {0, 0, 0});
    Chunk& chunk = store.get_or_create({0, 0, 0});

    // A flat floor: y <= 7 solid across the whole chunk interior (away from chunk edges so the
    // probe points are far from any halo effects).
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

    // Flat floor far from both walls: surface cells have exactly 4 solid corners -> ao == 1.0.
    const Vertex& flat = nearest_vertex(mesh, {6.0f, 8.0f, 20.0f});
    CHECK(flat.ao == 1.0f);

    // Base of a single wall (floor+wall crease, 6 solid corners) -> 0.70 level.
    const Vertex& crease = nearest_vertex(mesh, {19.0f, 8.0f, 20.0f});
    CHECK(crease.ao < flat.ao);

    // The pit corner where floor and BOTH walls meet (7 solid corners) -> darkest level 0.55.
    const Vertex& pit = nearest_vertex(mesh, {19.0f, 8.0f, 11.0f});
    CHECK(pit.ao <= crease.ao);
    CHECK(std::abs(pit.ao - 0.55f) < 1e-5f);

    // Monotonicity across the three: pit <= crease < flat (goal 12's "darkest of the levels at a
    // concave corner" check, generalized).
    CHECK(pit.ao <= crease.ao);
    CHECK(crease.ao < flat.ao);

    // A convex top edge of the wall must NOT darken (convex clamps to 1.0 by design).
    const Vertex& ridge = nearest_vertex(mesh, {20.0f, 15.0f, 20.0f});
    CHECK(ridge.ao == 1.0f);
}
