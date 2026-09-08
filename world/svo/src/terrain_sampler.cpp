#include "world/svo/terrain_sampler.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>

#include "world/chunk/chunk_voxels.hpp" // kChunkSize
#include "world/generation/field/field_sampler.hpp"

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
        grow_skeletons();
    } else {
        treeVolumes_.clear();
    }
    // Grass rides the tree acceleration structure as pseudo-tree entries, so it must be appended
    // BEFORE the grid is built -- and the grid build moved out of collect_trees for exactly that.
    place_grass();
    build_tree_grid(region);
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

}

void TerrainSampler::build_tree_grid(const Box& region) {
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

void TerrainSampler::grow_skeletons() {
    treeVolumes_.assign(trees_.size(), world::generation::TreeVolume{});
    if (params_.skeleton_radius_m <= 0.0f || trees_.empty()) {
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    const float r2 = params_.skeleton_radius_m * params_.skeleton_radius_m;
    for (std::size_t i = 0; i < trees_.size(); ++i) {
        const float dx = trees_[i].world_x - params_.skeleton_centre.x;
        const float dz = trees_[i].world_z - params_.skeleton_centre.z;
        if (dx * dx + dz * dz > r2) {
            continue;
        }
        world::generation::TreeVolume volume = world::generation::tree_volume_for(params_.seed, trees_[i]);
        if (volume.empty()) {
            continue;
        }
        // The volume's own bounds REPLACE the placement's: a grown crown does not fill the
        // octahedron exactly, and a bound that is not the one the geometry uses is either a hole
        // (too small) or wasted subdivision (too large).
        treeBounds_[i] = Box{volume.bounds().min, volume.bounds().max};
        ++skeletonStats_.trees;
        skeletonStats_.primitives += volume.primitives().size();
        skeletonStats_.memory_bytes += volume.memory_bytes();
        treeVolumes_[i] = std::move(volume);
    }
    skeletonStats_.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}


void TerrainSampler::place_grass() {
    if (params_.grass_radius_m <= 0.0f) {
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    const world::generation::GrassCoverParams& gp = params_.grass;
    const float radius = params_.grass_radius_m;
    const glm::vec3 centre = params_.skeleton_centre;

    // Ground and slope come from the SAME field the terrain does. A tuft placed against a different
    // surface floats or buries, and the two would have to be kept in step forever.
    const auto height_at = [this](float x, float z) { return heightmap_->height_at(x, z); };
    const auto slope_at = [this](float x, float z) { return field_.slope_at(x, z); };
    // The biome plane, read the same way `tree_placement.cpp` reads it -- Catmull-Rom through the
    // macro field, rounded to an index. A world with no macro field (a tool, a test) gets
    // Grassland, which is the density the pre-biome world had.
    const world::generation::field::TerrainField* macro = heightmap_->macro_field();
    const auto biome_at = [macro](float x, float z) {
        if (macro == nullptr) {
            return world::generation::field::Biome::Grassland;
        }
        const auto index = static_cast<std::uint8_t>(std::lround(
            world::generation::field::FieldSampler{*macro, world::generation::field::Plane::Biome}
                .value_at(x, z)));
        return index < static_cast<std::uint8_t>(world::generation::field::Biome::Count)
                   ? static_cast<world::generation::field::Biome>(index)
                   : world::generation::field::Biome::Grassland;
    };

    const auto patchOf = [&](float v) { return static_cast<std::int32_t>(std::floor(v / gp.patch_edge_m)); };
    const std::int32_t px0 = patchOf(centre.x - radius);
    const std::int32_t px1 = patchOf(centre.x + radius);
    const std::int32_t pz0 = patchOf(centre.z - radius);
    const std::int32_t pz1 = patchOf(centre.z + radius);

    std::vector<world::generation::GrassTuft> tufts;
    for (std::int32_t pz = pz0; pz <= pz1; ++pz) {
        for (std::int32_t px = px0; px <= px1; ++px) {
            const glm::vec2 origin{static_cast<float>(px) * gp.patch_edge_m,
                                   static_cast<float>(pz) * gp.patch_edge_m};
            // Whole-patch reject on distance, so the ring is a ring and not a square.
            const glm::vec2 mid = origin + glm::vec2{0.5f * gp.patch_edge_m};
            const float dx = mid.x - centre.x;
            const float dz = mid.y - centre.z;
            if (dx * dx + dz * dz > (radius + gp.patch_edge_m) * (radius + gp.patch_edge_m)) {
                continue;
            }
            tufts = world::generation::grass_tufts_in_patch(params_.seed, origin, gp, height_at, slope_at,
                                                            biome_at);
            if (tufts.empty()) {
                continue;
            }
            world::generation::TreeVolume volume = world::generation::grass_patch_volume(tufts, gp);
            if (volume.empty()) {
                continue;
            }
            // A pseudo-tree entry: the PLACEMENT is never consulted, because every consumer checks
            // `treeVolumes_[i].empty()` first and takes the volume when it is not. That is what
            // lets grass reuse the tree grid, the box classifier and the voxelizer unchanged.
            skeletonStats_.grass_patches += 1;
            skeletonStats_.grass_tufts += tufts.size();
            skeletonStats_.grass_blades += volume.primitives().size();
            skeletonStats_.grass_bytes += volume.memory_bytes();
            trees_.emplace_back();
            treeBounds_.push_back(Box{volume.bounds().min, volume.bounds().max});
            treeVolumes_.push_back(std::move(volume));
        }
    }
    skeletonStats_.grass_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
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

MaterialID TerrainSampler::column_material(float worldX, float worldZ, float surfaceHeight, bool beach,
                                           bool grassy, float voxelBottom, float voxelEdge) const noexcept {
    // GOAL 313: the carve, at the one place both the brick fill and the pointwise query pass
    // through -- so they cannot disagree about where a cave is.
    //
    // The voxel's CENTRE is the sample point, not its bottom corner. The occupancy rule uses the
    // bottom face because that is what makes the sparse-brick and 1 m chunk worlds byte-identical,
    // but a cave is a volume and sampling its boundary at a face would make a voxel's fate depend
    // on which side of the face the noise fell.
    if (params_.caves.enabled() && voxelBottom <= surfaceHeight) {
        const glm::vec3 centre{worldX + 0.5f * voxelEdge, voxelBottom + 0.5f * voxelEdge,
                               worldZ + 0.5f * voxelEdge};
        if (cave_at(params_.caves, centre, surfaceHeight)) {
            // Below the water table nothing is carved (cave_at enforces it), so a carved voxel is
            // always above it and always dry: air, not water.
            return MaterialID::Air;
        }
    }
    // The band rule is the materials' own (Group AC): each component claims its band at this voxel
    // size -- the same function fill_terrain evaluates at 1 m, which is what makes the two worlds
    // byte-identical there (test_terrain_sampler.cpp).
    const world::materials::TerrainQuery query{surfaceHeight,     voxelBottom, voxelEdge,
                                               params_.sea_level, beach,       grassy};
    return world::materials::terrain_material(query);
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
            const world::generation::TreeVolume& volume = treeVolumes_[index];
            const bool grown = !volume.empty();
            if (!(grown ? volume.intersects_box(box.min, box.max)
                        : world::generation::tree_intersects_box(trees_[index], box.min, box.max))) {
                continue;
            }
            if (grown ? volume.contains_box(box.min, box.max)
                      : world::generation::tree_lobe_contains_box(trees_[index], box.min, box.max)) {
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
    // GOAL 313, AND THE ONE PLACE CAVES CAN PUT A HOLE IN THE WORLD. Every conclusion below this
    // line says "this whole box is solid" WITHOUT SUBDIVIDING, which is exactly the reasoning a
    // cave invalidates. `caves_possible_in_band` is conservative -- false means provably cave-free
    // -- so a box the band cannot reach keeps its fast path unchanged and a box the band touches is
    // subdivided until the per-voxel carve in `fill_columns` can see it.
    //
    // The subdivision cost is confined to the band: with the shipped 6-55 m depths that is 49 m of
    // a world whose columns run to ~60 m, and nothing above the surface or below the water table
    // pays anything.
    if (caves_possible_in_band(params_.caves, box.min.y, box.max.y, r.min, r.max)) {
        return {BoxClass::Mixed, MaterialID::Air};
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
    for (const FocusKey& key : focus_keys(params_, center, radius)) {
        focusFields_.push_back(make_focus_tier(*heightmap_, key));
    }
}

std::array<TerrainSampler::FocusKey, 2> TerrainSampler::focus_keys(const TerrainSamplerParams& params,
                                                                   const glm::vec3& center,
                                                                   float radius) noexcept {
    // The snapping is the same as the original set_focus: each square is aligned to whole COARSE
    // cells so no tier boundary splits one. Factored out so a cache can key on the result -- two
    // camera positions a few centimetres apart snap to the same rectangle and should share a field
    // rather than sample 1.3 M noise points twice.
    const float coarse = params.height_field_cell;
    const auto key = [&](float r, float cell) {
        FocusKey k;
        k.x0 = std::floor((center.x - r) / coarse) * coarse;
        k.z0 = std::floor((center.z - r) / coarse) * coarse;
        k.extent = std::ceil(2.0f * r / coarse) * coarse;
        k.cell = cell;
        return k;
    };
    return {key(radius, 1.0f / 16.0f), key(4.0f * radius, 1.0f / 8.0f)};
}

std::shared_ptr<const HeightField>
TerrainSampler::make_focus_tier(const world::generation::HeightmapGenerator& heightmap, const FocusKey& key) {
    return std::make_shared<const HeightField>(heightmap, key.x0, key.z0, key.extent, key.cell);
}

void TerrainSampler::adopt_focus(FocusTiers tiers) {
    focusFields_ = std::move(tiers);
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
    // Goal 258: every path below writes into this byte scratch and packs ONCE. Before the palette
    // the uniform paths could write material words straight into the brick, because a material was
    // a byte at a fixed offset; a palette index is 3 bits and its value depends on what else the
    // brick holds, so "write this layer" is no longer a local edit. Packing once from a scratch is
    // both correct and the same shape `fill_columns` already used for the general case.
    std::array<std::uint8_t, kBrickVoxels> materials{};
    // The builder's box classification is deliberately conservative, so many bricks it asks for
    // turn out homogeneous. Decide those from the ONE height grid just fetched and skip the four
    // slope grids -- the only per-column work left is water vs. air (no material band involved).
    if (origin.y > hRange.max) {
        if (origin.y <= params_.sea_level) {
            // Water fills every layer whose bottom is at or below sea level.
            for (int j = 0; j < N; ++j) {
                if (origin.y + static_cast<float>(j) * voxelEdge <= params_.sea_level) {
                    fill_layer(materials.data(), j, MaterialID::Water);
                }
            }
        }
        brick_pack_materials(brick.words().data(), materials.data());
    } else if (brickTop <= hRange.min - kSoilDepth - voxelEdge) {
        // Every voxel is at least kSoilDepth + voxelEdge below the lowest column: solid stone.
        for (int j = 0; j < N; ++j) {
            fill_layer(materials.data(), j, MaterialID::Stone);
        }
        brick_pack_materials(brick.words().data(), materials.data());
    } else {
        fill_columns(origin, voxelEdge, h, brick);
    }

    if (!params_.trees) {
        return;
    }
    voxelize_trees(origin, voxelEdge, brick);
}

void TerrainSampler::fill_layer(std::uint8_t* materials, int j, MaterialID material) noexcept {
    // One Y layer = 8 runs of 8 consecutive linear indices (x innermost), so it is eight 8-byte
    // stores into the scratch. The brick itself is written once, by brick_pack_materials.
    const auto m = static_cast<std::uint8_t>(material);
    for (int k = 0; k < kBrickEdge; ++k) {
        std::uint8_t* run = materials + brick_voxel_index(0, j, k);
        for (int i = 0; i < kBrickEdge; ++i) {
            run[i] = m;
        }
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
                column[static_cast<std::size_t>(j) * N] = static_cast<std::uint8_t>(
                    column_material(origin.x + static_cast<float>(i) * voxelEdge,
                                    origin.z + static_cast<float>(k) * voxelEdge, surface, beach, grassy,
                                    bottom, voxelEdge));
            }
        }
    }
    brick_pack_materials(brick.words().data(), bytes.data());
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
        const world::generation::TreeVolume& volume = treeVolumes_[index];
        const bool grown = !volume.empty();
        if (!(grown ? volume.intersects_box(brickBox.min, brickBox.max)
                    : world::generation::tree_intersects_box(tree, brickBox.min, brickBox.max))) {
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
                    const MaterialID tm = grown ? volume.material_at(center)
                                                : world::generation::tree_material_at(tree, center);
                    if (tm == MaterialID::Air) {
                        continue;
                    }
                    // Terrain wins over canopy (no carving grass out of a hillside a lobe leans
                    // into); the trunk wins over everything (it is sunk into the ground on purpose)
                    // -- the materials' own yields_to_trees / overrides_terrain flags.
                    if (world::materials::tree_replaces(brick.at(x, y, z), tm)) {
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
    MaterialID m = column_material(voxelMin.x, voxelMin.z, h, beach, grassy, voxelMin.y, voxelEdge);
    if (params_.trees) {
        const Box voxel{voxelMin, voxelMin + glm::vec3{voxelEdge}};
        thread_local std::vector<std::uint32_t> touching;
        trees_touching(voxel, touching);
        const glm::vec3 center = voxelMin + 0.5f * voxelEdge;
        for (const std::uint32_t index : touching) {
            const world::generation::TreeVolume& volume = treeVolumes_[index];
            const MaterialID tm = volume.empty()
                                      ? world::generation::tree_material_at(trees_[index], center)
                                      : volume.material_at(center);
            if (tm == MaterialID::Air) {
                continue;
            }
            if (world::materials::tree_replaces(m, tm)) {
                m = tm;
            }
        }
    }
    return m;
}

} // namespace world::svo
