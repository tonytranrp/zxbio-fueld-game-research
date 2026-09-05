#include <algorithm>
#include <array>
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
// Signed volume x6 of a closed triangle mesh (divergence theorem: sum of a·(b x c) over every
// triangle, relative to any common origin). Positive iff the mesh is a consistently outward-
// oriented closed surface -- algorithm-independent, so this carries over unchanged from Naive
// Surface Nets to Group Q's blocky meshing as a real cross-check on the winding derivation.
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

// Blocky meshing (Group Q) gives every face its own 4 unshared vertices, always appended as one
// contiguous group of 4 sharing one flat normal -- unlike Surface Nets, which shared vertices
// across quads and needed normal averaging. Grouping by 4 lets tests reason per-face directly.
std::vector<glm::vec3> face_normals(const MeshData& mesh) {
    std::vector<glm::vec3> normals;
    for (std::size_t i = 0; i + 3 < mesh.vertices.size(); i += 4) {
        normals.push_back(mesh.vertices[i].normal);
    }
    return normals;
}
} // namespace

TEST_CASE("A single solid voxel surrounded by air produces exactly 6 outward-facing quads", "[meshing]") {
    ChunkStore store;
    const ChunkCoord coord{0, 0, 0};
    Chunk& chunk = store.get_or_create(coord);
    chunk.voxels().set(local_index(16, 16, 16), MaterialID::Stone);

    const MeshData mesh = extract_mesh(store, coord);

    // Blocky meshing gives every face its own 4 fresh vertices -- no sharing even between two
    // faces of the SAME voxel (that is what makes the normal exact per face instead of averaged;
    // see mesh_extractor.cpp's emit_quad comment). 6 faces * 4 = 24 vertices, 6 * 2 tris * 3 = 36
    // indices (the index count happens to match the old Surface-Nets count for this exact case;
    // the vertex count does not, and that difference is the point of the rewrite, not a bug).
    REQUIRE(mesh.vertices.size() == 24);
    REQUIRE(mesh.indices.size() == 36);

    // Every face sits exactly on the solid voxel's own [16,17]^3 boundary -- blocky faces are
    // placed at exact voxel boundaries, not pulled toward a crossing corner like Surface Nets, so
    // this can be an EXACT check now rather than a loose bound.
    for (const auto& v : mesh.vertices) {
        CHECK((v.position.x == 16.0f || v.position.x == 17.0f));
        CHECK((v.position.y == 16.0f || v.position.y == 17.0f));
        CHECK((v.position.z == 16.0f || v.position.z == 17.0f));
    }

    // Goal 117's real check: exactly the 6 cardinal directions appear, each exactly once, and each
    // face's STORED normal agrees with the normal actually implied by its own emitted positions
    // (cross product of two real edges) -- not just assumed consistent because the mesh compiles.
    const auto normals = face_normals(mesh);
    REQUIRE(normals.size() == 6);
    for (const glm::vec3 expected : {glm::vec3{1, 0, 0}, glm::vec3{-1, 0, 0}, glm::vec3{0, 1, 0},
                                     glm::vec3{0, -1, 0}, glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}}) {
        CHECK(std::count(normals.begin(), normals.end(), expected) == 1);
    }
    for (std::size_t f = 0; f + 3 < mesh.vertices.size(); f += 4) {
        const glm::vec3& p0 = mesh.vertices[f].position;
        const glm::vec3& p1 = mesh.vertices[f + 1].position;
        const glm::vec3& p2 = mesh.vertices[f + 2].position;
        const glm::vec3 impliedNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        CHECK(glm::length(impliedNormal - mesh.vertices[f].normal) < 1e-5f);
    }

    // Whole-mesh cross-check: a real closed, consistently outward-oriented cube has positive
    // signed volume regardless of which specific algorithm produced it.
    REQUIRE(signed_volume_x6(mesh) > 0.0f);
}

TEST_CASE("Two solid voxels facing each other across a chunk boundary block each other's shared face",
          "[meshing]") {
    // Two solid voxels straddling the X=31/X=32 world boundary between chunk (0,0,0) and (1,0,0),
    // everything else air. Under blocky meshing a voxel is owned by exactly the one chunk storing
    // it (no shared dual vertex ever computed by two chunks the way Surface Nets needed), so the
    // real cross-chunk correctness property is different from the old "positions must match" test:
    // the two occupied voxels mutually block each other's shared +X/-X face, and NEITHER chunk
    // should emit it -- not both (a z-fighting double face) and not neither's OTHER 5 real faces
    // (a hole). An isolated voxel emits 6 faces (previous test); exactly 5 here is exactly that one
    // shared face missing and nothing else disturbed.
    ChunkStore store;
    const ChunkCoord coord0{0, 0, 0};
    const ChunkCoord coord1{1, 0, 0};
    store.get_or_create(coord0).voxels().set(local_index(31, 16, 16), MaterialID::Stone);
    store.get_or_create(coord1).voxels().set(local_index(0, 16, 16), MaterialID::Stone);

    const MeshData mesh0 = extract_mesh(store, coord0);
    const MeshData mesh1 = extract_mesh(store, coord1);

    CHECK(mesh0.vertices.size() == 5 * 4);
    CHECK(mesh0.indices.size() == 5 * 6);
    CHECK(mesh1.vertices.size() == 5 * 4);
    CHECK(mesh1.indices.size() == 5 * 6);

    const auto normals0 = face_normals(mesh0);
    const auto normals1 = face_normals(mesh1);
    CHECK(std::find(normals0.begin(), normals0.end(), glm::vec3{1, 0, 0}) == normals0.end());
    CHECK(std::find(normals1.begin(), normals1.end(), glm::vec3{-1, 0, 0}) == normals1.end());
}

TEST_CASE("A chunk with no registered neighbors that is entirely air produces an empty mesh", "[meshing]") {
    ChunkStore store;
    const ChunkCoord coord{5, 5, 5};
    store.get_or_create(coord); // default-constructed: homogeneous Air

    const MeshData mesh = extract_mesh(store, coord);

    REQUIRE(mesh.vertices.empty());
    REQUIRE(mesh.indices.empty());
}

TEST_CASE(
    "A solid chunk surrounded entirely by solid neighbors produces an empty mesh (no false void surface)",
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
                store.get_or_create(ChunkCoord{center.x + dx, center.y + dy, center.z + dz})
                    .voxels()
                    .fill_uniform(MaterialID::Stone);
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
