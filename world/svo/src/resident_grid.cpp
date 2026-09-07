#include "world/svo/resident_grid.hpp"

#include <algorithm>
#include <cassert>

namespace world::svo {

ResidentGrid::ResidentGrid(CellGrid shape, std::size_t brick_slots)
    : shape_(std::move(shape)), pool_(brick_slots), bricks_(brick_slots * kBrickWords, 0u),
      cells_(shape_.cell_count()), cellNodes_(shape_.cell_count()), cellSlots_(shape_.cell_count()) {}

void ResidentGrid::free_cell_slots(std::size_t index) noexcept {
    for (const std::uint32_t slot : cellSlots_[index]) {
        pool_.release(slot);
    }
    cellSlots_[index].clear();
}

void ResidentGrid::mark_dirty(std::uint32_t slot) noexcept {
    // Coalesced on the way in: freshly allocated slots come off the free list in descending order,
    // so a cell's bricks are usually one contiguous run and this collapses to a single entry.
    if (!dirtyBricks_.empty()) {
        DirtyRun& last = dirtyBricks_.back();
        if (slot == last.first + last.count) {
            ++last.count;
            return;
        }
        if (slot + 1u == last.first) {
            last.first = slot;
            ++last.count;
            return;
        }
    }
    dirtyBricks_.push_back(DirtyRun{slot, 1u});
}

bool ResidentGrid::install(std::size_t index, const BrickTree& tree) {
    if (index >= cells_.size()) {
        return false;
    }
    if (tree.empty()) {
        evict(index);
        return true;
    }

    // The cell's OLD slots are freed first, so replacing a cell in place reuses its own storage and
    // a rebuild does not transiently need twice the pool.
    const bool wasResident = (cells_[index].flags & kFlatCellPresent) != 0u;
    free_cell_slots(index);

    const std::size_t needed = tree.brick_count();
    if (needed > pool_.free_slots()) {
        // NOTHING is installed. A partial install leaves node payloads pointing at slots the cell
        // does not own, and that corruption surfaces somewhere else entirely.
        if (wasResident) {
            cells_[index] = FlatCell{};
            cellNodes_[index].clear();
            --residentCells_;
        }
        return false;
    }

    std::vector<std::uint32_t>& slots = cellSlots_[index];
    slots.resize(needed);
    for (std::size_t i = 0; i < needed; ++i) {
        const std::uint32_t slot = pool_.allocate();
        assert(slot != kNoSlot); // guarded by the free_slots check above
        slots[i] = slot;
        const std::uint32_t* src = tree.brick_words(static_cast<std::uint32_t>(i));
        std::copy(src, src + kBrickWords, bricks_.begin() + static_cast<std::ptrdiff_t>(slot) * kBrickWords);
        mark_dirty(slot);
    }

    // THE ONE PIECE OF SURGERY: a brick leaf's payload holds a brick index in the CELL's numbering,
    // and the pool numbers them globally. Rewriting it here, once, is what makes brick residency
    // independent of where the node array happens to be repacked to.
    std::vector<std::uint32_t>& words = cellNodes_[index];
    words.assign(tree.nodes.begin(), tree.nodes.end());
    // STRUCTURALLY, from the root, following child pointers -- NOT linearly across the array.
    //
    // The first version walked the words in order, stepping by each header's own length. That
    // assumes the array is a gapless sequence of nodes in layout order, which nothing in
    // tree_layout.hpp promises, and it was wrong: a word that is not a header gets read as one, its
    // "kind" comes out as brick, and a payload that is not a brick index gets rewritten. The
    // symptom was the resident grid reporting a hit where the flat grid saw empty space -- a
    // corruption that surfaced nowhere near its cause, which is exactly why this walk follows the
    // pointers the traversal itself follows.
    std::vector<std::uint32_t> stack;
    stack.push_back(tree.root);
    while (!stack.empty()) {
        const std::uint32_t node = stack.back();
        stack.pop_back();
        const std::uint32_t header = words[node];
        const std::uint32_t kind = node_kind(header);
        if (kind == kNodeKindBrick) {
            const std::uint32_t local = words[node + kNodeBrickIndexSlot];
            words[node + kNodeBrickIndexSlot] = local < slots.size() ? slots[local] : 0u;
            continue;
        }
        if (kind == kNodeKindSolid) {
            continue;
        }
        const std::uint32_t mask = node_child_mask(header);
        for (int octant = 0; octant < 8; ++octant) {
            if ((mask & (1u << octant)) != 0u) {
                stack.push_back(words[node + node_child_slot(header, octant)]);
            }
        }
    }

    FlatCell& record = cells_[index];
    record.node_base = 0;  // set by repack_nodes
    record.brick_base = 0; // unused now: payloads carry absolute pool slots
    record.root = tree.root;
    record.flags = kFlatCellPresent;
    if (!wasResident) {
        ++residentCells_;
    }
    return true;
}

void ResidentGrid::evict(std::size_t index) {
    if (index >= cells_.size() || (cells_[index].flags & kFlatCellPresent) == 0u) {
        return;
    }
    free_cell_slots(index);
    cellNodes_[index].clear();
    cells_[index] = FlatCell{};
    --residentCells_;
}

void ResidentGrid::touch(std::size_t index, std::uint32_t frame) noexcept {
    if (index >= cellSlots_.size()) {
        return;
    }
    for (const std::uint32_t slot : cellSlots_[index]) {
        pool_.touch(slot, frame);
    }
}

void ResidentGrid::repack_nodes() {
    std::size_t total = 0;
    for (const std::vector<std::uint32_t>& words : cellNodes_) {
        total += words.size();
    }
    nodes_.clear();
    nodes_.reserve(total);
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        if ((cells_[i].flags & kFlatCellPresent) == 0u) {
            continue;
        }
        cells_[i].node_base = static_cast<std::uint32_t>(nodes_.size());
        nodes_.insert(nodes_.end(), cellNodes_[i].begin(), cellNodes_[i].end());
    }
}

