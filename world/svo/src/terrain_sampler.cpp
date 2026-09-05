#include "world/svo/terrain_sampler.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

#include "world/chunk/chunk_voxels.hpp" // kChunkSize

namespace world::svo {

using world::chunk::kChunkSize;
using world::chunk::MaterialID;
using world::generation::TreePlacement;

namespace {

// Trees may lean this far (horizontally) into a neighboring chunk column, so placements are
// gathered from columns overlapping the region grown by it.
constexpr float kTreeReach = 8.0f;

Box tree_box(const TreePlacement& tree) {
    const world::generation::TreeBounds b = world::generation::tree_bounds(tree);
    return Box{b.min, b.max};
}

} // namespace

TerrainSampler::TerrainSampler(const world::generation::HeightmapGenerator& heightmap,
                               const TerrainSamplerParams& params, const Box& region)
    : heightmap_(&heightmap), params_(params),
      field_(heightmap, region.min.x, region.min.z,
             std::max(region.max.x - region.min.x, region.max.z - region.min.z), params.height_field_cell) {
    if (params_.trees) {
        collect_trees(region);
    }
}

void TerrainSampler::collect_trees(const Box& region) {
    const auto toChunk = [](float v) {
        return static_cast<std::int32_t>(std::floor(v / static_cast<float>(kChunkSize)));
    };
    const std::int32_t cx0 = toChunk(region.min.x - kTreeReach);
    const std::int32_t cx1 = toChunk(region.max.x + kTreeReach);
    const std::int32_t cz0 = toChunk(region.min.z - kTreeReach);
    const std::int32_t cz1 = toChunk(region.max.z + kTreeReach);
    for (std::int32_t cz = cz0; cz <= cz1; ++cz) {
        for (std::int32_t cx = cx0; cx <= cx1; ++cx) {
            for (const TreePlacement& tree :
                 world::generation::compute_tree_placements(cx, cz, params_.seed, *heightmap_)) {
                const Box bounds = tree_box(tree);
                if (bounds.intersects(region)) {
                    trees_.push_back(tree);
                    treeBounds_.push_back(bounds);
                }
            }
        }
    }

    treeGrid_.xMin = region.min.x - kTreeReach;
    treeGrid_.zMin = region.min.z - kTreeReach;
    treeGrid_.cell = 16.0f;
    treeGrid_.nx = static_cast<std::int32_t>(
                       std::ceil((region.max.x - region.min.x + 2.0f * kTreeReach) / treeGrid_.cell)) +
                   1;
    treeGrid_.nz = static_cast<std::int32_t>(
                       std::ceil((region.max.z - region.min.z + 2.0f * kTreeReach) / treeGrid_.cell)) +
                   1;
    treeGrid_.cells.assign(static_cast<std::size_t>(treeGrid_.nx) * static_cast<std::size_t>(treeGrid_.nz),
                           {});
    for (std::uint32_t index = 0; index < treeBounds_.size(); ++index) {
        const Box& b = treeBounds_[index];
        const std::int32_t gx0 =
            std::clamp(static_cast<std::int32_t>(std::floor((b.min.x - treeGrid_.xMin) / treeGrid_.cell)), 0,
                       treeGrid_.nx - 1);
        const std::int32_t gx1 =
            std::clamp(static_cast<std::int32_t>(std::floor((b.max.x - treeGrid_.xMin) / treeGrid_.cell)), 0,
                       treeGrid_.nx - 1);
        const std::int32_t gz0 =
            std::clamp(static_cast<std::int32_t>(std::floor((b.min.z - treeGrid_.zMin) / treeGrid_.cell)), 0,
                       treeGrid_.nz - 1);
        const std::int32_t gz1 =
            std::clamp(static_cast<std::int32_t>(std::floor((b.max.z - treeGrid_.zMin) / treeGrid_.cell)), 0,
                       treeGrid_.nz - 1);
        for (std::int32_t gz = gz0; gz <= gz1; ++gz) {
            for (std::int32_t gx = gx0; gx <= gx1; ++gx) {
                treeGrid_
                    .cells[static_cast<std::size_t>(gz) * static_cast<std::size_t>(treeGrid_.nx) +
                           static_cast<std::size_t>(gx)]
                    .push_back(index);
            }
        }
    }
}

void TerrainSampler::trees_touching(const Box& box, std::vector<std::uint32_t>& out) const {
    out.clear();
    if (trees_.empty()) {
        return;
    }
    const auto cellX = [&](float x) {
        return std::clamp(static_cast<std::int32_t>(std::floor((x - treeGrid_.xMin) / treeGrid_.cell)), 0,
                          treeGrid_.nx - 1);
    };
    const auto cellZ = [&](float z) {
        return std::clamp(static_cast<std::int32_t>(std::floor((z - treeGrid_.zMin) / treeGrid_.cell)), 0,
                          treeGrid_.nz - 1);
    };
    const std::int32_t gx0 = cellX(box.min.x);
    const std::int32_t gx1 = cellX(box.max.x);
    const std::int32_t gz0 = cellZ(box.min.z);
    const std::int32_t gz1 = cellZ(box.max.z);
    for (std::int32_t gz = gz0; gz <= gz1; ++gz) {
        for (std::int32_t gx = gx0; gx <= gx1; ++gx) {
            for (const std::uint32_t index :
                 treeGrid_.cells[static_cast<std::size_t>(gz) * static_cast<std::size_t>(treeGrid_.nx) +
                                 static_cast<std::size_t>(gx)]) {
                if (treeBounds_[index].intersects(box) &&
                    std::find(out.begin(), out.end(), index) == out.end()) {
                    out.push_back(index);
                }
            }
        }
    }
}

MaterialID TerrainSampler::column_material(float surfaceHeight, bool beach, bool grassy, float voxelBottom,
                                           float voxelEdge) const noexcept {
    if (voxelBottom <= surfaceHeight) {
        const float depth = surfaceHeight - voxelBottom;
        if (depth < voxelEdge) {
            return beach ? MaterialID::Sand : (grassy ? MaterialID::Grass : MaterialID::Stone);
        }
        if (depth < kSoilDepth + voxelEdge) {
            return beach ? MaterialID::Sand : MaterialID::Dirt;
        }
        return MaterialID::Stone;
    }
    if (voxelBottom <= params_.sea_level) {
        return MaterialID::Water; // water never overrides solid ground (fill_terrain's rule)
    }
    return MaterialID::Air;
}

BoxClassification TerrainSampler::classify(const Box& box) const {
    // Trees: exact convex tests against every tree whose AABB touches the box (not the AABB
    // itself -- a 6x15x6 m tree AABB is ~23K bricks at 8 mm, most of them empty air the first
    // version sampled for nothing). A box inside one lobe AND above every column is solid leaves.
    bool insideLobe = false;
    if (params_.trees) {
        thread_local std::vector<std::uint32_t> touching;
        trees_touching(box, touching);
        for (const std::uint32_t index : touching) {
            if (!world::generation::tree_intersects_box(trees_[index], box.min, box.max)) {
                continue;
            }
            if (world::generation::tree_lobe_contains_box(trees_[index], box.min, box.max)) {
                insideLobe = true; // only leaves if no terrain reaches into the box -- checked below
                continue;
            }
            return {BoxClass::Mixed, MaterialID::Air};
        }
    }
    const HeightField* field = &field_;
    for (const auto& focus : focusFields_) {
        if (focus->covers(box.min.x, box.min.z, box.max.x, box.max.z)) {
            field = focus.get();
            break;
        }
    }
    const HeightField::Range r = field->range(box.min.x, box.min.z, box.max.x, box.max.z);
    if (box.min.y > r.max) {
        // Every voxel's bottom lies above every column's surface: air and/or water only.
        if (insideLobe) {
            // Above the terrain and inside a canopy lobe: leaves throughout (leaves do fill water
            // too -- a lobe never reaches sea level in practice, but the rule is consistent with
            // fill_brick's "trees override non-solid terrain" either way).
            return {BoxClass::Solid, MaterialID::Leaves};
        }
        if (box.max.y <= params_.sea_level) {
            return {BoxClass::Solid, MaterialID::Water};
        }
        if (box.min.y > params_.sea_level) {
            return {BoxClass::Air, MaterialID::Air};
        }
        return {BoxClass::Mixed, MaterialID::Air};
    }
    if (insideLobe) {
        return {BoxClass::Mixed, MaterialID::Air}; // a lobe overlapping terrain: sample it
    }
    if (box.max.y <= r.min - kSoilDepth) {
        // Even the topmost voxel is deeper than the soil band under the lowest column: stone.
        return {BoxClass::Solid, MaterialID::Stone};
    }
    if (box.max.y <= r.min && box.min.y >= r.max - kSoilDepth) {
        // Entirely below every surface (no surface voxel: every bottom <= box.max - e <= r.min - e,
        // so depth >= e) yet within the soil band under every column (depth <= r.max - box.min
        // <= kSoilDepth < kSoilDepth + e for any voxel edge e): uniformly the soil material. The
        // band is Sand on a beach column and Dirt elsewhere, so the beach test must be uniform over
        // the footprint too. Without this rule the whole 3 m soil band was "Mixed" -- measured as
        // 800K sampled-then-homogeneous bricks out of 1.04M at the finest level alone.
        if (r.max <= params_.sea_level + kBeachBand) {
            return {BoxClass::Solid, MaterialID::Sand};
        }
        if (r.min > params_.sea_level + kBeachBand) {
            return {BoxClass::Solid, MaterialID::Dirt};
        }
    }
    return {BoxClass::Mixed, MaterialID::Air};
}

void TerrainSampler::set_focus(const glm::vec3& center, float radius) {
    // Tiers, finest first: 1/16 m within `radius`, 1/8 m within 4x that. Each square is snapped to
    // whole coarse cells so no tier boundary splits a coarse cell. The 1/8 m tier over a 128 m
    // square is 1M samples (~20 ms) -- it exists because the mid-distance rings (bricks of
    // 25 cm - 1 m) were still sampling 2-4 bricks for every one kept under the 0.5 m field's margin.
    focusFields_.clear();
    const float coarse = params_.height_field_cell;
    const auto make = [&](float r, float cell) {
        const float x0 = std::floor((center.x - r) / coarse) * coarse;
        const float z0 = std::floor((center.z - r) / coarse) * coarse;
        const float extent = std::ceil(2.0f * r / coarse) * coarse;
        focusFields_.push_back(std::make_unique<HeightField>(*heightmap_, x0, z0, extent, cell));
    };
    make(radius, 1.0f / 16.0f);
    make(4.0f * radius, 1.0f / 8.0f);
}

namespace {

// Per-thread cache of the 8x8 column-height grid keyed by (voxel edge, x, z): the builder's
// conservative classification asks for a vertical STACK of bricks per surface column (~4 sampled
// for every ~2 kept on this terrain, measured), and all of them share one XZ footprint -- so one
// noise-grid call serves the whole stack instead of one per brick. Direct-mapped, no eviction
// policy: the DFS build visits a column's bricks close together in time.
struct ColumnGridCache {
    struct Entry {
        float voxelEdge = 0.0f;
        float x = 0.0f;
        float z = 0.0f;
        bool valid = false;
        world::generation::HeightmapMinMax range{};
        std::array<float, 64> h{};
    };
    static constexpr std::size_t kEntries = 8192;
    std::array<Entry, kEntries> entries{};

