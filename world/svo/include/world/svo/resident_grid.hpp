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
#include "world/svo/cell_marks.hpp"
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
    /// After this the cell is NOT RESIDENT, which is what makes the never-stall fallback answer for
    /// it -- as distinct from a cell that is resident and empty.
    void evict(std::size_t index);

    /// Is this cell's content known? True for a cell holding geometry AND for one known to hold
    /// none. False only when it has never been installed, or was evicted.
    [[nodiscard]] bool resident(std::size_t index) const noexcept {
        return index < cells_.size() && (cells_[index].flags & kFlatCellPresent) != 0u;
    }

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

    /// Prompt 004 goal 263: the always-resident COARSE PROXY covering the whole grid.
    ///
    /// GigaVoxels §1.4's rule is "if LOD not available -> pick next higher available level in
    /// Mip-map": the renderer never waits and never shows a hole, it degrades and files a request.
    /// A grid cell that is absent has no coarser level OF ITS OWN to fall back to -- it has nothing
    /// at all -- so the fallback has to be a structure that is always there. This is it: one tree
    /// over the whole region at a coarse voxel size, small enough to be built once and kept
    /// resident for the life of the world.
    ///
    /// With no proxy set, an absent cell is skipped and the ray passes through -- the pre-263
    /// behaviour, kept so the two can be compared rather than assumed.
    void set_proxy(std::shared_ptr<const BrickTree> proxy) noexcept { proxy_ = std::move(proxy); }
    [[nodiscard]] const BrickTree* proxy() const noexcept { return proxy_.get(); }

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
    std::shared_ptr<const BrickTree> proxy_;
    std::vector<DirtyRun> dirtyBricks_;
    std::vector<std::uint32_t> dirtyScratch_;
    std::size_t residentCells_ = 0;
};

/// March a resident grid. Identical in meaning to the `FlatCellGrid` overload -- asserted in the
/// tests over real rays, because a divergence here would mean the CPU reference and the GPU are
/// tracing different worlds.
[[nodiscard]] Hit trace_ray_grid(const ResidentGrid& grid, const Ray& ray, const TraceParams& params = {},
                                 GridTraceStats* stats = nullptr) noexcept;

/// Goal 263: how a hit was obtained -- from the fine grid, or from the coarse proxy because the
/// cell the ray needed was not resident. The renderer shades both; the distinction exists so a
/// capture can be judged ("is this frame coarse-but-complete, or is it wrong?") and so the
/// convergence over the following frames is measurable rather than impressionistic.
enum class HitSource : std::uint8_t { Fine, Proxy, None };

/// Goal 261: the same march, marking every cell the ray steps into.
///
/// A cell the ray ENTERS is stamped with `frame`; a cell it steps into that is NOT resident is
/// stamped with `frame` and the request bit. Both are plain stores of a value identical for every
/// ray in the frame, which is the whole reason the GPU mirror needs no atomic -- see cell_marks.hpp
/// for the argument.
///
/// This is the CPU reference the shader's `g_CellUsage` writes are checked against.
[[nodiscard]] Hit trace_ray_grid_marking(const ResidentGrid& grid, const Ray& ray,
                                         const TraceParams& params, std::uint32_t frame,
                                         CellMarks& marks, GridTraceStats* stats = nullptr) noexcept;

/// Goal 263: the same march, but a ray that steps into an ABSENT cell consults the coarse proxy
/// over that cell's own span instead of passing through it.
///
/// Restricting the proxy to the absent cell's span is what makes this correct rather than merely
/// plausible: the proxy covers the whole region, so consulting it unrestricted would let it answer
/// for cells that ARE resident and are about to give a better answer a few steps later.
[[nodiscard]] Hit trace_ray_grid_never_stall(const ResidentGrid& grid, const Ray& ray,
                                             const TraceParams& params, HitSource& source,
                                             GridTraceStats* stats = nullptr) noexcept;

} // namespace world::svo
