#pragma once

// Prompt 004 goal 255: the grid of shallow trees -- the CPU reference.
//
// THE DESIGN, and why it looks like so little code. The ranking is argued in full in
// research/frame-time-and-gpu-architecture-log.md section 5; the conclusion that shapes this header
// is that a grid of independent cells needs NO ENCODING CHANGE at all:
//
//   * A cell is exactly today's `BrickTree`, built with `root_size_log2 = 5` (32 m) instead of 9,
//     and positioned by its own `geometry.origin`. Same header word, same layout-v2 attribute word,
//     same `node_child_slot`. `tree_layout.hpp` is untouched, so the 7,000-ray oracle keeps its
//     meaning against every cell.
//   * There are NO cross-cell pointers. A ray leaving a cell does not follow one -- it steps the
//     grid arithmetically and enters the next cell at its root. That is why ESVO's relative
//     addressing (a `far` bit plus a per-block far-pointer table) is not merely second-ranked here
//     but MOOT: it exists to let a piece of one tree move while the rest stays valid, and a cell
//     already moves as a unit because nothing outside it points in.
//   * `trace_ray` already works in WORLD space -- it normalises by `tree.geometry.origin` itself --
//     so a cell needs no coordinate wrapper. That is the property that makes this file short.
//
// WHY IT IS WORTH DOING, in the two numbers the design rests on:
//
//   * REBUILD COST. Terrain is a surface, so a rebuild costs by AREA. One 32 m cell is (32/512)^2 =
//     1/256 of a 512 m region's footprint, against a measured 1.89-3.60 s whole-region build. That
//     is what turns "rebuild the world" into something a camera cannot outrun.
//   * CHAIN LENGTH. 16 levels become 12 -- a 25% shorter chain of dependent, cache-missing loads per
//     ray, and a 13-deep stack instead of 22 in the shader. Section 14 measured that a 29% cut in
//     traversal steps buys 19% of march time on this hardware, so this should be costed at about
//     0.65x whatever step reduction it produces. Goal 256 measures it rather than pre-crediting it.

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "engine/core/math.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/ray_trace.hpp"

namespace world::svo {

/// One cell of the grid: a whole `BrickTree` covering a `cell_edge` cube, or absent.
///
/// ABSENT IS A FLAG, NOT A NULL TREE, and the distinction is load-bearing for AK-D: a ray entering
/// an absent cell is the exact hook GigaVoxels' "never stall -- shade from the coarsest resident
/// level and file a request" rule attaches to. Today an absent cell is simply skipped.
struct GridCell {
    std::shared_ptr<const BrickTree> tree; ///< null = absent
    [[nodiscard]] bool present() const noexcept { return tree != nullptr && !tree->empty(); }
};

/// A rectangular block of equally sized cells covering a region of world space.
///
/// Cells are indexed `[x + dims.x * (y + dims.y * z)]` from `origin_cell`, and each cell's own
/// `BrickTree::geometry.origin` is what places it -- this class never transforms a ray.
class CellGrid {
public:
    CellGrid() = default;
    CellGrid(glm::ivec3 origin_cell, glm::ivec3 dims, int cell_size_log2, int voxel_size_log2) noexcept
        : originCell_(origin_cell), dims_(dims), cellSizeLog2_(cell_size_log2),
          voxelSizeLog2_(voxel_size_log2),
          cells_(static_cast<std::size_t>(dims.x) * static_cast<std::size_t>(dims.y) *
                 static_cast<std::size_t>(dims.z)) {}

    [[nodiscard]] glm::ivec3 dims() const noexcept { return dims_; }
    [[nodiscard]] glm::ivec3 origin_cell() const noexcept { return originCell_; }
    [[nodiscard]] int cell_size_log2() const noexcept { return cellSizeLog2_; }
    [[nodiscard]] int voxel_size_log2() const noexcept { return voxelSizeLog2_; }
    [[nodiscard]] float cell_edge() const noexcept { return std::ldexp(1.0f, cellSizeLog2_); }
    /// Levels inside one cell -- the number goal 254's arithmetic is about (12 at the design's
    /// 32 m / 7.8 mm, against 16 for a 512 m region).
    [[nodiscard]] int levels_per_cell() const noexcept { return cellSizeLog2_ - voxelSizeLog2_; }
    [[nodiscard]] std::size_t cell_count() const noexcept { return cells_.size(); }
    [[nodiscard]] std::size_t present_count() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return cells_.empty(); }

    /// World-space bounds of the whole grid.
    [[nodiscard]] glm::vec3 world_min() const noexcept { return glm::vec3{originCell_} * cell_edge(); }
    [[nodiscard]] glm::vec3 world_max() const noexcept {
        return glm::vec3{originCell_ + dims_} * cell_edge();
    }

    /// The `TreeGeometry` a cell at absolute grid coordinate `coord` must be built with.
    [[nodiscard]] TreeGeometry geometry_for(glm::ivec3 coord) const noexcept {
        TreeGeometry g;
        g.origin = glm::vec3{coord} * cell_edge();
        g.root_size_log2 = cellSizeLog2_;
        g.voxel_size_log2 = voxelSizeLog2_;
        return g;
    }

    /// Absolute grid coordinates this grid covers, in storage order.
    [[nodiscard]] std::vector<glm::ivec3> coords() const;

    [[nodiscard]] bool contains(glm::ivec3 coord) const noexcept {
        const glm::ivec3 local = coord - originCell_;
        return local.x >= 0 && local.y >= 0 && local.z >= 0 && local.x < dims_.x && local.y < dims_.y &&
               local.z < dims_.z;
    }
    [[nodiscard]] const GridCell* at(glm::ivec3 coord) const noexcept;
    void set(glm::ivec3 coord, std::shared_ptr<const BrickTree> tree) noexcept;

    /// Aggregate statistics over every present cell -- what a resident-memory budget is checked
    /// against, and what the ramp reports.
    struct Totals {
        std::size_t present_cells = 0;
        std::size_t bricks = 0;
        std::size_t node_words = 0;
        std::uint64_t bytes = 0;
    };
    [[nodiscard]] Totals totals() const noexcept;

private:
    glm::ivec3 originCell_{0};
    glm::ivec3 dims_{0};
    int cellSizeLog2_ = 5;
    int voxelSizeLog2_ = -7;
    std::vector<GridCell> cells_;
};

/// Diagnostics for one grid trace: how much of the cost is the grid walk rather than the octrees.
struct GridTraceStats {
    std::uint32_t cells_stepped = 0; ///< cells the DDA visited, present or not
    std::uint32_t cells_entered = 0; ///< of those, the ones actually traced
};

/// March `ray` through the grid, returning the first hit in any cell.
///
/// The grid is walked with an Amanatides-Woo 3D DDA and each present cell it enters is traced with
/// the ordinary `trace_ray`. Because cells are visited in ray order and `trace_ray` returns the
/// nearest hit WITHIN a cell, the first hit found is the globally nearest one -- there is nothing
/// else to prove and no ordering to maintain.
///
/// `params` is passed through unchanged, so the LOD early-out still measures distance from the ray's
/// own origin (goal 164's rule) and `max_t` still bounds the whole march.
[[nodiscard]] Hit trace_ray_grid(const CellGrid& grid, const Ray& ray, const TraceParams& params = {},
                                 GridTraceStats* stats = nullptr) noexcept;

} // namespace world::svo