    Entry& slot(float voxelEdge, float x, float z) noexcept {
        std::uint32_t bits = 0;
        const auto mixIn = [&](float v) {
            std::uint32_t u = 0;
            std::memcpy(&u, &v, sizeof(u));
            bits ^= u + 0x9E3779B9u + (bits << 6) + (bits >> 2);
        };
        mixIn(voxelEdge);
        mixIn(x);
        mixIn(z);
        return entries[bits % kEntries];
    }
};

ColumnGridCache& column_cache() noexcept {
    thread_local ColumnGridCache cache;
    return cache;
}

std::atomic<std::uint64_t> g_gridCalls{0};
std::atomic<std::uint64_t> g_gridHits{0};

} // namespace

std::uint64_t TerrainSampler::debug_grid_calls() noexcept {
    return g_gridCalls.load();
}
std::uint64_t TerrainSampler::debug_grid_cache_hits() noexcept {
    return g_gridHits.load();
}

void TerrainSampler::fill_brick(const glm::vec3& origin, float voxelEdge, Brick& brick) const {
    constexpr int N = kBrickEdge;
    ColumnGridCache::Entry& cached = column_cache().slot(voxelEdge, origin.x, origin.z);
    if (!(cached.valid && cached.voxelEdge == voxelEdge && cached.x == origin.x && cached.z == origin.z)) {
        cached.range =
            heightmap_->generate_column_heights_spaced(origin.x, origin.z, N, N, voxelEdge, cached.h.data());
        cached.voxelEdge = voxelEdge;
        cached.x = origin.x;
        cached.z = origin.z;
        cached.valid = true;
        g_gridCalls.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_gridHits.fetch_add(1, std::memory_order_relaxed);
    }
    const std::array<float, kBrickVoxels / kBrickEdge>& h = cached.h; // 64 column heights
    const world::generation::HeightmapMinMax hRange = cached.range;
    const float brickTop = origin.y + static_cast<float>(N - 1) * voxelEdge; // highest voxel bottom
    // The builder's box classification is deliberately conservative, so many bricks it asks for
    // turn out homogeneous. Decide those from the ONE height grid just fetched and skip the four
    // slope grids -- the only per-column work left is water vs. air (no material band involved).
    if (origin.y > hRange.max) {
        if (origin.y <= params_.sea_level) {
            // Water fills every layer whose bottom is at or below sea level: whole-layer writes.
            for (int j = 0; j < N; ++j) {
                if (origin.y + static_cast<float>(j) * voxelEdge <= params_.sea_level) {
                    fill_layer(brick, j, MaterialID::Water);
                }
            }
        }
    } else if (brickTop <= hRange.min - kSoilDepth - voxelEdge) {
        // Every voxel is at least kSoilDepth + voxelEdge below the lowest column: solid stone.
        for (int j = 0; j < N; ++j) {
            fill_layer(brick, j, MaterialID::Stone);
        }
    } else {
        fill_columns(origin, voxelEdge, h, brick);
    }

    if (!params_.trees) {
        return;
    }
    voxelize_trees(origin, voxelEdge, brick);
}

void TerrainSampler::fill_layer(Brick& brick, int j, MaterialID material) noexcept {
    // One Y layer = 8 runs of 8 consecutive linear indices (x innermost): 8 bits of a mask word and
    // 2 material words per run -- whole-word writes, no per-voxel read-modify-write.
    const auto m = static_cast<std::uint32_t>(material);
    const std::uint32_t materialWord = m | (m << 8) | (m << 16) | (m << 24);
    std::uint32_t* words = brick.words().data();
    for (int k = 0; k < kBrickEdge; ++k) {
        const std::size_t index = brick_voxel_index(0, j, k); // multiple of 8
        words[index >> 5] |= 0xFFu << (index & 31u);
        words[kBrickMaskWords + (index >> 2)] = materialWord;
        words[kBrickMaskWords + (index >> 2) + 1] = materialWord;
    }
}

void TerrainSampler::fill_columns(const glm::vec3& origin, float voxelEdge, const std::array<float, 64>& h,
                                  Brick& brick) const {
    constexpr int N = kBrickEdge;
    // Materials are written into a byte scratch and packed once at the end: measured, the
    // per-voxel Brick::set read-modify-write was the single largest cost of a whole tree build.
    std::array<std::uint8_t, kBrickVoxels> bytes{};
    for (int k = 0; k < N; ++k) {
        for (int i = 0; i < N; ++i) {
            const std::size_t c = static_cast<std::size_t>(k) * N + static_cast<std::size_t>(i);
            const float surface = h[c];
            if (origin.y > surface && origin.y > params_.sea_level) {
                continue; // whole column above its surface and above the sea: air
            }
            // Slope at a FIXED 1 m baseline regardless of voxel size (the grass-vs-rock decision
            // fill_terrain makes for the same column), read off the region-wide height field's
            // corner samples instead of four more noise-grid calls per brick -- exact at integer
            // columns, interpolated at sub-meter ones.
            const float slope = field_.slope_at(origin.x + static_cast<float>(i) * voxelEdge,
                                                origin.z + static_cast<float>(k) * voxelEdge);
            const bool beach = surface <= params_.sea_level + kBeachBand;
            const bool grassy = !beach && slope <= kGrassMaxSlope;
            std::uint8_t* column = bytes.data() + brick_voxel_index(i, 0, k);
            for (int j = 0; j < N; ++j) {
                const float bottom = origin.y + static_cast<float>(j) * voxelEdge;
                column[static_cast<std::size_t>(j) * N] =
                    static_cast<std::uint8_t>(column_material(surface, beach, grassy, bottom, voxelEdge));
            }
        }
    }
    std::uint32_t* words = brick.words().data();
    for (std::size_t index = 0; index < kBrickVoxels; index += 4) {
        const std::uint32_t packed = static_cast<std::uint32_t>(bytes[index]) |
                                     (static_cast<std::uint32_t>(bytes[index + 1]) << 8) |
                                     (static_cast<std::uint32_t>(bytes[index + 2]) << 16) |
                                     (static_cast<std::uint32_t>(bytes[index + 3]) << 24);
        words[kBrickMaskWords + (index >> 2)] = packed;
        const std::uint32_t bits = (bytes[index] != 0 ? 1u : 0u) | (bytes[index + 1] != 0 ? 2u : 0u) |
                                   (bytes[index + 2] != 0 ? 4u : 0u) | (bytes[index + 3] != 0 ? 8u : 0u);
        words[index >> 5] |= bits << (index & 31u);
    }
}

void TerrainSampler::voxelize_trees(const glm::vec3& origin, float voxelEdge, Brick& brick) const {
    constexpr int N = kBrickEdge;
    const float brickEdge = voxelEdge * static_cast<float>(N);
    const Box brickBox{origin, origin + glm::vec3{brickEdge}};
    thread_local std::vector<std::uint32_t> touching;
    trees_touching(brickBox, touching);
    for (const std::uint32_t index : touching) {
        const TreePlacement& tree = trees_[index];
        const Box& b = treeBounds_[index];
        if (!world::generation::tree_intersects_box(tree, brickBox.min, brickBox.max)) {
            continue; // AABB touched, actual trunk/lobes do not
        }
        // Only the voxels whose centers can fall inside the tree's bounds.
        const auto lo = [&](float bmin, float omin) {
            return std::clamp(static_cast<int>(std::floor((bmin - omin) / voxelEdge - 0.5f)), 0, N - 1);
        };
        const auto hi = [&](float bmax, float omin) {
            return std::clamp(static_cast<int>(std::ceil((bmax - omin) / voxelEdge - 0.5f)), 0, N - 1);
        };
        const int x0 = lo(b.min.x, origin.x);
        const int x1 = hi(b.max.x, origin.x);
        const int y0 = lo(b.min.y, origin.y);
        const int y1 = hi(b.max.y, origin.y);
        const int z0 = lo(b.min.z, origin.z);
        const int z1 = hi(b.max.z, origin.z);
        for (int z = z0; z <= z1; ++z) {
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    const glm::vec3 center = origin + (glm::vec3{static_cast<float>(x), static_cast<float>(y),
                                                                 static_cast<float>(z)} +
                                                       0.5f) *
                                                          voxelEdge;
                    const MaterialID tm = world::generation::tree_material_at(tree, center);
                    if (tm == MaterialID::Air) {
                        continue;
                    }
                    // Terrain wins over canopy (no carving grass out of a hillside a lobe leans
                    // into); the trunk wins over everything (it is sunk into the ground on purpose).
                    const MaterialID current = brick.at(x, y, z);
                    const bool terrainSolid = current != MaterialID::Air && current != MaterialID::Water;
                    if (!terrainSolid || tm == MaterialID::Wood) {
                        brick.set(x, y, z, tm);
                    }
                }
            }
        }
    }
}

MaterialID TerrainSampler::material_at(const glm::vec3& voxelMin, float voxelEdge) const {
    float h = 0.0f;
    heightmap_->generate_column_heights_spaced(voxelMin.x, voxelMin.z, 1, 1, voxelEdge, &h);
    const float slope = field_.slope_at(voxelMin.x, voxelMin.z);
    const bool beach = h <= params_.sea_level + kBeachBand;
    const bool grassy = !beach && slope <= kGrassMaxSlope;
    MaterialID m = column_material(h, beach, grassy, voxelMin.y, voxelEdge);
    if (params_.trees) {
        const Box voxel{voxelMin, voxelMin + glm::vec3{voxelEdge}};
        thread_local std::vector<std::uint32_t> touching;
        trees_touching(voxel, touching);
        const glm::vec3 center = voxelMin + 0.5f * voxelEdge;
        for (const std::uint32_t index : touching) {
            const MaterialID tm = world::generation::tree_material_at(trees_[index], center);
            if (tm == MaterialID::Air) {
                continue;
            }
            const bool terrainSolid = m != MaterialID::Air && m != MaterialID::Water;
            if (!terrainSolid || tm == MaterialID::Wood) {
                m = tm;
            }
        }
    }
    return m;
}

} // namespace world::svo
