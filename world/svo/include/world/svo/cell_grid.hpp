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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
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

    /// Storage index of a coordinate, and its inverse. Both are pure arithmetic -- nothing that
    /// looks a cell up should have to build a coordinate vector to do it.
    [[nodiscard]] std::size_t index_of(glm::ivec3 coord) const noexcept {
        const glm::ivec3 l = coord - originCell_;
        return static_cast<std::size_t>(l.x) +
               static_cast<std::size_t>(dims_.x) *
                   (static_cast<std::size_t>(l.y) +
                    static_cast<std::size_t>(dims_.y) * static_cast<std::size_t>(l.z));
    }
    [[nodiscard]] glm::ivec3 coord_of(std::size_t index) const noexcept {
        const auto x = static_cast<int>(index % static_cast<std::size_t>(dims_.x));
        const auto rest = index / static_cast<std::size_t>(dims_.x);
        const auto y = static_cast<int>(rest % static_cast<std::size_t>(dims_.y));
        const auto z = static_cast<int>(rest / static_cast<std::size_t>(dims_.y));
        return originCell_ + glm::ivec3{x, y, z};
    }

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


namespace detail {

// The ray, normalised into GRID CELL units: one unit is one cell edge, and the origin is the grid's
// own min corner. Working in these units is what makes the DDA below the textbook one rather than a
// version with `cell_edge` sprinkled through it.
struct GridRay {
    glm::vec3 o{0.0f};
    glm::vec3 d{0.0f};
    glm::vec3 invd{0.0f};
};

inline GridRay to_grid(const CellGrid& grid, const Ray& ray) noexcept {
    const float edge = grid.cell_edge();
    GridRay g;
    g.o = (ray.origin - grid.world_min()) / edge;
    g.d = ray.dir / edge;
    for (int a = 0; a < 3; ++a) {
        g.invd[a] = std::abs(g.d[a]) > 1.0e-20f ? 1.0f / g.d[a] : (g.d[a] >= 0.0f ? 1.0e30f : -1.0e30f);
    }
    return g;
}

// THE ONE WALK. Every grid form shares it, and what differs between them is the per-cell ACTION --
// `visit(coord, tCellEnter, tCellExit) -> std::optional<Hit>`. Returning a hit ends the walk.
//
// Passing the cell's own t-range is what goal 263 needs: a ray stepping into an ABSENT cell must
// consult the coarse proxy over THAT CELL'S SPAN and no further, because the proxy covers the whole
// region and would otherwise answer for cells that are resident and about to give a better answer a
// few steps later.
//
// Keeping the action a parameter rather than copying the DDA is deliberate: three near-identical
// Amanatides-Woo walks would drift, and the one that drifted would be the one nobody was testing.
template <typename Visit>
[[nodiscard]] Hit trace_grid_visit(const CellGrid& grid, const Ray& ray, const TraceParams& params,
                                   GridTraceStats* stats, Visit&& visit) noexcept {
    Hit miss;
    if (stats != nullptr) {
        *stats = GridTraceStats{};
    }
    if (grid.empty()) {
        return miss;
    }

    const GridRay g = to_grid(grid, ray);
    const glm::ivec3 dims = grid.dims();

    // Slab test against the whole grid, in cell units, so a ray that misses everything costs one
    // test rather than a walk.
    float tEnter = params.t_start > 0.0f ? params.t_start : 0.0f;
    float tExit = params.max_t;
    for (int a = 0; a < 3; ++a) {
        if (std::abs(g.d[a]) < 1.0e-20f) {
            // Parallel to this axis: constrained only by already being inside the slab.
            if (g.o[a] < 0.0f || g.o[a] >= static_cast<float>(dims[a])) {
                return miss;
            }
            continue;
        }
        const float t0 = (0.0f - g.o[a]) * g.invd[a];
        const float t1 = (static_cast<float>(dims[a]) - g.o[a]) * g.invd[a];
        tEnter = std::max(tEnter, std::min(t0, t1));
        tExit = std::min(tExit, std::max(t0, t1));
    }
    if (tExit < tEnter) {
        return miss;
    }

    // Amanatides & Woo, in cell units. The starting cell comes from the position at tEnter, clamped
    // -- a ray entering exactly on a face can land one cell out through float rounding, and clamping
    // is both cheaper and more robust than trying to make the arithmetic exact.
    glm::ivec3 cell;
    glm::ivec3 step;
    glm::vec3 tMax;
    glm::vec3 tDelta;
    const glm::vec3 entry = g.o + tEnter * g.d;
    for (int a = 0; a < 3; ++a) {
        cell[a] = std::clamp(static_cast<int>(std::floor(entry[a])), 0, dims[a] - 1);
        if (std::abs(g.d[a]) < 1.0e-20f) {
            step[a] = 0;
            tMax[a] = 1.0e30f;
            tDelta[a] = 1.0e30f;
            continue;
        }
        step[a] = g.d[a] > 0.0f ? 1 : -1;
        const float boundary = static_cast<float>(cell[a] + (step[a] > 0 ? 1 : 0));
        tMax[a] = (boundary - g.o[a]) * g.invd[a];
        tDelta[a] = std::abs(g.invd[a]);
    }

    // The walk. Cells are visited in ray order and each visit returns the nearest hit WITHIN its
    // cell, so the first hit returned is the globally nearest one.
    const glm::ivec3 originCell = grid.origin_cell();
    float cellEnter = tEnter;
    while (true) {
        if (stats != nullptr) {
            ++stats->cells_stepped;
        }
        const int axis = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
        const float cellExit = std::min(tMax[axis], tExit);
        if (const std::optional<Hit> hit = visit(originCell + cell, cellEnter, cellExit); hit) {
            if (stats != nullptr) {
                ++stats->cells_entered;
            }
            return *hit;
        }

        if (tMax[axis] > tExit) {
            return miss;
        }
        cell[axis] += step[axis];
        if (cell[axis] < 0 || cell[axis] >= dims[axis]) {
            return miss;
        }
        cellEnter = tMax[axis];
        tMax[axis] += tDelta[axis];
    }
}

// The common case: look a cell up and trace it whole. `viewOf(coord) -> TreeView`.
template <typename ViewOf>
[[nodiscard]] Hit trace_grid_with(const CellGrid& grid, const Ray& ray, const TraceParams& params,
                                  GridTraceStats* stats, ViewOf&& viewOf) noexcept {
    return trace_grid_visit(grid, ray, params, stats,
                            [&](glm::ivec3 coord, float, float) -> std::optional<Hit> {
                                const TreeView view = viewOf(coord);
                                if (view.empty()) {
                                    return std::nullopt;
                                }
                                // The cell's tree positions itself by geometry.origin, so the WORLD
                                // ray goes straight in -- no transform, and the LOD early-out still
                                // measures distance from the ray's own origin (goal 164).
                                const Hit hit = trace_ray(view, ray, params);
                                return hit.hit ? std::optional<Hit>{hit} : std::nullopt;
                            });
}

} // namespace detail

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


