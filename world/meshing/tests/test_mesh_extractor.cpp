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
// Signed volume x6 of a closed triangle mesh (divergence theorem: sum of a·(b x c) over every
// triangle, relative to any common origin). Positive iff the mesh is a consistently outward-
// oriented closed surface -- robust to exactly where Naive Surface Nets actually places each
// vertex (which is pulled toward the crossing corner within a cell, not its geometric center, so
// a per-quad "is this quad's center on the expected side of the voxel" heuristic is NOT reliable
// -- confirmed by hand: it produces false failures on this exact test case even when the winding
// is correct). This is the standard, position-independent way to check mesh orientation.
float signed_volume_x6(const MeshData& mesh) {
    float total = 0.0f;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const glm::vec3& a = mesh.vertices[mesh.indices[i]].position;
        const glm::vec3& b = mesh.vertices[mesh.indices[i + 1]].position;
        const glm::vec3& c = mesh.vertices[mesh.indices[i + 2]].position;
        total += glm::dot(a, glm::cross(b, c));
    }
    return total;
}
} // namespace

TEST_CASE("A single solid voxel surrounded by air produces exactly 6 outward-facing quads", "[meshing]") {
    ChunkStore store;
    const ChunkCoord coord{0, 0, 0};
    Chunk& chunk = store.get_or_create(coord);
    chunk.voxels().set(local_index(16, 16, 16), MaterialID::Stone);

    const MeshData mesh = extract_mesh(store, coord);

    // 8 cells touch the solid voxel as one of their 8 corners; each of those 8 is active (exactly
    // one differing corner among its own 8). 6 grid edges actually cross (voxel-to-solid-voxel to
    // each of its 6 air face-neighbors); each crossing edge emits exactly one quad.
    REQUIRE(mesh.vertices.size() == 8);
    REQUIRE(mesh.indices.size() == 6 * 6); // 6 quads * 2 triangles * 3 indices

    // Every vertex must lie strictly inside the solid voxel's own [16,17]^3 cell (Naive Surface
    // Nets places each vertex within the cell it belongs to) -- a real, if loose, sanity check on
    // vertex placement independent of the orientation check below.
    for (const auto& v : mesh.vertices) {
        REQUIRE(v.position.x >= 15.0f);
        REQUIRE(v.position.x <= 17.0f);
    }

    REQUIRE(signed_volume_x6(mesh) > 0.0f);
}

TEST_CASE("Adjacent chunks produce identical world-space vertex positions along their shared face", "[meshing]") {
    // Two solid voxels straddling the X=31/X=32 world boundary between chunk (0,0,0) and (1,0,0):
    // world (31,16,16) and (32,16,16), everything else air. The cell anchored at world-cell (31,
    // 16,16) is computed independently by BOTH chunks -- as chunk0's own owned cell at local
    // cx=31, and as chunk1's boundary-layer cell at local cx=-1 -- and must resolve to the exact
    // same world position in both meshes for the seam to be crack-free.
    ChunkStore store;
    const ChunkCoord coord0{0, 0, 0};
    const ChunkCoord coord1{1, 0, 0};
    store.get_or_create(coord0).voxels().set(local_index(31, 16, 16), MaterialID::Stone);
    store.get_or_create(coord1).voxels().set(local_index(0, 16, 16), MaterialID::Stone);

    const MeshData mesh0 = extract_mesh(store, coord0);
    const MeshData mesh1 = extract_mesh(store, coord1);

    REQUIRE_FALSE(mesh0.vertices.empty());
    REQUIRE_FALSE(mesh1.vertices.empty());

    // Find chunk0's vertex closest to its own high-X boundary (local x near 31) and chunk1's
    // vertex closest to its own low-X boundary (local x near -1); both convert to world X near 31.
    const Vertex* boundary0 = nullptr;
    for (const auto& v : mesh0.vertices) {
        if (v.position.x > 30.5f && (boundary0 == nullptr || v.position.x > boundary0->position.x)) {
            boundary0 = &v;
        }
    }
    const Vertex* boundary1 = nullptr;
    for (const auto& v : mesh1.vertices) {
        // Any negative local X is unambiguously in chunk1's -1 boundary layer -- the exact
        // fractional offset within that cell depends on which of its edges cross (not just the
        // X-direction one), so this must not assume the offset lands past any particular point
        // short of the cell's own [-1, 0) span.
        if (v.position.x < 0.0f && (boundary1 == nullptr || v.position.x < boundary1->position.x)) {
            boundary1 = &v;
        }
    }

    REQUIRE(boundary0 != nullptr);
    REQUIRE(boundary1 != nullptr);

    const glm::vec3 world0 = glm::vec3(coord0.x, coord0.y, coord0.z) * static_cast<float>(kChunkSize) + boundary0->position;
    const glm::vec3 world1 = glm::vec3(coord1.x, coord1.y, coord1.z) * static_cast<float>(kChunkSize) + boundary1->position;

    constexpr float kEps = 1e-5f;
    REQUIRE(std::abs(world0.x - world1.x) < kEps);
    REQUIRE(std::abs(world0.y - world1.y) < kEps);
    REQUIRE(std::abs(world0.z - world1.z) < kEps);
}

TEST_CASE("A chunk with no registered neighbors that is entirely air produces an empty mesh", "[meshing]") {
    ChunkStore store;
    const ChunkCoord coord{5, 5, 5};
    store.get_or_create(coord); // default-constructed: homogeneous Air

    const MeshData mesh = extract_mesh(store, coord);

    REQUIRE(mesh.vertices.empty());
    REQUIRE(mesh.indices.empty());
}

TEST_CASE("A solid chunk surrounded entirely by solid neighbors produces an empty mesh (no false void surface)",
          "[meshing]") {
    // Fill the center chunk and all 26 neighbors solid, so there is no actual air/solid interface
    // anywhere near the center chunk's own cells or padding -- isolating "uniform interior produces
    // no surface" from the (separately correct, not a bug) case of a lone solid chunk floating in
    // an air void, which legitimately DOES produce a visible outer shell.
    ChunkStore store;
    const ChunkCoord center{0, 0, 0};
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                store.get_or_create(ChunkCoord{center.x + dx, center.y + dy, center.z + dz}).voxels().fill_uniform(MaterialID::Stone);
            }
        }
    }

    const MeshData mesh = extract_mesh(store, center);

    REQUIRE(mesh.vertices.empty());
    REQUIRE(mesh.indices.empty());
}

TEST_CASE("A chunk straddling an air/water interface assigns Water material to its surface", "[meshing]") {
    ChunkStore store;
    const ChunkCoord coord{0, 0, 0};
    Chunk& chunk = store.get_or_create(coord);
    for (std::int32_t z = 0; z < kChunkSize; ++z) {
        for (std::int32_t x = 0; x < kChunkSize; ++x) {
            for (std::int32_t y = 0; y < 16; ++y) {
                chunk.voxels().set(local_index(x, y, z), MaterialID::Water);
            }
        }
    }

    const MeshData mesh = extract_mesh(store, coord);

    REQUIRE_FALSE(mesh.vertices.empty());
    for (const auto& v : mesh.vertices) {
        REQUIRE(v.material == MaterialID::Water);
    }
}
