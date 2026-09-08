#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/svo/caves.hpp"
#include "world/generation/tree_placement.hpp"
#include "world/generation/tree_volume.hpp"
#include "world/materials/terrain_query.hpp"
#include "world/svo/brick.hpp"
#include "world/svo/caves.hpp"
#include "world/svo/height_field.hpp"

#include "world/svo/sampler.hpp"
#include <array>
#include <memory>

namespace world::svo {

struct TerrainSamplerParams {
    int seed = 1337;
    float sea_level = 0.0f;
    bool trees = true;
    float height_field_cell = 0.5f; // HeightField base cell size (meters)
    // Goal 313, reopening goal 80. `threshold = 0` disables caves entirely and restores the exact
    // 2.5D world -- which is what `test_terrain_sampler.cpp`'s byte-equivalence against
    // `fill_terrain` runs with, since the mesh path has no cave rule.
    CaveParams caves{};
    // Prompt 007 goal 336. Trees within `skeleton_radius_m` of `skeleton_centre` are voxelized from
    // their GROWN SKELETON -- capsule branches and leaf clouds -- instead of the implicit
    // box-plus-octahedron. 0 keeps every tree implicit, which is what the terrain-sampler
    // equivalence test runs with and what every tool that has no camera gets.
    //
    // The radius is not taste. The LOD ladder makes the voxel edge at distance d
    // `max(finest, d * finest / lod_radius)`, so a branch of radius r stops being representable past
    // `d = lod_radius * 2r / finest`: 10 m for a 1 cm twig, 26 m for a 5 cm limb, 41 m for an 8 cm
    // trunk. Beyond that the two representations voxelize to the same blob. The default the app
    // passes is larger than 41 m only because the octree is built once and the camera keeps walking.
    float skeleton_radius_m = 0.0f;
    glm::vec3 skeleton_centre{0.0f};
};

// The world as a resolution-independent material field (research/micro-voxel-pivot-log.md §2.5):
// the SAME HeightmapGenerator and the SAME surface-banding rules terrain_fill.cpp applies to 1 m
// chunks, generalized from integer voxels to meters so they hold at any voxel size, plus the tree
// placements voxelized as the implicit shapes tree_placement.hpp defines. Occupancy rule kept
// bit-for-bit compatible with fill_terrain: a voxel is solid iff its BOTTOM face height <= the
// column's surface height (sampled at the voxel's min-corner (x,z)), so at voxel size 1 the
// sampler reproduces every shipped chunk exactly -- test_terrain_sampler.cpp proves it against
// fill_terrain itself, which is what ties the new representation to the old world.
//
// Band rules in meters, derived from fill_terrain's integer ones (depth = surface - y):
//   surface voxel : depth <  voxelEdge            (was: depth == 0)
//   soil band     : depth <  kSoilDepth+voxelEdge (was: depth <= kSoilDepth)
//   below         : Stone
//   water         : not solid AND bottom <= sea_level
class TerrainSampler {
public:
    // `region` bounds the trees and the height field this sampler will ever be asked about
    // (queries outside still answer, conservatively Mixed / pointwise).
    TerrainSampler(const world::generation::HeightmapGenerator& heightmap, const TerrainSamplerParams& params,
                   const Box& region);

    [[nodiscard]] BoxClassification classify(const Box& box) const;
    void fill_brick(const glm::vec3& origin, float voxelEdge, Brick& brick) const;

    // Builds a second, FINE height field (1/16 m cells) over the square of half-size `radius`
    // around `center`, used by classify() for every footprint it fully covers. The region-wide
    // field's 0.5 m cells bound sub-cell variation with a margin that is ~24 brick layers thick at
    // 6.25 cm bricks; near the camera -- where the builder subdivides to exactly those bricks --
    // that made it sample ~7 bricks for every one it kept (measured). Call before build_tree,
    // never concurrently with it (not thread-safe by design; classify() is).
    void set_focus(const glm::vec3& center, float radius);

    // Prompt 004 goal 251 (reopened by 254's measurement): the focus tiers, built once and SHARED.
    //
    // `set_focus` samples ~1.3 M noise points -- a 1/16 m field over `radius` plus a 1/8 m field over
    // 4x that -- and the cost does not shrink with the region being built. Measured: it is 5-9% of a
    // 512 m whole-region build and invisible, and ~100% of a 32 m cell build. Per-cell rebuild is
    // therefore not viable until this is hoisted out of the per-build path, which is why these are a
    // `shared_ptr` the caller can build once and hand to every sampler in a region.
    using FocusTiers = std::vector<std::shared_ptr<const HeightField>>;

    // The snapped rectangle a tier covers, so a cache can tell "same tier" from "same arguments".
    // Snapping is to whole coarse cells, exactly as set_focus does it, so two nearby centres that
    // round to the same rectangle share a field instead of building two identical ones.
    struct FocusKey {
        float x0 = 0.0f;
        float z0 = 0.0f;
        float extent = 0.0f;
        float cell = 0.0f;
        [[nodiscard]] friend bool operator==(const FocusKey&, const FocusKey&) = default;
    };

