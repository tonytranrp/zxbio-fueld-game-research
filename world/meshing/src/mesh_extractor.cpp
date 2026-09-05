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

namespace {

constexpr std::uint32_t kNoVertex = std::numeric_limits<std::uint32_t>::max();

// Padded local-space sampling: lx/ly/lz may range one voxel past this chunk's own 0..31 bounds in
// any direction, so the owning chunk per axis is a plain sign/range test into a 3x3x3 pointer
// cache resolved ONCE per extraction. The per-axis mask (world_to_local on the padded coordinate
// directly: -1 & 31 == 31, 32 & 31 == 0) is the same cross-chunk math as before -- face-, edge-,
// and corner-adjacent neighbors all resolve with no per-direction special-casing.
//
// The cache replaced a ChunkStore::find per sample for a measured reason, not style: ~295k hash
// finds per extraction each construct/destroy an unordered_map iterator, and MSVC's debug-
// iterator bookkeeping (_ITERATOR_DEBUG_LEVEL=2) routes every one through a single global lock --
// 16 concurrent mesh jobs ran ~80x slower than one, live-stalling chunk streaming's initial load.
// An array index per sample is also simply cheaper than a hash find in release builds.
class NeighborCache {
public:
    NeighborCache(const ChunkStore& store, ChunkCoord base) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dy = -1; dy <= 1; ++dy) {
                for (std::int32_t dx = -1; dx <= 1; ++dx) {
                    chunks_[slot(dx, dy, dz)] = store.find(ChunkCoord{base.x + dx, base.y + dy, base.z + dz});
                }
            }
        }
    }

    [[nodiscard]] MaterialID sample(std::int32_t lx, std::int32_t ly, std::int32_t lz) const {
        const std::int32_t dx = lx < 0 ? -1 : (lx >= kChunkSize ? 1 : 0);
        const std::int32_t dy = ly < 0 ? -1 : (ly >= kChunkSize ? 1 : 0);
        const std::int32_t dz = lz < 0 ? -1 : (lz >= kChunkSize ? 1 : 0);
        const Chunk* chunk = chunks_[slot(dx, dy, dz)];
        if (chunk == nullptr) {
            // The caller is responsible for ensuring all 26 neighbors are generated first (see
            // the header's precondition note) -- this is a safety net against a crash, not the
            // mechanism that guarantees seamless geometry.
            return MaterialID::Air;
        }
        return chunk->voxels().at(local_index(world_to_local(lx), world_to_local(ly), world_to_local(lz)));
    }

private:
    [[nodiscard]] static std::size_t slot(std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        return static_cast<std::size_t>(dx + 1) + static_cast<std::size_t>(dy + 1) * 3 +
               static_cast<std::size_t>(dz + 1) * 9;
    }

    std::array<const Chunk*, 27> chunks_{};
};

// The 8 corners of a cell: bit0=dx, bit1=dy, bit2=dz.
constexpr std::array<glm::vec3, 8> kCornerOffsets = {{
    {0, 0, 0},
    {1, 0, 0},
    {0, 1, 0},
    {1, 1, 0},
    {0, 0, 1},
    {1, 0, 1},
    {0, 1, 1},
    {1, 1, 1},
}};

// The 12 edges of a cell as corner-index pairs.
constexpr std::array<std::array<int, 2>, 12> kEdges = {{
    {0, 1},
    {2, 3},
    {4, 5},
    {6, 7}, // X-direction edges
    {0, 2},
    {1, 3},
    {4, 6},
    {5, 7}, // Y-direction edges
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7}, // Z-direction edges
}};

struct CellSample {
    bool active = false;
    glm::vec3 vertexPos{0.0f};             // valid only if active; chunk-local space
    MaterialID material = MaterialID::Air; // valid only if active
    float ao = 1.0f;                       // baked concavity AO (research/baked-ao-design.md)

    static CellSample compute(const NeighborCache& neighbors, std::int32_t cx, std::int32_t cy,
                              std::int32_t cz) {
        std::array<MaterialID, 8> corners{};
        int solidCorners = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            const glm::vec3& off = kCornerOffsets[i];
            corners[i] =
                neighbors.sample(cx + static_cast<std::int32_t>(off.x), cy + static_cast<std::int32_t>(off.y),
                                 cz + static_cast<std::int32_t>(off.z));
            solidCorners += is_occupied(corners[i]) ? 1 : 0;
        }

