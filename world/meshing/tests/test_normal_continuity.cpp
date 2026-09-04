#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"

// TERRAIN_FIXES_BRIEF Group R task 10: with task 8 having cleared the raw noise (smooth at the
// intended scale), the corrugation suspect moves to the mesh's normals. On smooth fBm terrain,
// adjacent vertices sit ~1 voxel apart on a surface whose curvature radius is tens of voxels --
// neighboring normals should differ by a few degrees. A normal-averaging bug (averaging only a
// subset of a vertex's adjacent faces) produces alternating tilts: adjacent normals zigzagging by
// tens of degrees, which Lambertian shading renders as exactly the observed tight corrugation.
TEST_CASE("adjacent mesh normals vary smoothly on real terrain", "[meshing][normals]") {
    const world::generation::HeightmapGenerator generator(1337);
    world::chunk::ChunkStore store;
    const world::chunk::ChunkCoord center{0, 0, 0}; // y=0 crosses the surface near the origin
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                world::generation::fill_terrain(
                    store.get_or_create({center.x + dx, center.y + dy, center.z + dz}), generator);
            }
        }
    }
    const auto mesh = world::meshing::extract_mesh(store, center);
    REQUIRE(mesh.indices.size() >= 3);

    float maxAngle = 0.0f;
    double sumAngle = 0.0;
    std::size_t edges = 0;
    const auto angleBetween = [&](std::uint32_t a, std::uint32_t b) {
        const float d = std::clamp(glm::dot(mesh.vertices[a].normal, mesh.vertices[b].normal), -1.0f, 1.0f);
        const float angle = std::acos(d) * 180.0f / std::numbers::pi_v<float>;
        maxAngle = std::max(maxAngle, angle);
        sumAngle += angle;
        ++edges;
    };
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        angleBetween(mesh.indices[i], mesh.indices[i + 1]);
        angleBetween(mesh.indices[i + 1], mesh.indices[i + 2]);
        angleBetween(mesh.indices[i + 2], mesh.indices[i]);
    }
    std::printf("normal continuity over %zu mesh edges: max adjacent angle %.1f deg, mean %.2f deg\n", edges,
                static_cast<double>(maxAngle), sumAngle / static_cast<double>(edges));

    // Stated before running: smooth terrain at this feature scale should keep the MEAN adjacent
    // angle in the single digits; a subset-averaging bug flips alternate normals by tens of
    // degrees and drags the mean up with it. The max is looser (cliff cells exist legitimately).
    CHECK(sumAngle / static_cast<double>(edges) < 10.0);
    CHECK(maxAngle < 90.0f);
}
