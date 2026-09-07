// Prompt 004 goal 256: the collider over a cell grid must answer exactly what it answers over the
// single tree covering the same world.
//
// WHY THIS FILE IS THE ONE THAT MATTERS. `OctreeCollider` exists (Prompt 003 goal 226) because it
// answers from the SAME structure the renderer marches -- that is its entire justification, and it
// is what makes "you cannot pass through anything the renderer draws" true by construction rather
// than by a tolerance. When the renderer's structure changed to a grid, the collider had to follow,
// and this file is the proof that following it changed no answer. A disagreement here is not a test
// failure; it is the clipping bug Prompt 003 was written to remove, reintroduced.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <memory>
#include <random>

#include "world/collision/octree_collider.hpp"
#include "world/svo/brick.hpp"
#include "world/svo/cell_grid.hpp"
#include "world/svo/sampler.hpp"
#include "world/svo/tree_builder.hpp"

#include "detail/tree_builder_impl.hpp"

using world::chunk::MaterialID;
using world::collision::Aabb;
using world::collision::OctreeCollider;
using namespace world::svo;

namespace {

// A solid ball plus a floor slab: the ball gives overhangs (the case a height field gets wrong) and
// the slab gives a surface every column can find.
struct BallAndFloor {
    glm::vec3 center{32.0f, 40.0f, 32.0f};
    float radius = 14.0f;
    float floorTop = 20.0f;

    [[nodiscard]] MaterialID material_at_center(const glm::vec3& c) const noexcept {
        if (c.y < floorTop) {
            return MaterialID::Stone;
        }
        return glm::length(c - center) < radius ? MaterialID::Stone : MaterialID::Air;
    }
    [[nodiscard]] BoxClassification classify(const Box& box) const noexcept {
        // Conservative: only claim uniformity when the whole box is clearly on one side of both
        // features. Anything else is Mixed, which makes the builder descend -- correct, just slower.
        BoxClassification out;
        const bool allBelowFloor = box.max.y <= floorTop;
        const bool allAboveFloor = box.min.y >= floorTop;
        if (allBelowFloor) {
            out.cls = BoxClass::Solid;
            out.material = MaterialID::Stone;
            return out;
        }
        if (allAboveFloor) {
            const glm::vec3 nearest = glm::clamp(center, box.min, box.max);
            float dMax = 0.0f;
            for (int corner = 0; corner < 8; ++corner) {
                const glm::vec3 p{(corner & 1) != 0 ? box.max.x : box.min.x,
                                  (corner & 2) != 0 ? box.max.y : box.min.y,
                                  (corner & 4) != 0 ? box.max.z : box.min.z};
                dMax = std::max(dMax, glm::length(p - center));
            }
            if (glm::length(nearest - center) >= radius) {
                out.cls = BoxClass::Air;
                return out;
            }
            if (dMax < radius) {
                out.cls = BoxClass::Solid;
                out.material = MaterialID::Stone;
                return out;
            }
        }
        out.cls = BoxClass::Mixed;
        return out;
    }
    void fill_brick(const glm::vec3& origin, float voxelEdge, Brick& brick) const noexcept {
        fill_brick_pointwise([&](const glm::vec3& c, float) { return material_at_center(c); }, origin,
                             voxelEdge, brick);
    }
};
static_assert(world::svo::VoxelSampler<BallAndFloor>);

struct Both {
    std::shared_ptr<const BrickTree> whole;
    std::shared_ptr<const FlatCellGrid> grid;
};

Both build_both(int region_log2, int cell_log2, int voxel_log2) {
    const BallAndFloor sampler;
    BuildParams params;
    params.uniform_lod = true; // identical geometry in both, so any disagreement is the QUERY

    TreeGeometry whole;
    whole.origin = glm::vec3{0.0f};
    whole.root_size_log2 = region_log2;
    whole.voxel_size_log2 = voxel_log2;

    const int perAxis = 1 << (region_log2 - cell_log2);
    CellGrid grid{glm::ivec3{0}, glm::ivec3{perAxis}, cell_log2, voxel_log2};
    for (const glm::ivec3 coord : grid.coords()) {
        BrickTree cell = build_tree(sampler, grid.geometry_for(coord), params, nullptr, nullptr);
        if (!cell.empty()) {
            grid.set(coord, std::make_shared<const BrickTree>(std::move(cell)));
        }
    }
    return Both{std::make_shared<const BrickTree>(build_tree(sampler, whole, params, nullptr, nullptr)),
                std::make_shared<const FlatCellGrid>(grid)};
}

} // namespace