        CellSample cell;
        // AO from the cell's own enclosure (zero extra samples -- the corners above already went
        // through the padded cross-chunk cache): flat ground is s=4 and MUST stay 1.0 (a linear
        // map over the whole range would uniformly dim all flat terrain, which is global dimming,
        // not occlusion); only the concave half darkens, in the 4 discrete levels the reference
        // cube-quad scheme also uses. s in {5,6,7} -> {0.85, 0.70, 0.55}.
        cell.ao = 1.0f - 0.15f * static_cast<float>(std::max(0, solidCorners - 4));
        glm::vec3 sum{0.0f};
        int crossingCount = 0;
        float bestCornerY = -1.0f;
        for (const auto& edge : kEdges) {
            const MaterialID a = corners[static_cast<std::size_t>(edge[0])];
            const MaterialID b = corners[static_cast<std::size_t>(edge[1])];
            if (is_occupied(a) != is_occupied(b)) {
                sum += (kCornerOffsets[static_cast<std::size_t>(edge[0])] +
                        kCornerOffsets[static_cast<std::size_t>(edge[1])]) *
                       0.5f;
                ++crossingCount;
                // Material pick (Group M refinement of the old "first solid corner wins"): the
                // HIGHEST crossing solid corner wins, ties broken toward non-water. With banded
                // surface materials this makes every surface cell read its actual TOP material
                // (grass skin, sand shore) instead of whichever soil corner edge-iteration
                // happened to visit first; the water tiebreak keeps shoreline edges land-colored.
                const std::size_t solidIdx = static_cast<std::size_t>(is_occupied(a) ? edge[0] : edge[1]);
                const MaterialID solidMat = is_occupied(a) ? a : b;
                const float cornerY = kCornerOffsets[solidIdx].y;
                const bool better = cornerY > bestCornerY ||
                                    (cornerY == bestCornerY && properties_of(cell.material).is_liquid &&
                                     !properties_of(solidMat).is_liquid);
                if (cell.material == MaterialID::Air || better) {
                    cell.material = solidMat;
                    bestCornerY = cornerY;
                }
            }
        }

        cell.active = crossingCount > 0;
        if (cell.active) {
            cell.vertexPos =
                glm::vec3(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz)) +
                sum / static_cast<float>(crossingCount);
        }
        return cell;
    }
};

// Cell anchors needed range over [-1, kChunkSize-1] per axis (kChunkSize+1 values): -1 is needed
// because a chunk's own boundary quads reference cells anchored one step into a lower-direction
// neighbor (see emit_quads_along_axis's derivation), even though such a cell's OWN quads (if any)
// are never independently owned/emitted by this chunk. kChunkSize-1 (31) is the highest cell this
// chunk emits quads for; +1 padding beyond that (up to local 32) is still sampled for corner data
// via sample_padded, but never needs its own cell-anchor slot. Net local sample range across both
// corner sampling and cell anchoring is -1..32 inclusive (34 values) -- matching this module's own
// spec derivation of a full 26-neighbor halo.
constexpr std::int32_t kCellMin = -1;
constexpr std::int32_t kCellRange = kChunkSize + 1; // 33 possible anchor values per axis: -1..31

std::size_t cell_array_index(std::int32_t cx, std::int32_t cy, std::int32_t cz) {
    const auto ix = static_cast<std::size_t>(cx - kCellMin);
    const auto iy = static_cast<std::size_t>(cy - kCellMin);
    const auto iz = static_cast<std::size_t>(cz - kCellMin);
    const auto range = static_cast<std::size_t>(kCellRange);
    return ix + iy * range + iz * range * range;
}

enum class Axis { X, Y, Z };

// For a crossing edge along `axis`, the four cells sharing it vary in the two axes OTHER than
// `axis`, at deltas {(-1,-1),(0,-1),(0,0),(-1,0)} relative to the edge's own (vx,vy,vz) -- applied
// to (Y,Z) for X-edges, (X,Z) for Y-edges, (X,Y) for Z-edges.
constexpr std::array<std::array<int, 2>, 4> kSharingCellDeltas = {{{-1, -1}, {0, -1}, {0, 0}, {-1, 0}}};

void anchor_for_delta(Axis axis, std::int32_t vx, std::int32_t vy, std::int32_t vz, int d0, int d1,
                      std::int32_t& cx, std::int32_t& cy, std::int32_t& cz) {
    switch (axis) {
    case Axis::X:
        cx = vx;
        cy = vy + d0;
        cz = vz + d1;
        break;
    case Axis::Y:
        cx = vx + d0;
        cy = vy;
        cz = vz + d1;
        break;
    case Axis::Z:
        cx = vx + d0;
        cy = vy + d1;
        cz = vz;
        break;
    }
}