    // The two tiers `set_focus` would build for these arguments, as keys -- so a caller can look
    // them up in a cache before paying to build them.
    [[nodiscard]] static std::array<FocusKey, 2> focus_keys(const TerrainSamplerParams& params,
                                                            const glm::vec3& center, float radius) noexcept;
    [[nodiscard]] static std::shared_ptr<const HeightField>
    make_focus_tier(const world::generation::HeightmapGenerator& heightmap, const FocusKey& key);

    // Adopt prebuilt tiers instead of sampling them. Finest first, as set_focus orders them.
    void adopt_focus(FocusTiers tiers);

    /// The tiers this sampler holds, so a caller that has already paid for them can hand them to
    /// every cell of a grid instead of each cell sampling 1.3 M noise points of its own.
    [[nodiscard]] const FocusTiers& focus_tiers() const noexcept { return focusFields_; }

    // Pointwise reference: the material of the voxel of edge `voxelEdge` whose min corner is `p`.
    [[nodiscard]] world::chunk::MaterialID material_at(const glm::vec3& voxelMin, float voxelEdge) const;

    [[nodiscard]] const HeightField& height_field() const noexcept { return field_; }
    [[nodiscard]] const std::vector<world::generation::TreePlacement>& trees() const noexcept {
        return trees_;
    }
    [[nodiscard]] const TerrainSamplerParams& params() const noexcept { return params_; }

    // Goal 336's accounting: how many trees got a grown skeleton, what that cost to build, and how
    // many bytes the volumes hold. Reported by the app so the growth has a number rather than a
    // feeling.
    struct SkeletonStats {
        std::size_t trees = 0;
        std::size_t primitives = 0;
        std::size_t memory_bytes = 0;
        double seconds = 0.0;
    };
    [[nodiscard]] SkeletonStats skeleton_stats() const noexcept { return skeletonStats_; }

    // Process-wide diagnostics: FastNoise2 grid calls made by fill_brick and column-cache hits.
    static std::uint64_t debug_grid_calls() noexcept;
    static std::uint64_t debug_grid_cache_hits() noexcept;

    // The band constants are the materials' own (world/materials/terrain_query.hpp, Group AC) --
    // one copy, shared with fill_terrain and the aim query. These names stay for classify()'s
    // fast paths, which reason about the soil band and the beach directly.
    static constexpr float kBeachBand = world::materials::TerrainBands::beach_band;
    static constexpr float kSoilDepth = world::materials::TerrainBands::soil_depth;
    static constexpr float kGrassMaxSlope = world::materials::TerrainBands::grass_max_slope;

private:
    struct TreeEntry {
        world::generation::TreePlacement tree;
        Box bounds;
    };
    // Coarse XZ grid over the region holding the indices of trees whose bounds touch each cell.
    struct TreeGrid {
        float xMin = 0.0f;
        float zMin = 0.0f;
        float cell = 16.0f;
        std::int32_t nx = 0;
        std::int32_t nz = 0;
        std::vector<std::vector<std::uint32_t>> cells;
    };
    void collect_trees(const Box& region);
    void grow_skeletons();
    void trees_touching(const Box& box, std::vector<std::uint32_t>& out) const;

    // Column material rule shared by fill_brick and material_at.
    // Takes the world x/z as well as the column's height, because goal 313's cave carve is a
    // function of all three coordinates. Both callers already have them.
    [[nodiscard]] world::chunk::MaterialID column_material(float worldX, float worldZ, float surfaceHeight,
                                                           bool beach, bool grassy, float voxelBottom,
                                                           float voxelEdge) const noexcept;
    // fill_brick's surface-straddling path: banded fill of every column from the 8x8 height grid
    // `h` plus four 1 m-offset slope grids.
    void fill_columns(const glm::vec3& origin, float voxelEdge, const std::array<float, 64>& h,
                      Brick& brick) const;
    void voxelize_trees(const glm::vec3& origin, float voxelEdge, Brick& brick) const;
    // Writes one Y layer of the 512-byte material scratch; the brick is packed once afterwards.
    static void fill_layer(std::uint8_t* materials, int j, world::chunk::MaterialID material) noexcept;

    const world::generation::HeightmapGenerator* heightmap_;
    TerrainSamplerParams params_;
    HeightField field_;
    FocusTiers focusFields_; // set_focus's tiers, finest first; shared so builds can reuse them
    std::vector<world::generation::TreePlacement> trees_;
    std::vector<Box> treeBounds_;
    // Parallel to `trees_`, and EMPTY for every tree outside `skeleton_radius_m` -- an empty volume
    // is the signal to fall back to the implicit shape, so there is no second list to keep in step.
    std::vector<world::generation::TreeVolume> treeVolumes_;
    TreeGrid treeGrid_;
    SkeletonStats skeletonStats_{};
};

static_assert(VoxelSampler<TerrainSampler>);

} // namespace world::svo
