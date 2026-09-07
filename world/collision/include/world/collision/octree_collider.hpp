#pragma once

// A SolidQuery over the sparse-brick octree (Prompt 003 goals 225-227; closes goal 173).
//
// WHY THIS AND NOT THE ANALYTIC COLLIDER. `TerrainCollider` answers from a 16 m cached height grid
// rebuilt on a background thread when the body leaves its inner half. Outside that cache it falls
// back to direct height queries, and the tree-trunk half of its answer only knows about trunks
// collected into the CURRENT cache. A fast camera outruns the cache, and then the world it collides
// against is not the world that is drawn. That is the "I clip through blocks" complaint, and no
// amount of widening the cache fixes it -- a cache has an edge by definition.
//
// This query has no edge. It answers from the SAME immutable BrickTree the renderer is marching,
// so "you cannot pass through anything the renderer draws" is true by construction rather than by
// a tolerance. And because it walks a genuinely 3D structure rather than a height field, a cave or
// an overhang (Prompt 006) cannot break it -- there is no "one surface per column" assumption in
// here to break.
//
// The tree is held by `shared_ptr<const BrickTree>`: immutable after construction, two owners
// (the renderer's staged upload and the simulation), no copy of 400 MB. See
// research/player-embodiment-log.md §1 for the ranking of the three options and why the other two
// lost.

#include <cstddef>
#include <memory>

#include "engine/core/math.hpp"
#include "world/collision/solid_query.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/cell_grid.hpp"

namespace world::collision {

class OctreeCollider {
public:
    OctreeCollider() = default;
    explicit OctreeCollider(std::shared_ptr<const world::svo::BrickTree> tree) noexcept
        : tree_(std::move(tree)) {}

    // Swapped on the main thread at a known point in the frame (after present, before the next
    // tick), so a plain member assignment is correct and sufficient -- the fixed step runs on the
    // same thread that adopts a finished build. No atomic, deliberately: an atomic here would
    // advertise a cross-thread contract that does not exist and would invite someone to rely on it.
    void set_tree(std::shared_ptr<const world::svo::BrickTree> tree) noexcept { tree_ = std::move(tree); }

    // Prompt 004 goal 256: the same query, over the cell grid.
    //
    // The renderer's structural change had to reach here, and the prompt says so explicitly -- this
    // collider's whole justification is that it answers from the SAME structure the renderer
    // marches, so leaving it on a single tree while the renderer moved to a grid would quietly
    // reintroduce the "the world you collide with is not the world you see" bug that Prompt 003
    // built it to remove.
    //
    // A grid takes precedence over a tree when both are set. Nothing else changes: the per-cell
    // descent is the SAME code, because a cell is a `TreeView` and the helpers already take one.
    void set_grid(std::shared_ptr<const world::svo::FlatCellGrid> grid) noexcept {
        grid_ = std::move(grid);
    }
    [[nodiscard]] const world::svo::FlatCellGrid* grid() const noexcept { return grid_.get(); }
    [[nodiscard]] const world::svo::BrickTree* tree() const noexcept { return tree_.get(); }
    [[nodiscard]] bool has_tree() const noexcept {
        return (grid_ != nullptr && !grid_->empty()) || (tree_ != nullptr && !tree_->empty());
    }
    // How many trees this collider has been handed. The harness asserts the simulation never runs
    // more than one generation behind the renderer.
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    void bump_generation() noexcept { ++generation_; }

    // The SolidQuery contract. A box overlaps solid if ANY voxel it touches holds a material whose
    // own definition says it is solid -- asked of world/materials, never by comparing IDs, so water
    // is swimmable and leaves are walk-through because their components say so.
    //
    // With no tree, NOTHING is solid. That is deliberate and it is the honest answer during the
    // first ~2 s of a run: the app has no world yet, so it cannot say anything is there. The caller
    // (the frame loop) does not step the body until a tree exists.
    [[nodiscard]] bool overlaps_solid(const Aabb& box) const noexcept;

    // The top of the highest solid voxel in this column at or below `yStart`. Genuinely 3D: it
    // descends the octree rather than assuming one surface per column, so a body standing on the
    // floor of a cave gets the cave floor and not the mountain above it. Returns
    // -infinity when the column has no solid voxel below yStart inside the tree.
    [[nodiscard]] float voxel_top(float x, float z, float yStart) const noexcept;

    // Diagnostics for the cost budget (goal 230): how many octree nodes the last overlaps_solid
    // visited. A box query that descends the whole depth on every call is the failure mode, and a
    // number is how you notice.
    [[nodiscard]] std::size_t last_nodes_visited() const noexcept { return lastNodesVisited_; }

    // Goal 230's attribution counters: how many times the sweep ASKED, and how deep each ask went.
    // Cost per tick is calls x depth, and knowing which of the two is large is the whole question --
    // the shore-lip measurement went from a guess to a number the moment these existed.
    [[nodiscard]] std::size_t query_count() const noexcept { return queryCount_; }
    [[nodiscard]] std::size_t node_visit_total() const noexcept { return nodeVisitTotal_; }
    void reset_query_counters() const noexcept {
        queryCount_ = 0;
        nodeVisitTotal_ = 0;
    }

private:
    std::shared_ptr<const world::svo::BrickTree> tree_;
    std::shared_ptr<const world::svo::FlatCellGrid> grid_;
    std::uint64_t generation_ = 0;
    mutable std::size_t lastNodesVisited_ = 0;
    mutable std::size_t queryCount_ = 0;
    mutable std::size_t nodeVisitTotal_ = 0;
};

static_assert(SolidQuery<OctreeCollider>);

} // namespace world::collision