// Emits quads for every crossing grid edge along `axis` whose OWNING voxel (the edge's lower
// endpoint) lies within this chunk's own 0..31 range -- this is what guarantees each world-space
// edge is owned by exactly one chunk (whichever chunk's local range contains that voxel triple),
// so neighbors never emit duplicate geometry for the same edge, only independently-computed,
// position-matching vertices at the seam (verified by the boundary-continuity test).
//
// Winding order, derived by hand (cross product of two real edge vectors per axis, not assumed
// symmetric -- see the accompanying design notes): the natural (v0,v1,v2,v3) order over
// kSharingCellDeltas produces a face normal matching "the LOWER-endpoint voxel is solid" for the
// X and Z axes, but matching "the HIGHER-endpoint voxel is solid" for Y specifically -- a real,
// concrete asymmetry from how the right-hand-rule cross product interacts with which two axes are
// held fixed per direction. Flip the order when the actual crossing doesn't match that axis's
// natural case, or every other quad on non-X/Z axes comes out facing inward.
void emit_quads_along_axis(Axis axis, const NeighborCache& neighbors,
                           const std::vector<std::uint32_t>& vertexIndex, MeshData& mesh,
                           std::vector<glm::vec3>& normalAccum) {
    const bool naturalIsLowSolid = (axis != Axis::Y);

    for (std::int32_t a = 0; a < kChunkSize; ++a) {
        for (std::int32_t b = 0; b < kChunkSize; ++b) {
            for (std::int32_t c = 0; c < kChunkSize; ++c) {
                std::int32_t vx = 0;
                std::int32_t vy = 0;
                std::int32_t vz = 0;
                switch (axis) {
                case Axis::X:
                    vx = a;
                    vy = b;
                    vz = c;
                    break;
                case Axis::Y:
                    vx = b;
                    vy = a;
                    vz = c;
                    break;
                case Axis::Z:
                    vx = b;
                    vy = c;
                    vz = a;
                    break;
                }

                MaterialID lo{};
                MaterialID hi{};
                switch (axis) {
                case Axis::X:
                    lo = neighbors.sample(vx, vy, vz);
                    hi = neighbors.sample(vx + 1, vy, vz);
                    break;
                case Axis::Y:
                    lo = neighbors.sample(vx, vy, vz);
                    hi = neighbors.sample(vx, vy + 1, vz);
                    break;
                case Axis::Z:
                    lo = neighbors.sample(vx, vy, vz);
                    hi = neighbors.sample(vx, vy, vz + 1);
                    break;
                }
                const bool loSolid = is_occupied(lo);
                if (loSolid == is_occupied(hi)) {
                    continue; // not a crossing edge
                }

                std::array<std::uint32_t, 4> quadVerts{};
                bool allValid = true;
                for (std::size_t i = 0; i < 4; ++i) {
                    std::int32_t cx = 0;
                    std::int32_t cy = 0;
                    std::int32_t cz = 0;
                    anchor_for_delta(axis, vx, vy, vz, kSharingCellDeltas[i][0], kSharingCellDeltas[i][1], cx,
                                     cy, cz);
                    const std::uint32_t vIdx = vertexIndex[cell_array_index(cx, cy, cz)];
                    if (vIdx == kNoVertex) {
                        // Should not happen: every cell sharing a genuine crossing edge is active
                        // by construction (it contains that edge's two disagreeing corners).
                        // Kept as a real safety net, not dead code, given how easy this specific
                        // derivation is to get subtly wrong.
                        allValid = false;
                        break;
                    }
                    quadVerts[i] = vIdx;
                }
                if (!allValid) {
                    continue;
                }

                const bool useNaturalOrder = (loSolid == naturalIsLowSolid);
                const std::array<std::uint32_t, 4> ordered =
                    useNaturalOrder ? quadVerts
                                    : std::array<std::uint32_t, 4>{quadVerts[0], quadVerts[3], quadVerts[2],
                                                                   quadVerts[1]};

                mesh.indices.push_back(ordered[0]);
                mesh.indices.push_back(ordered[1]);
                mesh.indices.push_back(ordered[2]);
                mesh.indices.push_back(ordered[0]);
                mesh.indices.push_back(ordered[2]);
                mesh.indices.push_back(ordered[3]);

                // Area-weighted normal accumulation for free: an unnormalized cross product's
                // magnitude is proportional to the triangle's area, so summing it directly into
                // each incident vertex (normalized once, after every axis is processed) gives the
                // area weighting without a separate pass.
                const glm::vec3& p0 = mesh.vertices[ordered[0]].position;
                const glm::vec3& p1 = mesh.vertices[ordered[1]].position;
                const glm::vec3& p2 = mesh.vertices[ordered[2]].position;
                const glm::vec3 faceNormal = glm::cross(p1 - p0, p2 - p0);
                for (std::uint32_t vi : ordered) {
                    normalAccum[vi] += faceNormal;
                }
            }
        }
    }
}

} // namespace

