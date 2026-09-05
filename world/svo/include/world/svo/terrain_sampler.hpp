#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_placement.hpp"
#include "world/svo/brick.hpp"
#include "world/svo/height_field.hpp"
#include "world/svo/sampler.hpp"

namespace world::svo {

struct TerrainSamplerParams {
    int seed = 1337;
    float sea_level = 0.0f;
    bool trees = true;
    float height_field_cell = 0.5f; // HeightField base cell size (meters)
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

    // Pointwise reference: the material of the voxel of edge `voxelEdge` whose min corner is `p`.
    [[nodiscard]] world::chunk::MaterialID material_at(const glm::vec3& voxelMin, float voxelEdge) const;

    [[nodiscard]] const HeightField& height_field() const noexcept { return field_; }
    [[nodiscard]] const std::vector<world::generation::TreePlacement>& trees() const noexcept {
        return trees_;
    }
    [[nodiscard]] const TerrainSamplerParams& params() const noexcept { return params_; }

    // Process-wide diagnostics: FastNoise2 grid calls made by fill_brick and column-cache hits.
    static std::uint64_t debug_grid_calls() noexcept;
    static std::uint64_t debug_grid_cache_hits() noexcept;

    static constexpr float kBeachBand = 1.75f;    // fill_terrain's kBeachBand
    static constexpr float kSoilDepth = 3.0f;     // fill_terrain's kSoilDepth, in meters
    static constexpr float kGrassMaxSlope = 1.9f; // fill_terrain's kGrassMaxSlope

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
    void trees_touching(const Box& box, std::vector<std::uint32_t>& out) const;

    // Column material rule shared by fill_brick and material_at.
    [[nodiscard]] world::chunk::MaterialID column_material(float surfaceHeight, bool beach, bool grassy,
                                                           float voxelBottom, float voxelEdge) const noexcept;
    // fill_brick's surface-straddling path: banded fill of every column from the 8x8 height grid
    // `h` plus four 1 m-offset slope grids.
    void fill_columns(const glm::vec3& origin, float voxelEdge, const std::array<float, 64>& h,
                      Brick& brick) const;
    void voxelize_trees(const glm::vec3& origin, float voxelEdge, Brick& brick) const;
    static void fill_layer(Brick& brick, int j, world::chunk::MaterialID material) noexcept;

    const world::generation::HeightmapGenerator* heightmap_;
    TerrainSamplerParams params_;
    HeightField field_;
    std::vector<std::unique_ptr<HeightField>> focusFields_; // set_focus's tiers, finest first
    std::vector<world::generation::TreePlacement> trees_;
    std::vector<Box> treeBounds_;
    TreeGrid treeGrid_;
};

static_assert(VoxelSampler<TerrainSampler>);

} // namespace world::svo
