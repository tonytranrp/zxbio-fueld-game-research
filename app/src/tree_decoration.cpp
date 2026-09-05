#include "tree_decoration.hpp"

#include <array>
#include <cmath>

#include "world/chunk/chunk_voxels.hpp" // kChunkSize

namespace app {

namespace {

using world::chunk::kChunkSize;
using world::generation::CanopyLobe;
using world::generation::kMaxCanopyLobes;
using world::generation::TrunkBox;
using world::meshing::MeshData;
using world::meshing::Vertex;

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

// Octahedron with independent horizontal/vertical half-extents (rh, rv) -- the one canopy
// primitive every silhouette variant composes from (goal 36: shape variety, same primitive kit).
void push_octahedron(MeshData& mesh, const glm::vec3& center, float rh, float rv,
                     world::chunk::MaterialID material) {
    const glm::vec3 top = center + glm::vec3{0.0f, rv, 0.0f};
    const glm::vec3 bottom = center - glm::vec3{0.0f, rv, 0.0f};
    const glm::vec3 px = center + glm::vec3{rh, 0.0f, 0.0f};
    const glm::vec3 nx = center - glm::vec3{rh, 0.0f, 0.0f};
    const glm::vec3 pz = center + glm::vec3{0.0f, 0.0f, rh};
    const glm::vec3 nz = center - glm::vec3{0.0f, 0.0f, rh};
    push_triangle(mesh, top, px, pz, material);
    push_triangle(mesh, top, pz, nx, material);
    push_triangle(mesh, top, nx, nz, material);
    push_triangle(mesh, top, nz, px, material);
    push_triangle(mesh, bottom, pz, px, material);
    push_triangle(mesh, bottom, nx, pz, material);
    push_triangle(mesh, bottom, nz, nx, material);
    push_triangle(mesh, bottom, px, nz, material);
}

void push_trunk(MeshData& mesh, float lx, float lz, float y0, float y1, float halfWidth) {
    const glm::vec3 t00{lx - halfWidth, y0, lz - halfWidth};
    const glm::vec3 t10{lx + halfWidth, y0, lz - halfWidth};
    const glm::vec3 t11{lx + halfWidth, y0, lz + halfWidth};
    const glm::vec3 t01{lx - halfWidth, y0, lz + halfWidth};
    const glm::vec3 u00{t00.x, y1, t00.z};
    const glm::vec3 u10{t10.x, y1, t10.z};
    const glm::vec3 u11{t11.x, y1, t11.z};
    const glm::vec3 u01{t01.x, y1, t01.z};
    push_quad(mesh, t00, t10, u10, u00, world::chunk::MaterialID::Wood); // -Z face
    push_quad(mesh, t11, t01, u01, u11, world::chunk::MaterialID::Wood); // +Z face
    push_quad(mesh, t10, t11, u11, u10, world::chunk::MaterialID::Wood); // +X face
    push_quad(mesh, t01, t00, u00, u01, world::chunk::MaterialID::Wood); // -X face
}

} // namespace

TreeEmitCounts append_tree_meshes(MeshData& mesh, world::chunk::ChunkCoord chunk, int seed,
                                  const world::generation::HeightmapGenerator& heightmap) {
    const float chunkBaseY = static_cast<float>(chunk.y * kChunkSize);
    const glm::vec3 chunkOrigin{static_cast<float>(chunk.x * kChunkSize), chunkBaseY,
                                static_cast<float>(chunk.z * kChunkSize)};
    TreeEmitCounts emitted;
    for (const TreePlacement& tree : compute_tree_placements(chunk.x, chunk.z, seed, heightmap)) {
        // Owned by the chunk whose Y range contains the base surface; skipped if the tree would
        // poke through this chunk's local ceiling (v1 simplification, deterministic).
        const float localBaseY = tree.base_height - chunkBaseY;
        // Ceiling check uses each shape's REAL top extent (the conifer's stacked canopy reaches
        // 2.55x its radius above the trunk -- a plain 2x formula would let it clip the chunk top).
        const float canopyRise =
            tree.shape == TreeShape::Conifer ? 2.55f * tree.canopy_radius : 2.0f * tree.canopy_radius;
        const float topY = localBaseY + tree.trunk_height + canopyRise;
        if (localBaseY < 0.0f || localBaseY >= static_cast<float>(kChunkSize) ||
            topY > static_cast<float>(kChunkSize)) {
            continue;
        }
        const float lx = tree.world_x - chunkOrigin.x;
        const float lz = tree.world_z - chunkOrigin.z;

        const std::size_t treeVertexStart = mesh.vertices.size();
        // The SAME implicit shapes the sparse-brick-octree voxelizer samples
        // (world/generation/tree_placement.hpp) -- one geometry definition, two consumers.
        TrunkBox trunk;
        if (world::generation::tree_trunk(tree, trunk)) {
            push_trunk(mesh, lx, lz, trunk.y0 - chunkBaseY, trunk.y1 - chunkBaseY, trunk.half_width);
        }
        std::array<CanopyLobe, kMaxCanopyLobes> lobes{};
        const std::size_t lobeCount = world::generation::tree_canopy_lobes(tree, lobes);
        for (std::size_t i = 0; i < lobeCount; ++i) {
            push_octahedron(mesh, lobes[i].center - chunkOrigin, lobes[i].rh, lobes[i].rv,
                            world::chunk::MaterialID::Leaves);
        }

        // Goal 38: per-tree brightness jitter through the (otherwise-unused-for-trees) AO
        // attribute, applied to every vertex this tree just emitted.
        for (std::size_t i = treeVertexStart; i < mesh.vertices.size(); ++i) {
            mesh.vertices[i].ao = tree.color_jitter;
        }
        switch (tree.shape) {
        case TreeShape::Round:
            ++emitted.round;
            break;
        case TreeShape::Conifer:
            ++emitted.conifer;
            break;
        case TreeShape::Shrub:
            ++emitted.shrub;
            break;
        }
    }
    return emitted;
}

} // namespace app