std::uint64_t ResidentGrid::dirty_brick_bytes() const noexcept {
    std::uint64_t runs = 0;
    for (const DirtyRun& run : dirtyBricks_) {
        runs += run.count;
    }
    return runs * kBrickWords * sizeof(std::uint32_t);
}

TreeView ResidentGrid::view_of(std::size_t index) const noexcept {
    if (index >= cells_.size() || (cells_[index].flags & kFlatCellPresent) == 0u) {
        return TreeView{};
    }
    TreeView view;
    view.geometry = shape_.geometry_for(shape_.coord_of(index));
    view.nodes = nodes_.data() + cells_[index].node_base;
    // Payloads hold absolute pool slots, so the brick base is the pool's own start.
    view.bricks = bricks_.data();
    view.root = cells_[index].root;
    return view;
}

Hit trace_ray_grid_marking(const ResidentGrid& grid, const Ray& ray, const TraceParams& params,
                           std::uint32_t frame, CellMarks& marks, GridTraceStats* stats) noexcept {
    return detail::trace_grid_with(grid.shape(), ray, params, stats, [&](glm::ivec3 coord) {
        if (!grid.shape().contains(coord)) {
            return TreeView{};
        }
        const std::size_t index = grid.shape().index_of(coord);
        const TreeView view = grid.view_of(index);
        // A plain store of a value every ray in this frame agrees on. No atomic, no accumulation --
        // adding either would put the read-modify-write back and is why there is no ray counter.
        marks.mark(index, frame, view.empty());
        return view;
    });
}

Hit trace_ray_grid(const ResidentGrid& grid, const Ray& ray, const TraceParams& params,
                   GridTraceStats* stats) noexcept {
    // Shares the owning/flat walk by construction: this is the same lambda shape those two use.
    return detail::trace_grid_with(grid.shape(), ray, params, stats, [&](glm::ivec3 coord) {
        return grid.shape().contains(coord) ? grid.view_of(grid.shape().index_of(coord)) : TreeView{};
    });
}

} // namespace world::svo
