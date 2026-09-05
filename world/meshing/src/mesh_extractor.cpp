#include "world/meshing/mesh_extractor.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "world/chunk/block_type.hpp"

namespace world::meshing {

using world::chunk::Chunk;
using world::chunk::ChunkCoord;
using world::chunk::ChunkStore;
using world::chunk::is_occupied;
using world::chunk::kChunkSize;
using world::chunk::local_index;
using world::chunk::MaterialID;
using world::chunk::properties_of;
using world::chunk::world_to_local;

// Padded local-space sampling: lx/ly/lz may range one voxel past this chunk's own 0..31 bounds in
// any direction, so the owning chunk per axis is a plain sign/range test into the already-resolved
// 3x3x3 pointer cache (NeighborCache, mesh_extractor.hpp). The per-axis mask (world_to_local on
// the padded coordinate directly: -1 & 31 == 31, 32 & 31 == 0) is the same cross-chunk math as
// before -- face-, edge-, and corner-adjacent neighbors all resolve with no per-direction
// special-casing. Unchanged by the Group Q blocky-meshing rewrite -- still exactly the sampling
// primitive both the old Surface-Nets algorithm and the new per-face algorithm need.
MaterialID NeighborCache::sample(std::int32_t lx, std::int32_t ly, std::int32_t lz) const {
    const std::int32_t dx = lx < 0 ? -1 : (lx >= kChunkSize ? 1 : 0);
    const std::int32_t dy = ly < 0 ? -1 : (ly >= kChunkSize ? 1 : 0);
    const std::int32_t dz = lz < 0 ? -1 : (lz >= kChunkSize ? 1 : 0);
    const Chunk* chunk = chunks_[slot(dx, dy, dz)];
    if (chunk == nullptr) {
        // The caller is responsible for ensuring all 26 neighbors are generated first (see the
        // header's precondition note) -- this is a safety net against a crash, not the mechanism
        // that guarantees seamless geometry.
        return MaterialID::Air;
    }
    return chunk->voxels().at(local_index(world_to_local(lx), world_to_local(ly), world_to_local(lz)));
}

namespace {

// Group Q (research/voxel-representation-redesign.md SS2): per-voxel-face emission with greedy
// merging, replacing Naive Surface Nets. A voxel is unambiguously owned by exactly one chunk (its
// own storage), so -- unlike Surface Nets' shared dual-cell vertices, which needed the "-1 boundary
// layer computed twice, positions must match" scheme -- a face's ownership is trivial: whichever
// chunk's local range contains the OCCUPIED voxel on one side of an occupied/unoccupied boundary
// emits that face. Neither chunk emits anything at a boundary where both sides are occupied (a
// solid-solid or solid-water interior boundary is never visible -- this matches the OLD algorithm's
// own behavior exactly, since its crossing test was also occupied-vs-occupied-insensitive; water
// stays deliberately opaque with no meshed lakebed underneath, per the goal-29 decision).
//
// One row of the 2D "mask" swept along one axis+direction: which material (Air = none) has an
// exposed face at this (u,v) cell, and that face's baked lighting at each of its 4 corners. Two
// adjacent cells merge into one bigger quad only when BOTH match exactly -- adjacent cells on a
// flat, unoccluded run always compute identical corner values (each is a pure function of local
// occupancy, and physically-shared corners are recomputed identically on both sides), so this
// naturally merges maximally over open ground and stops exactly at a genuine visual discontinuity
// (an edge, a slope, a depth change in water) rather than needing separate AO-aware bookkeeping.
struct MaskCell {
    MaterialID material = MaterialID::Air;               // Air is the "no face here" sentinel
    std::array<float, 4> corner{1.0f, 1.0f, 1.0f, 1.0f}; // order: (u0,v0),(u1,v0),(u1,v1),(u0,v1)

