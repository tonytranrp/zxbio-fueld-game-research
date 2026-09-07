// Prompt 004 goal 255/257: the cell grid, measured against the single tree it would replace.
//
// Goal 254's design rests on two numbers, and this measures both on the REAL terrain rather than on
// a test sphere:
//
//   1. REBUILD COST. Terrain is a surface, so a rebuild costs by area: one 32 m cell is 1/256 of a
//      512 m region's footprint. The design predicted ~7-14 ms per cell against the measured
//      1.89-3.60 s whole-region build. An earlier spot measurement said ~90 ms, because
//      `TerrainSampler::set_focus` samples ~1.3 M noise points REGARDLESS of the region being built
//      -- a cost that is 5-9% of a whole-region build and ~100% of a cell. Goal 251 built
//      `focus_keys`/`make_focus_tier`/`adopt_focus` so the tiers can be built once and shared by
//      every cell in a region; this probe is where that is proved or refuted.
//   2. TRAVERSAL COST. 16 levels become 12. Section 14 measured that a 29% cut in traversal steps
//      buys 19% of march time here, so a step reduction is worth about 0.65x of itself.
//
// Not shipped. A measurement tool, like tools/palette_probe.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <future>
#include <random>
#include <vector>

#include "engine/jobs/thread_pool.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/svo/cell_grid.hpp"
#include "world/svo/ray_trace.hpp"
#include "world/svo/terrain_sampler.hpp"
#include "world/svo/tree_builder.hpp"

using namespace world::svo;

namespace {

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

int arg_int(int argc, char** argv, const char* name, int fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) {
            return std::atoi(argv[i + 1]);
        }
    }
    return fallback;
}

struct Percentiles {
    double p50 = 0.0;
    double p95 = 0.0;
    double max = 0.0;
    double total = 0.0;
};

Percentiles summarize(std::vector<double> v) {
    Percentiles p;
    if (v.empty()) {
        return p;
    }
    for (const double x : v) {
        p.total += x;
    }
    std::sort(v.begin(), v.end());
    p.p50 = v[v.size() / 2];
    p.p95 = v[static_cast<std::size_t>(static_cast<double>(v.size()) * 0.95)];
    p.max = v.back();
    return p;
}

} // namespace