/// The GPU-shaped form of a grid: every cell's words concatenated into ONE node array and ONE brick
/// array, with a per-cell base offset into each.
///
/// Prompt 004 goal 256. This is not an optimisation of `CellGrid` -- it is the layout the SHADER
/// must have, hoisted onto the CPU so the two trace the same bytes rather than being two structures
/// kept in step by hand. A cell's internal offsets stay exactly as `build_tree` produced them
/// (relative to its own array) and the base is added at dereference, which is why `tree_layout.hpp`
/// needs no change and the 7,000-ray oracle keeps its meaning.
///
/// The record is deliberately four 32-bit words so a GPU can hold one `uint4` per cell.
struct FlatCell {
    std::uint32_t node_base = 0;  ///< word offset of this cell's first node
    std::uint32_t brick_base = 0; ///< BRICK index (not word offset) of this cell's first brick
    std::uint32_t root = 0;       ///< root header offset, relative to node_base
    std::uint32_t flags = 0;      ///< bit 0 = present; the rest is where AK-D's residency state goes
};
inline constexpr std::uint32_t kFlatCellPresent = 1u;
/// Bit 1: this cell IS resident and is known to contain nothing.
///
/// Prompt 004 goal 263 needed this distinction and did not have it. "No geometry here" and "not
/// loaded yet" are the same thing to a marcher -- both are a cell it cannot trace -- but they are
/// opposites to the never-stall fallback: an absent cell should be answered from the coarse proxy,
/// and an EMPTY one must not be, or the proxy's coarser voxels put geometry into space the fine
/// build correctly found to be empty. The test that caught this saw a proxy hit 0.3 m in front of
/// the fine one and it was not a rounding error.
inline constexpr std::uint32_t kFlatCellEmpty = 2u;

class FlatCellGrid {
public:
    FlatCellGrid() = default;
    /// Flattens `grid`. An absent cell gets a record with `flags == 0` and owns no storage.
    explicit FlatCellGrid(const CellGrid& grid);

    [[nodiscard]] const CellGrid& grid() const noexcept { return grid_; }
    [[nodiscard]] const std::vector<std::uint32_t>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<std::uint32_t>& bricks() const noexcept { return bricks_; }
    [[nodiscard]] const std::vector<FlatCell>& cells() const noexcept { return cells_; }
    [[nodiscard]] bool empty() const noexcept { return cells_.empty(); }
    [[nodiscard]] std::uint64_t memory_bytes() const noexcept {
        return static_cast<std::uint64_t>(nodes_.size() + bricks_.size()) * sizeof(std::uint32_t) +
               static_cast<std::uint64_t>(cells_.size()) * sizeof(FlatCell);
    }

    /// A `TreeView` onto cell `index`, or an empty view when the cell is absent. This is the exact
    /// arithmetic the shader performs, written down once.
    [[nodiscard]] TreeView view_of(std::size_t index) const noexcept;

private:
    CellGrid grid_;
    std::vector<std::uint32_t> nodes_;
    std::vector<std::uint32_t> bricks_;
    std::vector<FlatCell> cells_;
};

/// March the flattened grid. Must return exactly what the owning overload returns for the same
/// input -- asserted over 20,000 real-terrain rays, because "the flat form and the owning form
/// agree" is what lets the shader mirror one while the oracle guards the other.
[[nodiscard]] Hit trace_ray_grid(const FlatCellGrid& flat, const Ray& ray, const TraceParams& params = {},
                                 GridTraceStats* stats = nullptr) noexcept;

} // namespace world::svo
