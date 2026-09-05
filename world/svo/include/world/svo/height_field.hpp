#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "world/generation/heightmap_generator.hpp"

namespace world::svo {

// Conservative min/max surface-height pyramid over a square world region (research/micro-voxel-
// pivot-log.md §2.4): the builder classifies whole octree boxes as "entirely above every column's
// surface" / "entirely below every column's soil band" from this instead of sampling voxels, which
// is what makes an octree over a 512 m region cost surface-bricks, not volume.
//
// Built from ONE FastNoise2 grid call at `cell_size` spacing (corner samples), then min/max mips.
// Sub-cell height variation is covered by a PER-CELL margin (a fraction of the cell's own corner
// range plus a floor, folded into the base level so every coarser level inherits it) -- an
// empirical local Lipschitz-style bound on this specific noise, VERIFIED by
// test_height_field.cpp's dense re-sampling rather than assumed (there is no closed-form gradient
// bound for FastNoise2's Simplex/FBm worth trusting over a measurement). A query narrower than one
// cell answers with its enclosing cells' range. margin() reports the largest per-cell margin.
class HeightField {
public:
    struct Range {
        float min = 0.0f;
        float max = 0.0f;
    };

    // Samples corners of an N x N cell grid covering [xMin, xMin + extent) x [zMin, zMin + extent);
    // extent is rounded UP to a whole number of cells.
    HeightField(const world::generation::HeightmapGenerator& heightmap, float xMin, float zMin, float extent,
                float cellSize);

    // Conservative surface-height range over the footprint [x0, x1] x [z0, z1] (world meters):
    // guaranteed to contain the true min/max of every column inside it, margin included.
    [[nodiscard]] Range range(float x0, float z0, float x1, float z1) const noexcept;

    // True when the footprint lies entirely inside the sampled area (queries outside are clamped
    // to the edge cells and are NOT sound -- callers must check when a field is not region-wide).
    [[nodiscard]] bool covers(float x0, float z0, float x1, float z1) const noexcept;

    [[nodiscard]] float margin() const noexcept { return margin_; }
    [[nodiscard]] float cell_size() const noexcept { return cellSize_; }
    [[nodiscard]] std::int32_t cell_count() const noexcept { return cells_; }
    [[nodiscard]] Range whole_range() const noexcept;

    // The terrain's per-axis central-difference slope at a FIXED 1 m baseline
    // (max(|h(x+1,z)-h(x-1,z)|, |h(x,z+1)-h(x,z-1)|)/2 -- fill_terrain's grass-vs-rock test),
    // bilinearly interpolated from the corner samples. EXACT at corner positions whose +-1 m
    // neighbors are also corners (every integer column of a 0.5 m field with an integer origin,
    // which is what keeps the sampler byte-identical to fill_terrain at 1 m), interpolated in
    // between, clamped at the field's edge. Replaces four extra noise-grid calls per brick.
    [[nodiscard]] float slope_at(float x, float z) const noexcept;

private:
    struct Level {
        std::int32_t size = 0; // cells per axis at this level
        std::vector<float> min;
        std::vector<float> max;
    };
    std::vector<Level> levels_; // levels_[0] = per base cell (corner min/max), coarser after
    std::vector<float> slopes_; // per corner, (cells_+1)^2
    float xMin_ = 0.0f;
    float zMin_ = 0.0f;
    float cellSize_ = 1.0f;
    std::int32_t cells_ = 0;
    float margin_ = 0.0f;
};

} // namespace world::svo