    [[nodiscard]] bool mergeable_with(const MaskCell& other) const {
        return material != MaterialID::Air && material == other.material && corner == other.corner;
    }
};

// Real per-face-corner AO (0fps.net's original Minecraft-style scheme -- goal 10's design as
// originally researched, only approximated before because Surface Nets shared one vertex across
// several quads; blocky meshing gives every face its own 4 unshared vertices, so the exact scheme
// is finally the natural one, per SS2.2). `outside` is the empty cell just beyond the face -- the
// neighbor that made this face exposed in the first place; axisU/axisV are the two in-plane axes;
// su/sv pick which of the 4 corners (-1 or +1 along each). side1/side2 are the two edge-adjacent
// cells of `outside` in the plane one step further along the face normal; `corner` is their shared
// diagonal neighbor. Levels 0..3 map to the same four brightness steps the old scheme shipped
// (goal 12's tuning carries over unchanged: 1.0 down to 0.55).
float corner_ao(const NeighborCache& neighbors, glm::ivec3 outside, int axisU, int axisV, int su, int sv) {
    glm::ivec3 sideUOffset(0);
    glm::ivec3 sideVOffset(0);
    sideUOffset[axisU] = su;
    sideVOffset[axisV] = sv;
    const bool side1 = is_occupied(neighbors.sample(outside + sideUOffset));
    const bool side2 = is_occupied(neighbors.sample(outside + sideVOffset));
    const bool corner = is_occupied(neighbors.sample(outside + sideUOffset + sideVOffset));
    const int level =
        (side1 && side2) ? 0
                         : 3 - (static_cast<int>(side1) + static_cast<int>(side2) + static_cast<int>(corner));
    return 0.55f + 0.15f * static_cast<float>(level);
}

// The 4 corners' (su, sv) signs in mask order (u0,v0),(u1,v0),(u1,v1),(u0,v1).
constexpr std::array<std::array<int, 2>, 4> kCornerSigns = {{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}};

// Water repurposes the AO attribute as WATER-COLUMN DEPTH, uniformly across a face (depth is a
// per-column property, not a per-corner occlusion one -- same convention the old scheme used, see
// research/water-foliage-design.md goal 28). In this generator water only ever fills a flat slab up
// to sea level wherever the ground allows, so in practice only its TOP face is ever exposed (a
// side face would need an air-filled gap beside a shorter water column, which fill_terrain never
// produces); the scan direction is world-down regardless of which face is being built, a safe,
// harmless default even in that unreached case. Counts from the voxel itself (part of the column)
// downward, capped at 8 (the honest limit of the one-chunk neighbor halo).
float water_depth_ao(const NeighborCache& neighbors, glm::ivec3 voxel) {
    int depth = 0;
    while (depth < 8 && properties_of(neighbors.sample({voxel.x, voxel.y - depth, voxel.z})).is_liquid) {
        ++depth;
    }
    return static_cast<float>(depth) / 8.0f;
}

// Appends one (possibly greedy-merged) quad spanning [u,u+w) x [v,v+h) at the fixed `layer` plane
// along `axis`, facing `dir` (+1 or -1). Each face gets its own 4 fresh vertices -- no sharing with
// any other face, even an adjacent one this chunk also emits -- which is what makes the normal
// exact and the AO genuinely per-corner instead of averaged; this is the real, deliberate cost of
// blocky meshing's flat-shaded look, not an oversight.
//
// Winding, derived algebraically rather than eyeballed (the ribbon-bug lesson: never trust a
// winding derivation without checking it): axisU=(axis+1)%3, axisV=(axis+2)%3 is the cyclic
// right-handed triple (cross(X,Y)=Z, cross(Y,Z)=X, cross(Z,X)=X -- true for every axis by
// construction), so corner order (u0,v0)->(u1,v0)->(u1,v1)->(u0,v1) [index order 0,1,2,3] always
// has cross(p1-p0, p2-p0) pointing along +axis. dir>0 wants outward normal +axis, so it uses that
// order directly; dir<0 wants -axis, so it uses the reversed cyclic order 0,3,2,1 (verified by hand
// for a concrete unit case in this change's commit message and in test_mesh_extractor.cpp's golden
// single-voxel test, which checks actual emitted positions, not the derivation on paper).
void emit_quad(MeshData& mesh, int axis, int dir, int axisU, int axisV, std::int32_t layer, std::int32_t u,
               std::int32_t v, std::int32_t w, std::int32_t h, const MaskCell& cell) {
    glm::vec3 normal(0.0f);
    normal[axis] = static_cast<float>(dir);
    const float coordAlongAxis = static_cast<float>(layer + (dir > 0 ? 1 : 0));

    const auto corner_pos = [&](std::int32_t cu, std::int32_t cv) {
        glm::vec3 p(0.0f);
        p[axis] = coordAlongAxis;
        p[axisU] = static_cast<float>(cu);
        p[axisV] = static_cast<float>(cv);
        return p;
    };
    const std::array<glm::vec3, 4> pos = {corner_pos(u, v), corner_pos(u + w, v), corner_pos(u + w, v + h),
                                          corner_pos(u, v + h)};
    const std::array<int, 4> order =
        (dir > 0) ? std::array<int, 4>{0, 1, 2, 3} : std::array<int, 4>{0, 3, 2, 1};

    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (int idx : order) {
        mesh.vertices.push_back(Vertex{pos[idx], normal, cell.material, cell.corner[idx]});
    }
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}

// One full greedy-meshing sweep for a single (axis, dir) face direction: kChunkSize layers, each
// building a kChunkSize x kChunkSize mask of exposed faces then merging it into maximal rectangles
// (the standard 2D greedy-meshing algorithm -- expand width while cells match, then expand height
// while the whole width-strip matches, per block_mesh's documented approach cited in the redesign
// doc). Only this chunk's own local range [0,kChunkSize) is ever a merge candidate or emits
// anything -- neighbor voxels are sampled (via NeighborCache, which reaches across chunks) only to
// decide exposure and AO, never to emit geometry on a neighbor's behalf.
void sweep_axis_direction(int axis, int dir, const NeighborCache& neighbors, MeshData& mesh) {
    const int axisU = (axis + 1) % 3;
    const int axisV = (axis + 2) % 3;
    std::array<MaskCell, static_cast<std::size_t>(kChunkSize) * static_cast<std::size_t>(kChunkSize)> mask;
    std::array<bool, static_cast<std::size_t>(kChunkSize) * static_cast<std::size_t>(kChunkSize)> visited;

    for (std::int32_t layer = 0; layer < kChunkSize; ++layer) {
        mask.fill(MaskCell{});
        visited.fill(false);

        for (std::int32_t v = 0; v < kChunkSize; ++v) {
            for (std::int32_t u = 0; u < kChunkSize; ++u) {
                glm::ivec3 voxel(0);
                voxel[axis] = layer;
                voxel[axisU] = u;
                voxel[axisV] = v;
                const MaterialID here = neighbors.sample(voxel);
                if (!is_occupied(here)) {
                    continue; // mask cell already defaults to "no face"
                }
                glm::ivec3 outside = voxel;
                outside[axis] += dir;
                if (is_occupied(neighbors.sample(outside))) {
                    continue; // interior boundary (solid-solid or solid-water): never exposed
                }

                MaskCell cell;
                cell.material = here;
                if (properties_of(here).is_liquid) {
                    const float depth = water_depth_ao(neighbors, voxel);
                    cell.corner = {depth, depth, depth, depth};
                } else {
                    for (std::size_t c = 0; c < 4; ++c) {
                        cell.corner[c] = corner_ao(neighbors, outside, axisU, axisV, kCornerSigns[c][0],
                                                   kCornerSigns[c][1]);
                    }
                }
                mask[static_cast<std::size_t>(v) * static_cast<std::size_t>(kChunkSize) +
                     static_cast<std::size_t>(u)] = cell;
            }
        }

        for (std::int32_t v = 0; v < kChunkSize; ++v) {
            for (std::int32_t u = 0; u < kChunkSize; ++u) {
                const std::size_t idx = static_cast<std::size_t>(v) * static_cast<std::size_t>(kChunkSize) +
                                        static_cast<std::size_t>(u);
                if (visited[idx] || mask[idx].material == MaterialID::Air) {
                    continue;
                }
                const MaskCell& ref = mask[idx];

                std::int32_t w = 1;
                while (u + w < kChunkSize && !visited[idx + static_cast<std::size_t>(w)] &&
                       mask[idx + static_cast<std::size_t>(w)].mergeable_with(ref)) {
                    ++w;
                }

                std::int32_t h = 1;
                bool canGrow = true;
                while (v + h < kChunkSize && canGrow) {
                    const std::size_t rowStart =
                        static_cast<std::size_t>(v + h) * static_cast<std::size_t>(kChunkSize) +
                        static_cast<std::size_t>(u);
                    for (std::int32_t du = 0; du < w; ++du) {
                        const std::size_t ci = rowStart + static_cast<std::size_t>(du);
                        if (visited[ci] || !mask[ci].mergeable_with(ref)) {
                            canGrow = false;
                            break;
                        }
                    }
                    if (canGrow) {
                        ++h;
                    }
                }

                for (std::int32_t dv = 0; dv < h; ++dv) {
                    const std::size_t rowStart =
                        static_cast<std::size_t>(v + dv) * static_cast<std::size_t>(kChunkSize) +
                        static_cast<std::size_t>(u);
                    for (std::int32_t du = 0; du < w; ++du) {
                        visited[rowStart + static_cast<std::size_t>(du)] = true;
                    }
                }

                emit_quad(mesh, axis, dir, axisU, axisV, layer, u, v, w, h, ref);
            }
        }
    }
}

} // namespace

MeshData extract_mesh(const NeighborCache& neighbors) {
    MeshData mesh;
    for (int axis = 0; axis < 3; ++axis) {
        sweep_axis_direction(axis, +1, neighbors, mesh);
        sweep_axis_direction(axis, -1, neighbors, mesh);
    }
    return mesh;
}

MeshData extract_mesh(const ChunkStore& store, ChunkCoord coord) {
    return extract_mesh(NeighborCache(store, coord));
}

} // namespace world::meshing
