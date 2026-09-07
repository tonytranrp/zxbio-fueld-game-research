// Prompt 004 goal 261: the marking's two contracts.
//
//   1. IT IS IDEMPOTENT. The GPU does this with a plain store and no atomic, and that is only sound
//      because every ray in a frame writes the SAME word for the same cell. The tests assert that
//      directly, because the failure mode of getting it wrong is a race that produces
//      plausible-looking numbers rather than a crash.
//   2. THE REQUEST SET IS STABLE. Goal 261's Check is that a deliberately under-resident grid
//      produces a request set whose size AND content match across two identical runs. A set that
//      wobbled would make the producer chase its own tail.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <random>

#include "world/svo/cell_marks.hpp"
#include "world/svo/resident_grid.hpp"
#include "world/svo/tree_builder.hpp"

#include "detail/tree_builder_impl.hpp"

#include "test_samplers.hpp"

using namespace world::svo;
using svo_tests::SphereSampler;
using world::chunk::MaterialID;

namespace {

struct Cells {
    CellGrid grid;
    std::vector<std::shared_ptr<const BrickTree>> trees;
};

Cells build_cells() {
    const SphereSampler sampler{glm::vec3{32.0f}, 20.0f, MaterialID::Stone};
    BuildParams params;
    params.uniform_lod = true;
    Cells out;
    out.grid = CellGrid{glm::ivec3{0}, glm::ivec3{4}, 4, -2}; // 64 m of 16 m cells at 0.25 m
    out.trees.resize(out.grid.cell_count());
    for (std::size_t i = 0; i < out.grid.cell_count(); ++i) {
        BrickTree cell =
            build_tree(sampler, out.grid.geometry_for(out.grid.coord_of(i)), params, nullptr, nullptr);
        if (!cell.empty()) {
            out.trees[i] = std::make_shared<const BrickTree>(std::move(cell));
        }
    }
    return out;
}

std::vector<Ray> fan(int count) {
    std::vector<Ray> rays;
    rays.reserve(static_cast<std::size_t>(count));
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> t(4.0f, 60.0f);
    for (int i = 0; i < count; ++i) {
        Ray ray;
        ray.origin = glm::vec3{-20.0f, t(rng), t(rng)};
        ray.dir = glm::normalize(glm::vec3{1.0f, 0.0f, 0.0f} +
                                 glm::vec3{0.0f, t(rng) * 0.004f - 0.12f, t(rng) * 0.004f - 0.12f});
        rays.push_back(ray);
    }
    return rays;
}

} // namespace

TEST_CASE("the mark word round-trips its frame and its request bit", "[svo][marks]") {
    CHECK(cell_mark_frame(make_cell_mark(0u, false)) == 0u);
    CHECK_FALSE(cell_mark_requested(make_cell_mark(0u, false)));
    CHECK(cell_mark_frame(make_cell_mark(123456u, true)) == 123456u);
    CHECK(cell_mark_requested(make_cell_mark(123456u, true)));
    // The frame occupies the low 31 bits, so the top one is never mistaken for a huge frame index.
    CHECK(cell_mark_frame(make_cell_mark(kCellFrameMask, true)) == kCellFrameMask);
    CHECK(cell_mark_requested(make_cell_mark(kCellFrameMask, true)));
}

TEST_CASE("marking the same cell many times writes the same word every time", "[svo][marks]") {
    // THE PROPERTY THAT REMOVES THE ATOMIC. If this were not exact, the GPU's unsynchronised stores
    // would be a data race rather than a set of agreeing writes.
    CellMarks marks(16);
    for (int i = 0; i < 1000; ++i) {
        marks.mark(7, 99u, false);
    }
    CHECK(marks.at(7) == make_cell_mark(99u, false));
    for (int i = 0; i < 1000; ++i) {
        marks.mark(7, 99u, true);
    }
    CHECK(marks.at(7) == make_cell_mark(99u, true));

    // Out of range is a no-op rather than a write past the end.
    marks.mark(9999, 1u, true);
    CHECK(marks.size() == 16);
}

