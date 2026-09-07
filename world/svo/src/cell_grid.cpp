#include "world/svo/cell_grid.hpp"

#include <algorithm>
#include <cmath>

namespace world::svo {

std::size_t CellGrid::present_count() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(cells_.begin(), cells_.end(), [](const GridCell& c) { return c.present(); }));
}

std::vector<glm::ivec3> CellGrid::coords() const {
    std::vector<glm::ivec3> out;
    out.reserve(cells_.size());
    for (int z = 0; z < dims_.z; ++z) {
        for (int y = 0; y < dims_.y; ++y) {
            for (int x = 0; x < dims_.x; ++x) {
                out.push_back(originCell_ + glm::ivec3{x, y, z});
            }
        }
    }
    return out;
}

const GridCell* CellGrid::at(glm::ivec3 coord) const noexcept {
    if (!contains(coord)) {
        return nullptr;
    }
    const glm::ivec3 l = coord - originCell_;
    return &cells_[static_cast<std::size_t>(l.x) +
                   static_cast<std::size_t>(dims_.x) *
                       (static_cast<std::size_t>(l.y) +
                        static_cast<std::size_t>(dims_.y) * static_cast<std::size_t>(l.z))];
}

void CellGrid::set(glm::ivec3 coord, std::shared_ptr<const BrickTree> tree) noexcept {
    if (!contains(coord)) {
        return;
    }
    const glm::ivec3 l = coord - originCell_;
    cells_[static_cast<std::size_t>(l.x) +
           static_cast<std::size_t>(dims_.x) *
               (static_cast<std::size_t>(l.y) +
                static_cast<std::size_t>(dims_.y) * static_cast<std::size_t>(l.z))]
        .tree = std::move(tree);
}

CellGrid::Totals CellGrid::totals() const noexcept {
    Totals t;
    for (const GridCell& c : cells_) {
        if (!c.present()) {
            continue;
        }
        ++t.present_cells;
        t.bricks += c.tree->brick_count();
        t.node_words += c.tree->node_words();
        t.bytes += static_cast<std::uint64_t>(c.tree->memory_bytes());
    }
    return t;
}

Hit trace_ray_grid(const CellGrid& grid, const Ray& ray, const TraceParams& params,
                   GridTraceStats* stats) noexcept {
    return detail::trace_grid_with(grid, ray, params, stats, [&](glm::ivec3 coord) {
        const GridCell* c = grid.at(coord);
        return c != nullptr && c->present() ? c->tree->view() : TreeView{};
    });
}

FlatCellGrid::FlatCellGrid(const CellGrid& grid) : grid_(grid) {
    cells_.resize(grid.cell_count());
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        const GridCell* c = grid.at(grid.coord_of(i));
        if (c == nullptr || !c->present()) {
            continue; // flags stay 0: absent, and it owns no storage at all
        }
        const BrickTree& tree = *c->tree;
        FlatCell& record = cells_[i];
        record.node_base = static_cast<std::uint32_t>(nodes_.size());
        record.brick_base = static_cast<std::uint32_t>(bricks_.size() / kBrickWords);
        record.root = tree.root;
        record.flags = kFlatCellPresent;
        nodes_.insert(nodes_.end(), tree.nodes.begin(), tree.nodes.end());
        bricks_.insert(bricks_.end(), tree.bricks.begin(), tree.bricks.end());
    }
}

TreeView FlatCellGrid::view_of(std::size_t index) const noexcept {
    if (index >= cells_.size() || (cells_[index].flags & kFlatCellPresent) == 0u) {
        return TreeView{};
    }
    const FlatCell& record = cells_[index];
    TreeView view;
    view.geometry = grid_.geometry_for(grid_.coord_of(index));
    view.nodes = nodes_.data() + record.node_base;
    view.bricks = bricks_.data() + static_cast<std::size_t>(record.brick_base) * kBrickWords;
    view.root = record.root;
    return view;
}

Hit trace_ray_grid(const FlatCellGrid& flat, const Ray& ray, const TraceParams& params,
                   GridTraceStats* stats) noexcept {
    return detail::trace_grid_with(flat.grid(), ray, params, stats,
                      [&](glm::ivec3 coord) { return flat.view_of(flat.grid().index_of(coord)); });
}

} // namespace world::svo
