// Prompt 004 goal 260 (and the upload half of 257): the resident grid's two contracts.
//
//   1. IT TRACES WHAT THE OTHER FORMS TRACE. The brick indices inside every node payload are
//      rewritten from the cell's numbering to the pool's when a cell is installed. That is surgery
//      on built bytes, and if it is wrong the world renders as garbage somewhere far from the cause.
//      So it is checked ray by ray against the flat form, which is checked against the owning form,
//      which is checked against the 7,000-ray oracle.
//   2. AN UNCHANGED CELL COSTS ZERO UPLOAD BYTES. That is the entire point -- goal 257 made the
//      BUILD incremental and the upload immediately undid it. The dirty-byte accounting is what
//      goal 257's Check asks for, so it is asserted rather than reported.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>

#include "world/svo/cell_grid.hpp"
#include "world/svo/resident_grid.hpp"
#include "world/svo/tree_builder.hpp"

#include "detail/tree_builder_impl.hpp"

#include "test_samplers.hpp"

using namespace world::svo;
using svo_tests::SphereSampler;
using world::chunk::MaterialID;

namespace {

struct Built {
    CellGrid grid;
    std::vector<std::shared_ptr<const BrickTree>> cells;
};

Built build_cells(int region_log2, int cell_log2, int voxel_log2) {
    const SphereSampler sampler{glm::vec3{32.0f}, 20.0f, MaterialID::Stone};
    BuildParams params;
    params.uniform_lod = true;

    const int perAxis = 1 << (region_log2 - cell_log2);
    Built out;
    out.grid = CellGrid{glm::ivec3{0}, glm::ivec3{perAxis}, cell_log2, voxel_log2};
    out.cells.resize(out.grid.cell_count());
    for (std::size_t i = 0; i < out.grid.cell_count(); ++i) {
        BrickTree cell =
            build_tree(sampler, out.grid.geometry_for(out.grid.coord_of(i)), params, nullptr, nullptr);
        if (!cell.empty()) {
            out.cells[i] = std::make_shared<const BrickTree>(std::move(cell));
        }
    }
    return out;
}

std::size_t total_bricks(const Built& b) {
    std::size_t n = 0;
    for (const auto& c : b.cells) {
        if (c) {
            n += c->brick_count();
        }
    }
    return n;
}

ResidentGrid install_all(const Built& b, std::size_t slots) {
    ResidentGrid resident{b.grid, slots};
    const BrickTree nothing;
    for (std::size_t i = 0; i < b.cells.size(); ++i) {
        // Empty cells are installed TOO, as resident-and-empty. A producer that only installs the
        // cells with geometry leaves the empty ones looking un-loaded, and goal 263's fallback then
        // fills correct empty space with the proxy's coarser guess.
        REQUIRE(resident.install(i, b.cells[i] ? *b.cells[i] : nothing));
    }
    resident.repack_nodes();
    return resident;
}

struct RayGen {
    std::mt19937 rng;
    float extent = 64.0f;
    Ray next() {
        std::uniform_real_distribution<float> inside(0.15f * extent, 0.85f * extent);
        std::uniform_real_distribution<float> around(-0.8f * extent, 1.8f * extent);
        const glm::vec3 target{inside(rng), inside(rng), inside(rng)};
        glm::vec3 from{around(rng), around(rng), around(rng)};
        const int axis = static_cast<int>(rng() % 3u);
        from[axis] = (rng() % 2u) != 0u ? -0.5f * extent : 1.5f * extent;
        Ray ray;
        ray.origin = from;
        ray.dir = glm::normalize(target - from);
        return ray;
    }
};

} // namespace

TEST_CASE("a resident grid traces identically to the flat one", "[svo][resident]") {
    const Built b = build_cells(6, 4, -2);
    const std::size_t bricks = total_bricks(b);
    REQUIRE(bricks > 100);

    CellGrid owning = b.grid;
    for (std::size_t i = 0; i < b.cells.size(); ++i) {
        if (b.cells[i]) {
            owning.set(owning.coord_of(i), b.cells[i]);
        }
    }
    const FlatCellGrid flat{owning};
    const ResidentGrid resident = install_all(b, bricks + 64);

    CHECK(resident.resident_cells() == b.grid.cell_count()); // every cell, empty ones included
    CHECK(resident.pool().used() == bricks);

    RayGen gen{std::mt19937{5}};
    std::size_t hits = 0;
    for (int i = 0; i < 20000; ++i) {
        const Ray ray = gen.next();
        const Hit a = trace_ray_grid(flat, ray, TraceParams{});
        const Hit c = trace_ray_grid(resident, ray, TraceParams{});
        REQUIRE(c.hit == a.hit);
        if (a.hit) {
            ++hits;
            REQUIRE(c.t == Catch::Approx(a.t));
            REQUIRE(c.material == a.material);
            REQUIRE(c.normal == a.normal);
        }
    }
    CHECK(hits > 5000);
}