int main(int argc, char** argv) {
    const int regionLog2 = arg_int(argc, argv, "--region-log2", 9); // 512 m
    const int cellLog2 = arg_int(argc, argv, "--cell-log2", 5);     // 32 m
    const int voxelLog2 = arg_int(argc, argv, "--voxel-log2", -7);  // 7.8 mm
    const int seed = arg_int(argc, argv, "--seed", 1337);
    const auto lodRadius = static_cast<float>(arg_int(argc, argv, "--lod-radius", 4));
    const bool shareFocus = arg_int(argc, argv, "--share-focus", 1) != 0;

    // The eye height is DERIVED from the heightmap, not guessed. The first version of this probe
    // hard-coded y = 20 and every one of its 20,000 rays reported 1.0 steps and a hit -- because the
    // camera was inside the hill. That is the eighth instrument this pass has caught measuring
    // nothing, and it is why this line samples the terrain it is about to build.
    const world::generation::HeightmapGenerator probeHeight(static_cast<std::uint32_t>(seed));
    const glm::vec3 camera{48.0f, probeHeight.height_at(48.0f, 0.0f) + 1.7f, 0.0f};

    TreeGeometry whole;
    whole.root_size_log2 = regionLog2;
    whole.voxel_size_log2 = voxelLog2;
    const float half = whole.root_edge() * 0.5f;
    // SNAPPED TO THE CELL EDGE, not to 8 m as the app does. The two structures must cover the same
    // volume or the comparison is between two different worlds -- which is exactly what the first
    // run did: the region snapped to 8 m and the grid to 32 m, they were offset by 16 m, and the
    // grid "missed" two thirds of the hits. Ninth instrument.
    const auto snap = static_cast<float>(std::ldexp(1.0, cellLog2));
    whole.origin = glm::vec3{std::floor((camera.x - half) / snap) * snap,
                             std::floor((8.0f - half) / snap) * snap,
                             std::floor((camera.z - half) / snap) * snap};

    std::printf("region %g m, cell %g m, voxel %g mm, lod-radius %g, focus %s\n",
                static_cast<double>(whole.root_edge()), std::ldexp(1.0, cellLog2),
                std::ldexp(1.0, voxelLog2) * 1000.0, static_cast<double>(lodRadius),
                shareFocus ? "SHARED (goal 251)" : "per cell");
    std::printf("levels: whole %d, cell %d\n", whole.voxel_bits(), cellLog2 - voxelLog2);

    const world::generation::HeightmapGenerator heightmap(static_cast<std::uint32_t>(seed));
    engine::jobs::ThreadPool pool(std::max(1u, std::thread::hardware_concurrency()));

    TerrainSamplerParams sp;
    sp.seed = static_cast<std::uint32_t>(seed);

    BuildParams bp;
    bp.lod_center = camera;
    bp.lod_radius = lodRadius;
    // --uniform-lod separates the two questions this probe can otherwise confuse. With distance LOD
    // on, a 512 m region and a 32 m cell compute DIFFERENT levels for the same point (the level is
    // relative to the root edge, and the two roots are four levels apart), so the structures hold
    // genuinely different geometry and rays are entitled to disagree. With it off they must hold
    // the SAME geometry, and any disagreement is a traversal bug. Run both.
    bp.uniform_lod = arg_int(argc, argv, "--uniform-lod", 0) != 0;
    if (bp.uniform_lod) {
        std::printf("UNIFORM LOD: both structures hold identical geometry, so rays must agree 100%%\n");
    }

    // ---- the whole-region tree, as the baseline ------------------------------------------------
    BrickTree wholeTree;
    double wholeSampler = 0.0;
    double wholeFocus = 0.0;
    double wholeBuild = 0.0;
    {
        auto t0 = std::chrono::steady_clock::now();
        TerrainSampler sampler(heightmap, sp, Box{whole.origin, whole.max_corner()});
        wholeSampler = seconds_since(t0);
        t0 = std::chrono::steady_clock::now();
        sampler.set_focus(bp.lod_center, 4.0f * lodRadius);
        wholeFocus = seconds_since(t0);
        t0 = std::chrono::steady_clock::now();
        wholeTree = build_tree(sampler, whole, bp, &pool, nullptr);
        wholeBuild = seconds_since(t0);
    }
    std::printf("\nWHOLE REGION: %zu bricks, %.1f MB | sampler %.3f s + focus %.3f s + build %.3f s "
                "= %.3f s\n",
                wholeTree.brick_count(), static_cast<double>(wholeTree.memory_bytes()) / 1.0e6, wholeSampler,
                wholeFocus, wholeBuild, wholeSampler + wholeFocus + wholeBuild);

    // ---- the same world as a grid of cells ------------------------------------------------------
    const int perAxis = 1 << (regionLog2 - cellLog2);
    const auto cellEdge = static_cast<float>(std::ldexp(1.0, cellLog2));
    const glm::ivec3 originCell{static_cast<int>(std::floor(whole.origin.x / cellEdge)),
                                static_cast<int>(std::floor(whole.origin.y / cellEdge)),
                                static_cast<int>(std::floor(whole.origin.z / cellEdge))};
    CellGrid grid{originCell, glm::ivec3{perAxis}, cellLog2, voxelLog2};

    // Goal 251's shared tiers: `set_focus` samples ~1.3 M noise points and the cost does not shrink
    // with the region being built, so paying it per cell would dominate. Built ONCE here and adopted
    // by every cell -- which is the whole reason that primitive exists.
    TerrainSampler::FocusTiers sharedTiers;
    double focusSeconds = 0.0;
    if (shareFocus) {
        const auto t0 = std::chrono::steady_clock::now();
        TerrainSamplerParams probeParams = sp;
        for (const TerrainSampler::FocusKey& key :
             TerrainSampler::focus_keys(probeParams, bp.lod_center, 4.0f * lodRadius)) {
            sharedTiers.push_back(TerrainSampler::make_focus_tier(heightmap, key));
        }
        focusSeconds = seconds_since(t0);
    }

    // ACROSS CELLS, not inside one. The first version built cells one at a time, each handing the
    // pool to `build_tree` -- but a 32 m cell has almost nothing to split, so that measured a serial
    // grid against a parallel region and reported the grid 6.1x slower. The parallelism in a grid is
    // BETWEEN cells; measuring it any other way compares two different things.
    const std::vector<glm::ivec3> coords = grid.coords();
    std::vector<double> cellMs(coords.size(), 0.0);
    std::vector<std::shared_ptr<const BrickTree>> builtCells(coords.size());
    const auto gridStart = std::chrono::steady_clock::now();
    {
        std::vector<std::future<void>> jobs;
        jobs.reserve(coords.size());
        for (std::size_t i = 0; i < coords.size(); ++i) {
            jobs.push_back(pool.submit([&, i] {
                const TreeGeometry cg = grid.geometry_for(coords[i]);
                const auto t0 = std::chrono::steady_clock::now();
                TerrainSampler sampler(heightmap, sp, Box{cg.origin, cg.max_corner()});
                if (shareFocus) {
                    sampler.adopt_focus(sharedTiers);
                } else {
                    sampler.set_focus(bp.lod_center, 4.0f * lodRadius);
                }
                BrickTree cellTree = build_tree(sampler, cg, bp, nullptr, nullptr);
                cellMs[i] = seconds_since(t0) * 1000.0;
                if (!cellTree.empty()) {
                    builtCells[i] = std::make_shared<const BrickTree>(std::move(cellTree));
                }
            }));
        }
        for (std::future<void>& f : jobs) {
            f.get();
        }
    }
    const double gridSeconds = seconds_since(gridStart);
    std::size_t built = 0;
    {
        std::vector<double> present;
        for (std::size_t i = 0; i < coords.size(); ++i) {
            if (builtCells[i]) {
                grid.set(coords[i], builtCells[i]);
                present.push_back(cellMs[i]);
                ++built;
            }
        }
        cellMs.swap(present);
    }
    const Percentiles cell = summarize(cellMs);
    const CellGrid::Totals totals = grid.totals();

    std::printf("GRID: %zu of %zu cells present, %zu bricks, %.1f MB | shared focus %.3f s + build "
                "%.3f s = %.3f s\n",
                totals.present_cells, grid.cell_count(), totals.bricks,
                static_cast<double>(totals.bytes) / 1.0e6, focusSeconds, gridSeconds,
                focusSeconds + gridSeconds);
    std::printf("PER CELL (the number goal 254 predicted at 7-14 ms): p50 %.1f ms, p95 %.1f ms, max "
                "%.1f ms\n",
                cell.p50, cell.p95, cell.max);

    // ---- and what it costs to trace -------------------------------------------------------------
    std::mt19937 rng(9001);
    std::uniform_real_distribution<float> yaw(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> pitch(-0.35f, 0.15f);
    std::uint64_t wholeSteps = 0;
    std::uint64_t gridSteps = 0;
    std::uint64_t gridCells = 0;
    std::size_t hitsWhole = 0;
    std::size_t hitsGrid = 0;
    std::size_t agree = 0;
    constexpr int kRays = 20000;
    TraceParams tp;
    for (int i = 0; i < kRays; ++i) {
        const float a = yaw(rng);
        const float p = pitch(rng);
        Ray ray;
        ray.origin = camera;
        ray.dir =
            glm::normalize(glm::vec3{std::cos(a) * std::cos(p), std::sin(p), std::sin(a) * std::cos(p)});
        const Hit w = trace_ray(wholeTree, ray, tp);
        GridTraceStats gs;
        const Hit gh = trace_ray_grid(grid, ray, tp, &gs);
        wholeSteps += w.steps;
        gridSteps += gh.steps;
        gridCells += gs.cells_entered;
        hitsWhole += w.hit ? 1u : 0u;
        hitsGrid += gh.hit ? 1u : 0u;
        if (w.hit == gh.hit && (!w.hit || std::fabs(w.t - gh.t) < 0.05f)) {
            ++agree;
        }
    }
    // ---- and what ONE cell costs to rebuild, alone, with nothing competing for the pool --------
    // This is the number a per-cell rebuild (goal 257) actually pays: the p50 above is contaminated
    // by 64 cells contending for 16 threads, which is the right way to measure a WHOLE-grid build
    // and the wrong way to measure an incremental one.
    {
        const glm::ivec3 surfaceCoord{
            static_cast<int>(std::floor(camera.x / cellEdge)),
            static_cast<int>(std::floor((camera.y - 2.0f) / cellEdge)),
            static_cast<int>(std::floor(camera.z / cellEdge))};
        const TreeGeometry cg = grid.geometry_for(surfaceCoord);
        double best = 1.0e9;
        std::size_t bricks = 0;
        for (int trial = 0; trial < 3; ++trial) {
            const auto t0 = std::chrono::steady_clock::now();
            TerrainSampler sampler(heightmap, sp, Box{cg.origin, cg.max_corner()});
            sampler.adopt_focus(sharedTiers);
            const BrickTree one = build_tree(sampler, cg, bp, &pool, nullptr);
            best = std::min(best, seconds_since(t0) * 1000.0);
            bricks = one.brick_count();
        }
        std::printf("ONE CELL ALONE (goal 257's real cost): %.1f ms, %zu bricks, cell at (%d,%d,%d)\n",
                    best, bricks, surfaceCoord.x, surfaceCoord.y, surfaceCoord.z);

        // And the SAME measurement on a cell far from the LOD centre. If cost scaled with AREA as
        // goal 254 assumed, these two would be similar. If it scales with DETAIL, and LOD
        // concentrates detail at the camera, the far one is far cheaper -- and that difference is
        // what decides whether per-cell rebuild is viable.
        glm::ivec3 farCoord = surfaceCoord;
        farCoord.x += perAxis / 2 - 1;
        if (grid.contains(farCoord)) {
            const TreeGeometry fg = grid.geometry_for(farCoord);
            double farBest = 1.0e9;
            std::size_t farBricks = 0;
            for (int trial = 0; trial < 3; ++trial) {
                const auto t0 = std::chrono::steady_clock::now();
                TerrainSampler sampler(heightmap, sp, Box{fg.origin, fg.max_corner()});
                sampler.adopt_focus(sharedTiers);
                const BrickTree one = build_tree(sampler, fg, bp, &pool, nullptr);
                farBest = std::min(farBest, seconds_since(t0) * 1000.0);
                farBricks = one.brick_count();
            }
            std::printf("ONE FAR CELL ALONE: %.1f ms, %zu bricks, cell at (%d,%d,%d) -- %.1fx cheaper\n",
                        farBest, farBricks, farCoord.x, farCoord.y, farCoord.z,
                        farBest > 0.0 ? best / farBest : 0.0);
        }
    }

    std::printf("TRACE over %d rays: whole %.1f steps/ray (%zu hits) | grid %.1f steps/ray + %.1f "
                "cells (%zu hits) | agree %.1f%%\n",
                kRays, static_cast<double>(wholeSteps) / kRays, hitsWhole,
                static_cast<double>(gridSteps) / kRays, static_cast<double>(gridCells) / kRays, hitsGrid,
                100.0 * static_cast<double>(agree) / kRays);
    return 0;
}
