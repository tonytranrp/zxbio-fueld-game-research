#pragma once

// Prompt 004 goal 260 (and the upload half of 257): the grid as it lives on the GPU, with bricks in
// persistent pool slots so a cell that did not change is never re-sent.
//
// THE PROBLEM. Goal 257 made the BUILD incremental -- a camera move rebuilds 138 of 4,096 cells
// instead of all of them -- and then the upload undid it, because `FlatCellGrid` concatenates every
// cell into one array, so 138 changed cells still re-sent all 292.9 MB.
//
// THE SPLIT, and it is chosen from where the bytes actually are (research section 21):
//
//   * BRICKS, 94.6% of the resident bytes and all exactly `kBrickWords` long, live in a
//     fixed-capacity `BrickPool`. A cell keeps its slots across rebuilds, so an unchanged cell
//     costs ZERO upload bytes. Only the slots that actually changed are dirty.
//   * NODES, 5.4%, are repacked and re-sent whole on every rebuild. That is a deliberate
//     simplification rather than an oversight: node arrays are variable-length, so pooling them
//     needs variable-size spans and the compaction that comes with them, and the whole node array
//     is ~30 MB against ~520 MB of bricks. Re-sending 30 MB to avoid writing a compacting allocator
//     is the right trade at this ratio, and if the ratio ever changes this comment is the reason to
//     revisit it.
//
// WHAT MAKES IT WORK. A brick leaf's payload word holds a brick INDEX, and this class rewrites that
// index from the cell's own numbering to the POOL's when the cell is installed. That is the one
// piece of surgery on the built bytes, it happens once per installed cell, and it is why repacking
// the node array does not disturb brick residency at all -- the two are independent after it.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "world/svo/brick_pool.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/cell_grid.hpp"
#include "world/svo/tree_layout.hpp"

namespace world::svo {

/// A half-open run of equal-sized elements, in whatever unit the accessor names.
struct DirtyRun {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

class ResidentGrid {
public:
    ResidentGrid() = default;
    /// `shape` fixes the cell layout; `brick_slots` fixes the pool and is never exceeded or grown.
    ResidentGrid(CellGrid shape, std::size_t brick_slots);

    [[nodiscard]] const CellGrid& shape() const noexcept { return shape_; }
    [[nodiscard]] const BrickPool& pool() const noexcept { return pool_; }
    [[nodiscard]] const std::vector<std::uint32_t>& brick_words() const noexcept { return bricks_; }
    [[nodiscard]] const std::vector<std::uint32_t>& node_words() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<FlatCell>& cells() const noexcept { return cells_; }
    [[nodiscard]] bool empty() const noexcept { return cells_.empty(); }
    [[nodiscard]] std::size_t resident_cells() const noexcept { return residentCells_; }

    /// Bricks the pool can hold, and the bytes it occupies -- both constant for this object's life.
    [[nodiscard]] std::size_t brick_capacity() const noexcept { return pool_.capacity(); }
    [[nodiscard]] std::uint64_t pool_bytes() const noexcept { return pool_.bytes(kBrickWords); }

    /// Install `tree` as cell `index`, replacing whatever was there.
    ///
    /// Returns false and installs NOTHING when the pool cannot fit the tree's bricks -- the caller
    /// must evict first. A partial install would leave a cell whose node payloads point at slots it
    /// does not own, which is the kind of corruption that shows up somewhere else entirely.
    bool install(std::size_t index, const BrickTree& tree);

    /// Drop cell `index`, returning its slots to the pool. Its node words go on the next repack.
    void evict(std::size_t index);

    /// Mark every brick of cell `index` used on `frame` (goal 261's usage stamps, CPU side).
    void touch(std::size_t index, std::uint32_t frame) noexcept;

    /// Repack the node array from the resident cells and update every cell's `node_base`. Cheap
    /// relative to the bricks, and it is what keeps node storage free of holes.
    void repack_nodes();

    /// Brick SLOTS whose contents changed since `clear_dirty`, coalesced into runs.
    [[nodiscard]] const std::vector<DirtyRun>& dirty_brick_slots() const noexcept { return dirtyBricks_; }
    /// Bytes those runs represent -- the number goal 257's Check asks for.
    [[nodiscard]] std::uint64_t dirty_brick_bytes() const noexcept;
    void clear_dirty() noexcept { dirtyBricks_.clear(); }

    /// A view onto a resident cell, for the CPU-side trace. Empty when the cell is not resident.
    [[nodiscard]] TreeView view_of(std::size_t index) const noexcept;

private:
    void free_cell_slots(std::size_t index) noexcept;
    void mark_dirty(std::uint32_t slot) noexcept;

    CellGrid shape_;
    BrickPool pool_;
    std::vector<std::uint32_t> bricks_; // the pool's storage: capacity * kBrickWords
    std::vector<std::uint32_t> nodes_;  // repacked from the per-cell buffers below
    std::vector<FlatCell> cells_;
    std::vector<std::vector<std::uint32_t>> cellNodes_; // per cell, brick indices already rewritten
    std::vector<std::vector<std::uint32_t>> cellSlots_; // per cell, the pool slots it owns
    std::vector<DirtyRun> dirtyBricks_;
    std::vector<std::uint32_t> dirtyScratch_;
    std::size_t residentCells_ = 0;
};

/// March a resident grid. Identical in meaning to the `FlatCellGrid` overload -- asserted in the
/// tests over real rays, because a divergence here would mean the CPU reference and the GPU are
/// tracing different worlds.
[[nodiscard]] Hit trace_ray_grid(const ResidentGrid& grid, const Ray& ray, const TraceParams& params = {},
                                 GridTraceStats* stats = nullptr) noexcept;

} // namespace world::svo
