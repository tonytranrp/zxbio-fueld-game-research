#include "tree_decoration.hpp"

#include <cmath>

#include "world/chunk/chunk_voxels.hpp" // kChunkSize

namespace app {

namespace {

using world::chunk::kChunkSize;
using world::meshing::MeshData;
using world::meshing::Vertex;

// splitmix64: a well-mixed, dependency-free hash for deterministic placement keys.
std::uint64_t mix(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

std::uint64_t placement_key(int seed, std::int32_t cx, std::int32_t cz, std::int32_t gx, std::int32_t gz) {
    std::uint64_t k = static_cast<std::uint32_t>(seed);
    k = mix(k ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32 | static_cast<std::uint32_t>(cz)));
    k = mix(k ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(gx)) << 32 | static_cast<std::uint32_t>(gz)));
    return k;
}

// One flat-shaded triangle: three duplicated vertices sharing the face normal (trees are meant
// to read as faceted primitives; flat normals also sidestep any averaging across the two
// materials).
void push_triangle(MeshData& mesh, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                   world::chunk::MaterialID material) {
    const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Vertex{a, normal, material});
    mesh.vertices.push_back(Vertex{b, normal, material});
    mesh.vertices.push_back(Vertex{c, normal, material});
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
}

void push_quad(MeshData& mesh, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
               world::chunk::MaterialID material) {
    push_triangle(mesh, a, b, c, material);
    push_triangle(mesh, a, c, d, material);
}

} // namespace

std::vector<TreePlacement> compute_tree_placements(std::int32_t chunkX, std::int32_t chunkZ, int seed,
                                                   const world::generation::HeightmapGenerator& heightmap) {
    std::vector<TreePlacement> placements;
    constexpr std::int32_t kCell = 8; // 4x4 candidate grid per 32-voxel column
    for (std::int32_t gz = 0; gz < kChunkSize / kCell; ++gz) {
        for (std::int32_t gx = 0; gx < kChunkSize / kCell; ++gx) {
            const std::uint64_t key = placement_key(seed, chunkX, chunkZ, gx, gz);
            if ((key & 0xFFu) >= 96u) { // ~37% of cells host a candidate (pre-mask)
                continue;
            }
            // Jitter within [2, cell-2): with cell size 8, adjacent cells' trees are always
            // >= kTreeMinSpacing (4) apart -- spacing holds by construction, tested anyway.
            const auto jx = static_cast<float>(2 + ((key >> 8) & 0xFFu) % 5u);
            const auto jz = static_cast<float>(2 + ((key >> 16) & 0xFFu) % 5u);
            const float worldX = static_cast<float>(chunkX * kChunkSize + gx * kCell) + jx;
            const float worldZ = static_cast<float>(chunkZ * kChunkSize + gz * kCell) + jz;

            const float h = heightmap.height_at(worldX, worldZ);
            if (h < kTreeMinHeight || h > kTreeMaxHeight) {
                continue; // no water/beach trees, no trees above the tree line
            }
            const float slopeX =
                std::abs(heightmap.height_at(worldX + 1.0f, worldZ) - heightmap.height_at(worldX - 1.0f, worldZ)) *
                0.5f;
            const float slopeZ =
                std::abs(heightmap.height_at(worldX, worldZ + 1.0f) - heightmap.height_at(worldX, worldZ - 1.0f)) *
                0.5f;
            if (std::max(slopeX, slopeZ) > kTreeMaxSlope) {
                continue; // too steep
            }

            TreePlacement tree;
            tree.world_x = worldX;
            tree.world_z = worldZ;
            tree.base_height = h;
            tree.trunk_height = 4.0f + static_cast<float>((key >> 24) & 0x3u); // 4..7
            tree.canopy_radius = 2.0f + static_cast<float>((key >> 26) & 0x1u); // 2..3
            placements.push_back(tree);
        }
    }
    return placements;
}

