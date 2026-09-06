#include "world/collision/terrain_collider.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

#include "world/chunk/chunk_voxels.hpp" // kChunkSize

namespace world::collision {

using world::generation::TreePlacement;

namespace {

// Trees may lean this far into a neighboring chunk column (terrain_sampler.cpp's kTreeReach).
constexpr float kTreeReach = 8.0f;

} // namespace

TerrainCollider::TerrainCollider(const world::generation::HeightmapGenerator& heightmap,
                                 const TerrainColliderParams& params)
    : heightmap_(&heightmap), params_(params) {}

TerrainCollider::~TerrainCollider() = default; // worker_ joins (declared last)

std::unique_ptr<TerrainCollider::Cache> TerrainCollider::build_cache(const glm::vec3& center) const {
    const auto start = std::chrono::steady_clock::now();
    auto cache = std::make_unique<Cache>();
    const float half = params_.cache_extent * 0.5f;
    // Snap the cache origin to the cell grid so consecutive caches sample identical columns.
    cache->xMin = std::floor((center.x - half) / params_.cache_cell) * params_.cache_cell;
    cache->zMin = std::floor((center.z - half) / params_.cache_cell) * params_.cache_cell;
    cache->n = static_cast<std::int32_t>(std::ceil(params_.cache_extent / params_.cache_cell)) + 1;
    cache->heights.resize(static_cast<std::size_t>(cache->n) * static_cast<std::size_t>(cache->n));
    heightmap_->generate_column_heights_spaced(cache->xMin, cache->zMin, cache->n, cache->n,
                                               params_.cache_cell, cache->heights.data());
    cache->center = center;

    if (params_.trees) {
        const auto toChunk = [](float v) {
            return static_cast<std::int32_t>(std::floor(v / static_cast<float>(world::chunk::kChunkSize)));
        };
        const float xMax = cache->xMin + params_.cache_extent;
        const float zMax = cache->zMin + params_.cache_extent;
        for (std::int32_t cz = toChunk(cache->zMin - kTreeReach); cz <= toChunk(zMax + kTreeReach); ++cz) {
            for (std::int32_t cx = toChunk(cache->xMin - kTreeReach); cx <= toChunk(xMax + kTreeReach);
                 ++cx) {
                for (const TreePlacement& tree :
                     world::generation::compute_tree_placements(cx, cz, params_.seed, *heightmap_)) {
                    world::generation::TrunkBox trunk;
                    if (!world::generation::tree_trunk(tree, trunk)) {
                        continue; // shrubs: canopy only, walk through
                    }
                    Aabb box;
                    box.min =
                        glm::vec3{tree.world_x - trunk.half_width, trunk.y0, tree.world_z - trunk.half_width};
                    box.max =
                        glm::vec3{tree.world_x + trunk.half_width, trunk.y1, tree.world_z + trunk.half_width};
                    if (box.max.x > cache->xMin && box.min.x < xMax && box.max.z > cache->zMin &&
                        box.min.z < zMax) {
                        cache->trunks.push_back(Trunk{box});
                    }
                }
            }
        }
    }
    cache->buildMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return cache;
}

bool TerrainCollider::refresh(const glm::vec3& center) {
    bool adopted = false;
    if (pendingReady_.load(std::memory_order_acquire)) {
        cache_ = std::move(pending_);
        lastRefreshMs_ = cache_->buildMs;
        pendingReady_.store(false, std::memory_order_relaxed);
        pendingActive_.store(false, std::memory_order_relaxed);
        adopted = true;
    }
    if (cache_) {
        const float dx = std::abs(center.x - cache_->center.x);
        const float dz = std::abs(center.z - cache_->center.z);
        if (dx < params_.cache_extent * 0.25f && dz < params_.cache_extent * 0.25f) {
            return adopted;
        }
    }
    if (!cache_ || !params_.async) {
        // First cache (or synchronous mode): build it now so the body never runs without one.
        cache_ = build_cache(center);
        lastRefreshMs_ = cache_->buildMs;
        return true;
    }
    if (pendingActive_.load(std::memory_order_acquire)) {
        return adopted; // a rebuild is already running; adopt it next call
    }
    if (worker_.joinable()) {
        worker_.join(); // the previous worker has finished (its result was adopted above)
    }
    pendingActive_.store(true, std::memory_order_relaxed);
    worker_ = std::jthread([this, center] {
        pending_ = build_cache(center);
        pendingReady_.store(true, std::memory_order_release);
    });
    return adopted;
}

float TerrainCollider::voxel_top_of(float height) const noexcept {
    // The topmost solid voxel has its bottom at floor(h / e) * e (bottom <= h), so its top is one
    // edge higher.
    const float e = params_.voxel_edge;
    return std::floor(height / e) * e + e;
}

float TerrainCollider::cached_height(std::int32_t ix, std::int32_t iz) const noexcept {
    const Cache& c = *cache_;
    ix = std::clamp(ix, 0, c.n - 1);
    iz = std::clamp(iz, 0, c.n - 1);
    return c
        .heights[static_cast<std::size_t>(iz) * static_cast<std::size_t>(c.n) + static_cast<std::size_t>(ix)];
}

bool TerrainCollider::cache_covers(float x0, float z0, float x1, float z1) const noexcept {
    if (!cache_) {
        return false;
    }
    const Cache& c = *cache_;
    const float xMax = c.xMin + static_cast<float>(c.n - 1) * params_.cache_cell;
    const float zMax = c.zMin + static_cast<float>(c.n - 1) * params_.cache_cell;
    return x0 >= c.xMin && z0 >= c.zMin && x1 <= xMax && z1 <= zMax;
}

float TerrainCollider::ground_height(float x, float z) const {
    return heightmap_->height_at(x, z);
}

float TerrainCollider::voxel_top(float x, float z) const {
    // The voxel column containing (x, z) is sampled at its min corner.
    const float e = params_.voxel_edge;
    const float cx = std::floor(x / e) * e;
    const float cz = std::floor(z / e) * e;
    return voxel_top_of(heightmap_->height_at(cx, cz));
}

bool TerrainCollider::terrain_overlaps(const Aabb& box) const {
    const float cell = params_.cache_cell;
    if (cache_covers(box.min.x - cell, box.min.z - cell, box.max.x + cell, box.max.z + cell)) {
        // Every cache cell whose column range touches the footprint; a cell's height is bounded
        // conservatively by the max of its four corners (the surface is smooth at 3 cm scale).
        const Cache& c = *cache_;
        const auto ix0 = static_cast<std::int32_t>(std::floor((box.min.x - c.xMin) / cell));
        const auto iz0 = static_cast<std::int32_t>(std::floor((box.min.z - c.zMin) / cell));
        const auto ix1 = static_cast<std::int32_t>(std::floor((box.max.x - c.xMin) / cell));
        const auto iz1 = static_cast<std::int32_t>(std::floor((box.max.z - c.zMin) / cell));
        for (std::int32_t iz = iz0; iz <= iz1; ++iz) {
            for (std::int32_t ix = ix0; ix <= ix1; ++ix) {
                const float h = std::max(std::max(cached_height(ix, iz), cached_height(ix + 1, iz)),
                                         std::max(cached_height(ix, iz + 1), cached_height(ix + 1, iz + 1)));
                if (voxel_top_of(h) > box.min.y) {
                    return true;
                }
            }
        }
        return false;
    }
    // Outside the cache (a teleport, a rebuild still in flight, a test without refresh): direct
    // queries on a coarse grid of the footprint plus its corners. Exact at the samples,
    // conservative nowhere -- the cache is the real path.
    constexpr int kSamples = 5;
    for (int j = 0; j < kSamples; ++j) {
        for (int i = 0; i < kSamples; ++i) {
            const float x = box.min.x + (box.max.x - box.min.x) * static_cast<float>(i) / (kSamples - 1);
            const float z = box.min.z + (box.max.z - box.min.z) * static_cast<float>(j) / (kSamples - 1);
            if (voxel_top(x, z) > box.min.y) {
                return true;
            }
        }
    }
    return false;
}

bool TerrainCollider::trees_overlap(const Aabb& box) const {
    if (!cache_) {
        return false;
    }
    for (const Trunk& t : cache_->trunks) {
        if (t.box.intersects(box)) {
            return true;
        }
    }
    return false;
}

bool TerrainCollider::overlaps_solid(const Aabb& box) const {
    return terrain_overlaps(box) || trees_overlap(box);
}

} // namespace world::collision