TEST_CASE("a fully resident grid requests nothing", "[svo][marks]") {
    const Cells c = build_cells();
    std::size_t bricks = 0;
    for (const auto& t : c.trees) {
        if (t) {
            bricks += t->brick_count();
        }
    }
    ResidentGrid grid{c.grid, bricks + 32};
    for (std::size_t i = 0; i < c.trees.size(); ++i) {
        if (c.trees[i]) {
            REQUIRE(grid.install(i, *c.trees[i]));
        }
    }
    grid.repack_nodes();

    CellMarks marks(c.grid.cell_count());
    std::size_t hits = 0;
    for (const Ray& ray : fan(3000)) {
        hits += trace_ray_grid_marking(grid, ray, TraceParams{}, 5u, marks).hit ? 1u : 0u;
    }
    CHECK(hits > 500);
    // Cells were used...
    CHECK_FALSE(marks.used_on(5u).empty());
    // ...and nothing was missing, EXCEPT the cells that hold no geometry at all -- those are
    // genuinely absent and a ray stepping through one does request it. That is correct behaviour
    // and worth stating: "not resident" and "empty" are the same thing to a marcher, and it is the
    // producer's job to know that building an empty cell produces nothing.
    for (const std::uint32_t index : marks.requests_on(5u)) {
        CHECK(c.trees[index] == nullptr);
    }
}

TEST_CASE("an under-resident grid produces a stable request set across identical runs", "[svo][marks]") {
    // Goal 261's Check, verbatim: a deliberately under-resident state, and the request set's SIZE
    // and CONTENT must match between two identical runs.
    const Cells c = build_cells();
    const auto run = [&](std::uint32_t frame) {
        // A pool far too small for the world: most cells fail to install and stay absent.
        ResidentGrid grid{c.grid, 400};
        for (std::size_t i = 0; i < c.trees.size(); ++i) {
            if (c.trees[i]) {
                grid.install(i, *c.trees[i]); // may fail; that is the point
            }
        }
        grid.repack_nodes();
        // Against the trees that EXIST, not `c.grid.present_count()` -- the CellGrid in `Cells` is
        // only a shape here and never had cells set on it, so that count is 0 and the comparison
        // would pass vacuously in the wrong direction.
        const std::size_t buildable = static_cast<std::size_t>(
            std::count_if(c.trees.begin(), c.trees.end(), [](const auto& p) { return p != nullptr; }));
        REQUIRE(grid.resident_cells() > 0);
        REQUIRE(grid.resident_cells() < buildable);

        CellMarks marks(c.grid.cell_count());
        for (const Ray& ray : fan(3000)) {
            (void)trace_ray_grid_marking(grid, ray, TraceParams{}, frame, marks);
        }
        return marks.requests_on(frame);
    };

    const std::vector<std::uint32_t> a = run(11u);
    const std::vector<std::uint32_t> b = run(11u);
    REQUIRE_FALSE(a.empty()); // a run that requested nothing would prove nothing
    CHECK(a.size() == b.size());
    CHECK(a == b);

    // And the frame index is carried, not just the bit -- a request from an old frame must not be
    // mistaken for a fresh one.
    const std::vector<std::uint32_t> other = run(12u);
    CHECK(other.size() == a.size());
}

TEST_CASE("used_on and requests_on ignore other frames", "[svo][marks]") {
    CellMarks marks(8);
    marks.mark(0, 10u, false);
    marks.mark(1, 10u, true);
    marks.mark(2, 11u, false);
    marks.mark(3, 11u, true);

    CHECK(marks.used_on(10u) == std::vector<std::uint32_t>{0u, 1u});
    CHECK(marks.requests_on(10u) == std::vector<std::uint32_t>{1u});
    CHECK(marks.used_on(11u) == std::vector<std::uint32_t>{2u, 3u});
    CHECK(marks.requests_on(11u) == std::vector<std::uint32_t>{3u});
    CHECK(marks.used_on(12u).empty());

    marks.clear();
    CHECK(marks.used_on(10u).empty());
    CHECK(marks.requests_on(10u).empty());
}