std::size_t append_tree_meshes(MeshData& mesh, world::chunk::ChunkCoord chunk, int seed,
                               const world::generation::HeightmapGenerator& heightmap) {
    const float chunkBaseY = static_cast<float>(chunk.y * kChunkSize);
    std::size_t emitted = 0;
    for (const TreePlacement& tree : compute_tree_placements(chunk.x, chunk.z, seed, heightmap)) {
        // Owned by the chunk whose Y range contains the base surface; skipped if the tree would
        // poke through this chunk's local ceiling (v1 simplification, deterministic).
        const float localBaseY = tree.base_height - chunkBaseY;
        const float topY = localBaseY + tree.trunk_height + 2.0f * tree.canopy_radius;
        if (localBaseY < 0.0f || localBaseY >= static_cast<float>(kChunkSize) ||
            topY > static_cast<float>(kChunkSize)) {
            continue;
        }
        const float lx = tree.world_x - static_cast<float>(chunk.x * kChunkSize);
        const float lz = tree.world_z - static_cast<float>(chunk.z * kChunkSize);

        // Trunk: a square prism, sunk half a voxel so it never floats above the surface mesh.
        constexpr float kTrunkHalfWidth = 0.35f;
        const float y0 = localBaseY - 0.5f;
        const float y1 = localBaseY + tree.trunk_height;
        const glm::vec3 t00{lx - kTrunkHalfWidth, y0, lz - kTrunkHalfWidth};
        const glm::vec3 t10{lx + kTrunkHalfWidth, y0, lz - kTrunkHalfWidth};
        const glm::vec3 t11{lx + kTrunkHalfWidth, y0, lz + kTrunkHalfWidth};
        const glm::vec3 t01{lx - kTrunkHalfWidth, y0, lz + kTrunkHalfWidth};
        const glm::vec3 u00{t00.x, y1, t00.z};
        const glm::vec3 u10{t10.x, y1, t10.z};
        const glm::vec3 u11{t11.x, y1, t11.z};
        const glm::vec3 u01{t01.x, y1, t01.z};
        push_quad(mesh, t00, t10, u10, u00, world::chunk::MaterialID::Wood); // -Z face
        push_quad(mesh, t11, t01, u01, u11, world::chunk::MaterialID::Wood); // +Z face
        push_quad(mesh, t10, t11, u11, u10, world::chunk::MaterialID::Wood); // +X face
        push_quad(mesh, t01, t00, u00, u01, world::chunk::MaterialID::Wood); // -X face

        // Canopy: an octahedron centered above the trunk.
        const float r = tree.canopy_radius;
        const glm::vec3 center{lx, y1 + r, lz};
        const glm::vec3 top = center + glm::vec3{0.0f, r, 0.0f};
        const glm::vec3 bottom = center - glm::vec3{0.0f, r, 0.0f};
        const glm::vec3 px = center + glm::vec3{r, 0.0f, 0.0f};
        const glm::vec3 nx = center - glm::vec3{r, 0.0f, 0.0f};
        const glm::vec3 pz = center + glm::vec3{0.0f, 0.0f, r};
        const glm::vec3 nz = center - glm::vec3{0.0f, 0.0f, r};
        push_triangle(mesh, top, px, pz, world::chunk::MaterialID::Leaves);
        push_triangle(mesh, top, pz, nx, world::chunk::MaterialID::Leaves);
        push_triangle(mesh, top, nx, nz, world::chunk::MaterialID::Leaves);
        push_triangle(mesh, top, nz, px, world::chunk::MaterialID::Leaves);
        push_triangle(mesh, bottom, pz, px, world::chunk::MaterialID::Leaves);
        push_triangle(mesh, bottom, nx, pz, world::chunk::MaterialID::Leaves);
        push_triangle(mesh, bottom, nz, nx, world::chunk::MaterialID::Leaves);
        push_triangle(mesh, bottom, px, nz, world::chunk::MaterialID::Leaves);
        ++emitted;
    }
    return emitted;
}

} // namespace app