MeshData extract_mesh(const ChunkStore& store, ChunkCoord coord) {
    MeshData mesh;
    const NeighborCache neighbors(store, coord);

    // Precompute every needed cell's activity/vertex/material once, up front -- both the vertex-
    // emission pass and the quad-connection pass need arbitrary lookups (including the -1-anchored
    // boundary layer), so compute-then-index is simpler than trying to interleave the two passes.
    std::vector<CellSample> cells(static_cast<std::size_t>(kCellRange) *
                                  static_cast<std::size_t>(kCellRange) *
                                  static_cast<std::size_t>(kCellRange));
    for (std::int32_t cz = kCellMin; cz < kChunkSize; ++cz) {
        for (std::int32_t cy = kCellMin; cy < kChunkSize; ++cy) {
            for (std::int32_t cx = kCellMin; cx < kChunkSize; ++cx) {
                cells[cell_array_index(cx, cy, cz)] = CellSample::compute(neighbors, cx, cy, cz);
            }
        }
    }

    // Emit one vertex per ACTIVE cell across the FULL computed range, including the -1 boundary
    // layer -- not just this chunk's own 0..31 "owned" cells. A boundary-layer cell's own quads
    // (if any) are never independently emitted by this chunk (see emit_quads_along_axis), but this
    // chunk's OWN owned quads still reference such cells as their negative-side neighbor, so they
    // need a real vertex in THIS chunk's own buffer too -- a second, independently-computed vertex
    // at a position matching whatever the actual owning neighbor also emits for the same cell.
    std::vector<std::uint32_t> vertexIndex(cells.size(), kNoVertex);
    for (std::int32_t cz = kCellMin; cz < kChunkSize; ++cz) {
        for (std::int32_t cy = kCellMin; cy < kChunkSize; ++cy) {
            for (std::int32_t cx = kCellMin; cx < kChunkSize; ++cx) {
                const std::size_t idx = cell_array_index(cx, cy, cz);
                const CellSample& cell = cells[idx];
                if (!cell.active) {
                    continue;
                }
                float ao = cell.ao;
                if (properties_of(cell.material).is_liquid) {
                    // Water repurposes the AO attribute as WATER-COLUMN DEPTH (see
                    // research/water-foliage-design.md goal 28): concavity-AO is meaningless on a
                    // flat open surface, and the shader's water path wants depth for its
                    // shallow-to-deep tint. Scan down through water voxels until ground, capped
                    // at 8 (also the honest limit of the one-chunk neighbor halo).
                    // Counts from the anchor voxel itself (part of the column) downward.
                    int depth = 0;
                    while (depth < 8 && properties_of(neighbors.sample(cx, cy - depth, cz)).is_liquid) {
                        ++depth;
                    }
                    ao = static_cast<float>(depth) / 8.0f;
                }
                vertexIndex[idx] = static_cast<std::uint32_t>(mesh.vertices.size());
                mesh.vertices.push_back(Vertex{cell.vertexPos, glm::vec3{0.0f}, cell.material, ao});
            }
        }
    }

    std::vector<glm::vec3> normalAccum(mesh.vertices.size(), glm::vec3{0.0f});
    emit_quads_along_axis(Axis::X, neighbors, vertexIndex, mesh, normalAccum);
    emit_quads_along_axis(Axis::Y, neighbors, vertexIndex, mesh, normalAccum);
    emit_quads_along_axis(Axis::Z, neighbors, vertexIndex, mesh, normalAccum);

    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const glm::vec3& accum = normalAccum[i];
        const float len = glm::length(accum);
        // A zero-length accumulator means this vertex's cell is active but every one of its edges
        // belongs to a neighbor's ownership (never referenced by any of THIS chunk's own quads) --
        // harmless (the vertex is simply unreferenced by mesh.indices, never rasterized), but
        // guarded here so no NaN from normalizing a zero vector ends up in the buffer regardless.
        mesh.vertices[i].normal = (len > 1e-8f) ? (accum / len) : glm::vec3{0.0f, 1.0f, 0.0f};
    }

    return mesh;
}

} // namespace world::meshing