TEST_CASE("the grid collider answers overlaps_solid exactly as the tree collider does", "[collision][grid]") {
    const Both both = build_both(6, 4, -2); // 64 m region, 16 m cells, 0.25 m voxels
    OctreeCollider tree{both.whole};
    OctreeCollider grid;
    grid.set_grid(both.grid);
    REQUIRE(tree.has_tree());
    REQUIRE(grid.has_tree());

    std::mt19937 rng(4242);
    std::uniform_real_distribution<float> pos(-4.0f, 68.0f);
    std::uniform_real_distribution<float> size(0.05f, 2.5f);

    std::size_t solid = 0;
    std::size_t compared = 0;
    for (int i = 0; i < 20000; ++i) {
        const glm::vec3 lo{pos(rng), pos(rng), pos(rng)};
        const glm::vec3 hi = lo + glm::vec3{size(rng), size(rng), size(rng)};
        const Aabb box{lo, hi};
        const bool a = tree.overlaps_solid(box);
        const bool b = grid.overlaps_solid(box);
        REQUIRE(a == b);
        solid += a ? 1u : 0u;
        ++compared;
    }
    // A run where nothing was solid would pass while proving nothing.
    CHECK(solid > 2000);
    CHECK(solid < compared - 2000);
}

TEST_CASE("the grid collider answers voxel_top exactly as the tree collider does", "[collision][grid]") {
    const Both both = build_both(6, 4, -2);
    OctreeCollider tree{both.whole};
    OctreeCollider grid;
    grid.set_grid(both.grid);

    std::mt19937 rng(99);
    std::uniform_real_distribution<float> xz(-4.0f, 68.0f);
    std::uniform_real_distribution<float> y(-4.0f, 70.0f);

    std::size_t found = 0;
    std::size_t infinite = 0;
    for (int i = 0; i < 20000; ++i) {
        const float px = xz(rng);
        const float pz = xz(rng);
        const float py = y(rng);
        const float a = tree.voxel_top(px, pz, py);
        const float b = grid.voxel_top(px, pz, py);
        if (std::isinf(a) || std::isinf(b)) {
            REQUIRE(std::isinf(a));
            REQUIRE(std::isinf(b));
            ++infinite;
            continue;
        }
        REQUIRE(b == Catch::Approx(a));
        ++found;
    }
    CHECK(found > 5000);
    CHECK(infinite > 100); // the outside-the-world case is exercised too
}

TEST_CASE("the grid collider finds a surface UNDER an overhang, like the tree one", "[collision][grid]") {
    // The case a height field gets wrong and the reason this collider walks a 3D structure. A
    // column through the ball has the ball's underside above the floor; standing under it, the
    // ground is the FLOOR, and the query must not report the ball.
    const Both both = build_both(6, 4, -2);
    OctreeCollider tree{both.whole};
    OctreeCollider grid;
    grid.set_grid(both.grid);

    // Directly under the ball's centre, starting below it.
    const float underBall = tree.voxel_top(32.0f, 32.0f, 22.0f);
    CHECK(grid.voxel_top(32.0f, 32.0f, 22.0f) == Catch::Approx(underBall));
    CHECK(underBall == Catch::Approx(20.0f).margin(0.5f)); // the floor, not the ball

    // And starting above it, the same column finds the ball's top.
    const float onBall = tree.voxel_top(32.0f, 32.0f, 60.0f);
    CHECK(grid.voxel_top(32.0f, 32.0f, 60.0f) == Catch::Approx(onBall));
    CHECK(onBall > 50.0f);
}

TEST_CASE("a collider with neither tree nor grid says nothing is solid", "[collision][grid]") {
    // The honest answer during the first seconds of a run, and the one the frame loop relies on.
    const OctreeCollider empty;
    CHECK_FALSE(empty.has_tree());
    CHECK_FALSE(empty.overlaps_solid(Aabb{glm::vec3{0.0f}, glm::vec3{1.0f}}));
    CHECK(std::isinf(empty.voxel_top(0.0f, 0.0f, 10.0f)));

    // An empty grid is the same as no grid.
    OctreeCollider emptyGrid;
    emptyGrid.set_grid(std::make_shared<const FlatCellGrid>());
    CHECK_FALSE(emptyGrid.has_tree());
    CHECK_FALSE(emptyGrid.overlaps_solid(Aabb{glm::vec3{0.0f}, glm::vec3{1.0f}}));
}

TEST_CASE("a grid takes precedence over a tree when both are set", "[collision][grid]") {
    // Documented behaviour, asserted: during a migration both can be present, and the grid is the
    // structure the renderer is marching, so the grid is the one that must answer.
    const Both both = build_both(6, 4, -2);
    OctreeCollider c{both.whole};
    CHECK(c.overlaps_solid(Aabb{glm::vec3{30.0f, 10.0f, 30.0f}, glm::vec3{31.0f, 11.0f, 31.0f}}));

    c.set_grid(both.grid);
    CHECK(c.grid() != nullptr);
    // Same answer, from the grid now.
    CHECK(c.overlaps_solid(Aabb{glm::vec3{30.0f, 10.0f, 30.0f}, glm::vec3{31.0f, 11.0f, 31.0f}}));

    // Clearing the grid falls back to the tree rather than to "nothing is solid".
    c.set_grid(nullptr);
    CHECK(c.overlaps_solid(Aabb{glm::vec3{30.0f, 10.0f, 30.0f}, glm::vec3{31.0f, 11.0f, 31.0f}}));
}