TEST_CASE("the compaction separates used from unused, oldest first", "[svo][marks][compact]") {
    // Goal 262's structure, on a hand-built case where the right answer can be read off.
    CellMarks marks(6);
    marks.mark(0, 3u, false);  // old
    marks.mark(1, 9u, false);  // current
    marks.mark(2, 1u, false);  // oldest
    marks.mark(3, 9u, true);   // current, and requested
    marks.mark(4, 7u, false);  // old
    // cell 5 never marked at all: stamp 0, the oldest possible

    const CellMarks::Compaction c = marks.compact(9u);
    REQUIRE(c.order.size() == 6);
    // Evictable first, oldest stamp first: 5 (0), 2 (1), 0 (3), 4 (7).
    CHECK(c.first_used_this_frame == 4);
    CHECK(c.order[0] == 5u);
    CHECK(c.order[1] == 2u);
    CHECK(c.order[2] == 0u);
    CHECK(c.order[3] == 4u);
    // Then the ones used this frame, in index order.
    CHECK(c.order[4] == 1u);
    CHECK(c.order[5] == 3u);
    CHECK(c.requested == 1);
}

TEST_CASE("the compaction is correct over 10,000 random usage patterns", "[svo][marks][compact]") {
    // Goal 262's Check verbatim: a unit test against a CPU reference over 10,000 random patterns.
    // The reference here is the DEFINITION rather than a second implementation -- for each pattern
    // the invariants are re-derived from the marks directly, which is what makes this a check on
    // the compaction and not two copies of the same mistake.
    std::mt19937 rng(20260907u);
    for (int trial = 0; trial < 10000; ++trial) {
        const std::size_t n = 1 + rng() % 64u;
        const std::uint32_t frame = 1u + rng() % 20u;
        CellMarks marks(n);
        std::vector<std::uint32_t> stamp(n, 0u);
        std::vector<bool> requested(n, false);
        for (std::size_t i = 0; i < n; ++i) {
            if ((rng() % 4u) != 0u) { // some cells are never marked at all
                stamp[i] = rng() % 22u;
                requested[i] = (rng() % 3u) == 0u;
                marks.mark(i, stamp[i], requested[i]);
            }
        }

        const CellMarks::Compaction c = marks.compact(frame);

        // 1. It is a PERMUTATION of every cell -- nothing lost, nothing duplicated.
        REQUIRE(c.order.size() == n);
        std::vector<std::uint32_t> sorted = c.order;
        std::sort(sorted.begin(), sorted.end());
        for (std::size_t i = 0; i < n; ++i) {
            REQUIRE(sorted[i] == static_cast<std::uint32_t>(i));
        }

        // 2. The boundary is exact: everything before it was NOT used this frame, everything from
        //    it on WAS. This is the property eviction safety rests on.
        for (std::size_t i = 0; i < c.order.size(); ++i) {
            const bool usedNow = cell_mark_frame(marks.at(c.order[i])) == frame;
            REQUIRE(usedNow == (i >= c.first_used_this_frame));
        }

        // 3. The evictable half is ordered oldest first.
        for (std::size_t i = 1; i < c.first_used_this_frame; ++i) {
            REQUIRE(cell_mark_frame(marks.at(c.order[i - 1])) <= cell_mark_frame(marks.at(c.order[i])));
        }

        // 4. The request count matches the marks.
        std::size_t expected = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (cell_mark_frame(marks.at(i)) == frame && cell_mark_requested(marks.at(i))) {
                ++expected;
            }
        }
        REQUIRE(c.requested == expected);
    }
}

TEST_CASE("a frame nothing was used on leaves everything evictable", "[svo][marks][compact]") {
    CellMarks marks(5);
    marks.mark(0, 1u, false);
    marks.mark(1, 2u, false);
    const CellMarks::Compaction c = marks.compact(99u);
    CHECK(c.first_used_this_frame == 5);
    CHECK(c.requested == 0);
    // And an empty grid compacts to nothing rather than misbehaving.
    const CellMarks none(0);
    CHECK(none.compact(1u).order.empty());
    CHECK(none.compact(1u).first_used_this_frame == 0);
}
