#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "engine/core/math.hpp"
#include "world/collision/solid_query.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_placement.hpp"

namespace world::collision {

struct TerrainColliderParams {
    int seed = 1337;
    float voxel_edge = 1.0f / 128.0f; // the finest voxel the renderer shows (TreeGeometry's)
    bool trees = true;                // tree trunks are solid; canopies are not
    // The local height cache: a square of `cache_extent` meters around the last refresh center,
    // sampled every `cache_cell` meters (513x513 samples at the defaults -- one SIMD grid call,
    // ~6 ms, built on a background thread). refresh() starts a rebuild once the body leaves the
    // inner half; until it lands, queries outside the old cache fall back to direct height
    // lookups.
    float cache_extent = 16.0f;
    float cache_cell = 1.0f / 32.0f;
    bool async = true; // false: refresh() builds the cache synchronously (tests, tools)
};

// The analytic world as a SolidQuery (docs/goals.md Group AA): the same height function the
// micro-voxel tree is sampled from ("a voxel is solid iff its bottom is at or below the surface
// height at its min corner", terrain_sampler.cpp), evaluated over a cached fine grid, plus the
// same deterministic tree placements' trunk boxes. Collision therefore agrees with what is drawn
// to within one cache cell on slopes, at any distance from the tree's build center, and it does
// not depend on which LOD the renderer happens to hold there. Not the octree itself: that is the
// follow-up once editing (goal 160) can make the analytic world stale.
class TerrainCollider {
public:
    TerrainCollider(const world::generation::HeightmapGenerator& heightmap,
                    const TerrainColliderParams& params);
    ~TerrainCollider();

    TerrainCollider(const TerrainCollider&) = delete;
    TerrainCollider& operator=(const TerrainCollider&) = delete;

    // Adopts a finished background cache, then starts a new one when `center` has left the
    // current cache's inner half (call every frame; cheap when nothing needs doing). Returns true
    // when a new cache became current this call. The first call (or async == false) builds
    // synchronously so the body never starts without a cache.
    bool refresh(const glm::vec3& center);

    // SolidQuery: true when any voxel column under the box's footprint rises into it, or a tree
    // trunk overlaps it. Outside the cache the terrain falls back to direct height queries.
    [[nodiscard]] bool overlaps_solid(const Aabb& box) const;

    // Top of the topmost solid voxel at (x, z): the surface the body actually rests on.
    [[nodiscard]] float voxel_top(float x, float z) const;
    // The exact analytic surface height (what walk mode's ground clamp used before collision).
    [[nodiscard]] float ground_height(float x, float z) const;

    [[nodiscard]] const TerrainColliderParams& params() const noexcept { return params_; }
    [[nodiscard]] std::size_t tree_count() const noexcept { return cache_ ? cache_->trunks.size() : 0; }
    [[nodiscard]] double last_refresh_ms() const noexcept { return lastRefreshMs_; }
    [[nodiscard]] bool refresh_pending() const noexcept { return pendingActive_.load(); }

private:
    struct Trunk {
        Aabb box;
    };
    // One built cache: heights[iz * n + ix] = surface at (xMin + ix * cell, zMin + iz * cell).
    struct Cache {
        std::vector<float> heights;
        std::int32_t n = 0;
        float xMin = 0.0f;
        float zMin = 0.0f;
        glm::vec3 center{0.0f};
        std::vector<Trunk> trunks;
        double buildMs = 0.0;
    };
    [[nodiscard]] std::unique_ptr<Cache> build_cache(const glm::vec3& center) const;
    [[nodiscard]] bool terrain_overlaps(const Aabb& box) const;
    [[nodiscard]] bool trees_overlap(const Aabb& box) const;
    [[nodiscard]] bool cache_covers(float x0, float z0, float x1, float z1) const noexcept;
    [[nodiscard]] float cached_height(std::int32_t ix, std::int32_t iz) const noexcept;
    [[nodiscard]] float voxel_top_of(float height) const noexcept;

    const world::generation::HeightmapGenerator* heightmap_;
    TerrainColliderParams params_;
    std::unique_ptr<Cache> cache_; // current; read by overlaps_solid on the calling thread
    double lastRefreshMs_ = 0.0;

    // Background rebuild: the worker writes `pending_` then flips pendingReady_; refresh() adopts
    // it on the calling thread. At most one worker at a time.
    std::unique_ptr<Cache> pending_;
    std::atomic<bool> pendingReady_{false};
    std::atomic<bool> pendingActive_{false};
    std::jthread worker_;
};

static_assert(SolidQuery<TerrainCollider>);

} // namespace world::collision