TEST_CASE("reinstalling only some cells dirties only their bricks", "[svo][resident]") {
    // Goal 257's Check, as an assertion. The whole grid installs, we clear the dirty log, and then
    // reinstall a handful of cells -- the dirty bytes must be those cells' bricks and nothing else.
    const Built b = build_cells(6, 4, -2);
    const std::size_t bricks = total_bricks(b);
    ResidentGrid resident = install_all(b, bricks + 64);

    const std::uint64_t allBytes = resident.dirty_brick_bytes();
    CHECK(allBytes == static_cast<std::uint64_t>(bricks) * kBrickWords * sizeof(std::uint32_t));
    resident.clear_dirty();
    CHECK(resident.dirty_brick_bytes() == 0u);

    // Reinstall three present cells.
    std::size_t touched = 0;
    std::size_t expectedBricks = 0;
    for (std::size_t i = 0; i < b.cells.size() && touched < 3; ++i) {
        if (!b.cells[i]) {
            continue;
        }
        REQUIRE(resident.install(i, *b.cells[i]));
        expectedBricks += b.cells[i]->brick_count();
        ++touched;
    }
    REQUIRE(touched == 3);
    resident.repack_nodes();

    CHECK(resident.dirty_brick_bytes() ==
          static_cast<std::uint64_t>(expectedBricks) * kBrickWords * sizeof(std::uint32_t));
    // And it is a small fraction of the whole -- the number that says the upload got cheaper.
    CHECK(resident.dirty_brick_bytes() < allBytes / 4);
    // Reinstalling in place must not leak slots: the pool holds exactly what it did.
    CHECK(resident.pool().used() == bricks);
}

TEST_CASE("eviction returns slots and the grid still traces", "[svo][resident]") {
    const Built b = build_cells(6, 4, -2);
    const std::size_t bricks = total_bricks(b);
    ResidentGrid resident = install_all(b, bricks + 64);

    std::size_t victim = 0;
    while (victim < b.cells.size() && !b.cells[victim]) {
        ++victim;
    }
    REQUIRE(victim < b.cells.size());
    const std::size_t freed = b.cells[victim]->brick_count();
    const std::size_t before = resident.pool().used();

    resident.evict(victim);
    resident.repack_nodes();
    CHECK(resident.pool().used() == before - freed);
    CHECK(resident.view_of(victim).empty());

    // The rest still traces -- an eviction must not corrupt its neighbours' node bases.
    RayGen gen{std::mt19937{21}};
    std::size_t hits = 0;
    for (int i = 0; i < 5000; ++i) {
        hits += trace_ray_grid(resident, gen.next(), TraceParams{}).hit ? 1u : 0u;
    }
    CHECK(hits > 1000);
}

TEST_CASE("a pool too small refuses the install rather than corrupting it", "[svo][resident]") {
    // The failure that matters: a PARTIAL install leaves node payloads pointing at slots the cell
    // does not own, and that corruption surfaces somewhere else entirely. install() must be
    // all-or-nothing.
    const Built b = build_cells(6, 4, -2);
    ResidentGrid resident{b.grid, 8}; // far too small for any real cell

    std::size_t victim = 0;
    while (victim < b.cells.size() && !b.cells[victim]) {
        ++victim;
    }
    REQUIRE(b.cells[victim]->brick_count() > 8);

    CHECK_FALSE(resident.install(victim, *b.cells[victim]));
    CHECK(resident.resident_cells() == 0);
    CHECK(resident.pool().used() == 0); // every slot it took was given back
    CHECK(resident.view_of(victim).empty());
}

TEST_CASE("the pool never grows, whatever is installed and evicted", "[svo][resident]") {
    // Goal 260's headline Check: resident capacity is fixed at construction and independent of what
    // the world does.
    const Built b = build_cells(6, 4, -2);
    const std::size_t slots = total_bricks(b) + 64;
    ResidentGrid resident{b.grid, slots};
    const std::uint64_t bytes = resident.pool_bytes();

    for (int round = 0; round < 4; ++round) {
        for (std::size_t i = 0; i < b.cells.size(); ++i) {
            if (b.cells[i]) {
                resident.install(i, *b.cells[i]);
            }
        }
        for (std::size_t i = 0; i < b.cells.size(); i += 2) {
            resident.evict(i);
        }
        resident.repack_nodes();
        CHECK(resident.brick_capacity() == slots);
        CHECK(resident.pool_bytes() == bytes);
        CHECK(resident.brick_words().size() == slots * kBrickWords);
        CHECK(resident.pool().used() + resident.pool().free_slots() == slots);
    }
}

TEST_CASE("usage stamps reach every brick of a cell", "[svo][resident]") {
    const Built b = build_cells(6, 4, -2);
    ResidentGrid resident = install_all(b, total_bricks(b) + 64);

    std::size_t cell = 0;
    while (cell < b.cells.size() && !b.cells[cell]) {
        ++cell;
    }
    resident.touch(cell, 42u);
    // Nothing else was touched, so everything else is evictable at frame 42 and this cell is not.
    const std::vector<std::uint32_t> old = resident.pool().evictable(42u, 1000000);
    CHECK(old.size() == resident.pool().used() - b.cells[cell]->brick_count());
}


TEST_CASE("never-stall fills an absent cell from the coarse proxy", "[svo][resident][proxy]") {
    // Goal 263's rule: "if LOD not available -> pick next higher available level". The property is
    // not that the answer is RIGHT to the voxel -- it cannot be, the cell is missing -- but that
    // there is an answer at all where the fine grid would have shown a hole.
    const Built b = build_cells(6, 4, -2);
    const std::size_t bricks = total_bricks(b);
    ResidentGrid resident = install_all(b, bricks + 64);

    // The coarse proxy: the same world at 4x the voxel size, one tree over the whole region.
    TreeGeometry coarseGeom;
    coarseGeom.origin = glm::vec3{0.0f};
    coarseGeom.root_size_log2 = 6;
    coarseGeom.voxel_size_log2 = 0; // 1 m against the grid's 0.25 m
    BuildParams cp;
    cp.uniform_lod = true;
    auto proxy = std::make_shared<const BrickTree>(
        build_tree(SphereSampler{glm::vec3{32.0f}, 20.0f, MaterialID::Stone}, coarseGeom, cp, nullptr,
                   nullptr));
    REQUIRE_FALSE(proxy->empty());
    resident.set_proxy(proxy);

    // Evict everything: the fine grid is now entirely absent.
    for (std::size_t i = 0; i < b.cells.size(); ++i) {
        resident.evict(i);
    }
    resident.repack_nodes();
    REQUIRE(resident.resident_cells() == 0);

    RayGen gen{std::mt19937{77}};
    std::size_t answered = 0;
    std::size_t fromProxy = 0;
    std::size_t total = 0;
    for (int i = 0; i < 4000; ++i) {
        const Ray ray = gen.next();
        // What the fine grid alone would say with everything evicted: nothing, ever.
        CHECK_FALSE(trace_ray_grid(resident, ray, TraceParams{}).hit);

        HitSource source = HitSource::None;
        const Hit hit = trace_ray_grid_never_stall(resident, ray, TraceParams{}, source);
        ++total;
        if (hit.hit) {
            ++answered;
            CHECK(source == HitSource::Proxy);
            // And it is a real surface, not an arbitrary answer.
            const float r = glm::length(hit.position - glm::vec3{32.0f});
            CHECK(r > 12.0f);
            CHECK(r < 40.0f);
            ++fromProxy;
        }
    }
    // The coarse level answers where the fine grid showed a hole. Not every ray hits -- many miss
    // the sphere entirely -- but a large fraction must, or the fallback is not working.
    CHECK(answered > total / 5);
    CHECK(fromProxy == answered);
}

TEST_CASE("never-stall prefers the FINE answer when the cell is resident", "[svo][resident][proxy]") {
    // The other half, and the one a careless implementation gets wrong: a proxy that answers for
    // cells that ARE resident would quietly coarsen the whole image. With everything installed, no
    // hit may come from the proxy.
    const Built b = build_cells(6, 4, -2);
    ResidentGrid resident = install_all(b, total_bricks(b) + 64);

    TreeGeometry coarseGeom;
    coarseGeom.origin = glm::vec3{0.0f};
    coarseGeom.root_size_log2 = 6;
    coarseGeom.voxel_size_log2 = 0;
    BuildParams cp;
    cp.uniform_lod = true;
    resident.set_proxy(std::make_shared<const BrickTree>(build_tree(
        SphereSampler{glm::vec3{32.0f}, 20.0f, MaterialID::Stone}, coarseGeom, cp, nullptr, nullptr)));

    RayGen gen{std::mt19937{78}};
    std::size_t fine = 0;
    for (int i = 0; i < 4000; ++i) {
        const Ray ray = gen.next();
        HitSource source = HitSource::None;
        const Hit withProxy = trace_ray_grid_never_stall(resident, ray, TraceParams{}, source);
        const Hit without = trace_ray_grid(resident, ray, TraceParams{});
        // Identical to the plain march: an installed grid must not be affected by the proxy at all.
        REQUIRE(withProxy.hit == without.hit);
        if (without.hit) {
            REQUIRE(withProxy.t == Catch::Approx(without.t));
            REQUIRE(source == HitSource::Fine);
            ++fine;
        }
    }
    CHECK(fine > 1000);
}

TEST_CASE("never-stall without a proxy is the old behaviour exactly", "[svo][resident][proxy]") {
    // Kept so the two can be compared rather than assumed, which is why set_proxy is optional.
    const Built b = build_cells(6, 4, -2);
    ResidentGrid resident = install_all(b, total_bricks(b) + 64);
    RayGen gen{std::mt19937{79}};
    for (int i = 0; i < 3000; ++i) {
        const Ray ray = gen.next();
        HitSource source = HitSource::None;
        const Hit a = trace_ray_grid_never_stall(resident, ray, TraceParams{}, source);
        const Hit b2 = trace_ray_grid(resident, ray, TraceParams{});
        REQUIRE(a.hit == b2.hit);
        if (a.hit) {
            REQUIRE(a.t == Catch::Approx(b2.t));
        }
    }
}
