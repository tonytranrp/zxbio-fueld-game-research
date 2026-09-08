#include "svo_world.hpp"

#include "world/svo/lod_bands.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <utility>

#include "engine/core/log.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "world/svo/terrain_sampler.hpp"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#else
#define ZoneScopedN(name)
#endif

namespace app {

using engine::core::log;
using engine::core::LogLevel;

namespace {

// Goal 170's measured rule: a build on EVERY hardware thread starved the render thread -- 12 of
// the 13 frames over 20 ms in a 900-frame walk were `present` stalls of 20-30 ms with a build
// running and nothing else happening; on 12 of 16 threads the same walk had one slow frame (the
// tree swap). Three quarters of the machine builds, the rest keeps the frame moving.
std::size_t default_build_threads() noexcept {
    const unsigned int hw = std::jthread::hardware_concurrency();
    return hw == 0 ? 1 : std::max<std::size_t>(1, static_cast<std::size_t>(hw) * 3 / 4);
}

} // namespace

namespace {

// Goal 296: 8 km at 16 m cells. The size is not a preference -- the channel-head threshold
// A_c = 0.1-5 km^2 against a 512 m region means the PLAYABLE WORLD holds 2.6 channel-head areas at
// best, so the erosion has to run on something much wider and the region is a window into it. Full
// arithmetic in research/earth-terrain-pipeline-log.md section 1.
[[nodiscard]] world::generation::field::FieldGeometry macro_geometry() noexcept {
    constexpr float kCell = 16.0f;
    constexpr std::int32_t kCells = 500;
    constexpr float kHalf = 0.5f * kCell * static_cast<float>(kCells);
    return world::generation::field::FieldGeometry{
        .origin_x = -kHalf, .origin_z = -kHalf, .cell_size = kCell, .cells = kCells};
}

[[nodiscard]] std::shared_ptr<const world::generation::field::TerrainField>
bake_macro_field(const SvoWorldOptions& options) {
    if (!options.macro_field) {
        return nullptr;
    }
    auto field = std::make_shared<world::generation::field::TerrainField>(macro_geometry());
    const auto started = std::chrono::steady_clock::now();
    world::generation::field::run_pipeline(
        *field, world::generation::field::MacroParams{.seed = options.seed}, options.field_stages,
        [](std::string_view name, double seconds, void*) {
            // Goal 299 wants a per-stage breakdown rather than one number, so the log line names
            // the stage that cost it.
            log(LogLevel::Info, "terrain field stage \"{}\": {:.3f} s", name, seconds);
        },
        nullptr);
    // Goal 321: put the playable region on land. The 512 m region sits at world (0, 0) and the
    // continent mask is a 4 km feature, so without this the spawn point is wherever the seed put
    // it -- which on the shipped seed was open water to the horizon.
    if (!world::generation::field::recentre_on_land(*field, 320.0f)) {
        log(LogLevel::Warn, "terrain field: no land large enough for the playable region; "
                            "the world origin is wherever the seed put it");
    } else {
        log(LogLevel::Info, "terrain field: world origin moved to ({:.0f}, {:.0f}) in field space "
                            "so the playable region is on land",
            -field->geometry().origin_x, -field->geometry().origin_z);
    }

    const double total =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    log(LogLevel::Info, "terrain field: {} x {} cells at {:.0f} m ({:.2f} MB) baked in {:.3f} s",
        field->cells(), field->cells(), field->geometry().cell_size,
        static_cast<double>(field->bytes()) / 1.0e6, total);
    return field;
}

} // namespace

SvoWorld::SvoWorld(const SvoWorldOptions& options)
    : options_(options), heightmap_(options.seed, bake_macro_field(options)),
      pool_(options.worker_threads == 0 ? default_build_threads() : options.worker_threads) {}

SvoWorld::~SvoWorld() = default; // worker_ joins first (declared last), then pool_ drains

world::svo::TreeGeometry SvoWorld::geometry_for(glm::vec3 camera) const noexcept {
    world::svo::TreeGeometry g;
    g.root_size_log2 = options_.root_size_log2;
    g.voxel_size_log2 = options_.voxel_size_log2;
    const float half = g.root_edge() * 0.5f;
    g.origin = glm::vec3{std::floor((camera.x - half) / 8.0f) * 8.0f, 8.0f - half,
                         std::floor((camera.z - half) / 8.0f) * 8.0f};
    return g;
}

float SvoWorld::distance_from_build_center(glm::vec3 camera) const noexcept {
    return glm::length(camera - buildCenter_);
}

bool SvoWorld::should_rebuild(glm::vec3 camera, float speed, double now) const noexcept {
    if (building_.load()) {
        return false; // one build at a time, unchanged
    }
    // Goal 249's minimum interval, measured from the ADOPTION of the last tree rather than the
    // start of its build: the upload is 13-21 frames of UpdateBuffer traffic after the build ends,
    // and starting the next resample inside that window stacks CPU work on top of it.
    if (now - lastAdoptSeconds_ < static_cast<double>(options_.rebuild_min_interval_seconds)) {
        return false;
    }
    // Goal 250: not while moving fast. A build centred where the camera was is worthless by the
    // time it lands, and at 40 m/s a 2 s build is 80 m stale on arrival.
    if (speed > options_.rebuild_max_speed) {
        return false;
    }
    const float distance = distance_from_build_center(camera);
    // Goal 249's hysteresis. Having triggered once, wait until the camera settles near the new
    // centre before arming again -- otherwise a camera drifting along the trigger radius asks for
    // a rebuild on every frame it crosses back and forth.
    if (awaitingSettle_) {
        if (distance <= options_.rebuild_settle_metres) {
            awaitingSettle_ = false;
        }
        return false;
    }
    if (distance <= options_.rebuild_trigger_metres) {
        return false;
    }
    awaitingSettle_ = true;
    return true;
}

void SvoWorld::note_adopted(double now, glm::vec3 cameraAtAdopt) noexcept {
    lastAdoptSeconds_ = now;
    // Goal 250's check: how far the camera had travelled from the build centre by the time the tree
    // it asked for actually landed. This is the number that says whether deferring worked.
    lastAdoptCamera_ = cameraAtAdopt;
    lastAdoptLag_ = distance_from_build_center(cameraAtAdopt);
}

bool SvoWorld::request_build(glm::vec3 camera) {
    if (building_.load()) {
        return false;
    }
    if (worker_.joinable()) {
        worker_.join(); // the previous build has finished (building_ is false); reap its thread
    }
    building_.store(true);
    requested_ = true;
    buildCenter_ = camera;
    worker_ = std::jthread([this, camera] { build_job(camera); });
    return true;
}

void SvoWorld::build_job(glm::vec3 camera) {
    ZoneScopedN("svo build");
    try {
        const world::svo::TreeGeometry g = geometry_for(camera);
        world::svo::TerrainSamplerParams sp;
        sp.seed = options_.seed;
        sp.trees = options_.trees;
        const world::svo::Box region{g.origin, g.max_corner()};
        const auto samplerStart = std::chrono::steady_clock::now();
        world::svo::TerrainSampler sampler(heightmap_, sp, region);
        // Goal 251: the focus tiers are ~1.3 M noise samples and do not shrink with the region --
        // at a 32 m cell they ARE the build (254's measurement). The sharing primitive for that
        // lives on TerrainSampler now (`focus_keys` / `make_focus_tier` / `adopt_focus`), and the
        // cell grid will build the tiers ONCE per rebuild and hand them to every cell in it.
        //
        // A cross-BUILD cache was implemented here first and MEASURED AT A 0% HIT RATE: the tiers
        // are keyed on a rectangle snapped to 0.5 m, and consecutive builds are 8 m apart by
        // construction (the goal 249 trigger), so a moving camera never revisits one and a
        // stationary camera never rebuilds. It was removed rather than left in -- machinery that
        // provably cannot fire is worse than none, because it reads as coverage.
        sampler.set_focus(camera, 4.0f * options_.effective_lod_radius());
        const double samplerSeconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - samplerStart).count();

        world::svo::BuildParams bp;
        bp.lod_center = camera;
        bp.lod_radius = options_.effective_lod_radius();
        world::svo::BuildStats stats;

        // ---- the cell-grid path (goals 254-256) -------------------------------------------------
        if (options_.cell_size_log2 > 0) {
            build_grid_job(g, sp, bp, sampler, samplerSeconds);
            building_.store(false);
            return;
        }

        world::svo::BrickTree tree = world::svo::build_tree(sampler, g, bp, &pool_, &stats);

        LastBuild last;
        last.stats = stats;
        last.tree = tree.stats();
        last.bricks = tree.brick_count();
        last.memory_bytes = tree.memory_bytes();
        last.trees = sampler.trees().size();
        last.sampler_seconds = samplerSeconds;
        last.valid = true;

        const std::lock_guard guard(mutex_);
        finished_ = std::make_shared<const world::svo::BrickTree>(std::move(tree));
        lastBuild_ = last;
    } catch (const std::exception& e) {
        log(LogLevel::Error, "svo build failed: {}", e.what());
    }
    building_.store(false);
}

std::shared_ptr<const world::svo::FlatCellGrid> SvoWorld::take_finished_grid() {
    const std::lock_guard guard(mutex_);
    std::shared_ptr<const world::svo::FlatCellGrid> out;
    if (finishedGrid_) {
        out = finishedGrid_;
        finishedGrid_.reset();
    }
    return out;
}

// Goal 256: the same region as a grid of cells, built with the SAME sampler parameters so the
// world is identical -- only its container changes.
//
// Two things here are the whole reason this is not simply a loop:
//
//   1. THE FOCUS TIERS ARE BUILT ONCE AND SHARED. `set_focus` samples ~1.3 M noise points and the
//      cost does not shrink with the region, so paying it per cell would make every cell cost what
//      a region costs -- measured, and the reason goal 251 built `focus_keys`/`make_focus_tier`/
//      `adopt_focus` at all. The sampler above already built them for this camera; every cell
//      adopts those.
//   2. THE PARALLELISM IS BETWEEN CELLS, not inside one. A 32 m cell has almost nothing for
//      `build_tree`'s own subtree split to divide, so handing it the pool and building cells one at
//      a time measures a serial grid against a parallel region -- which reported the grid 6.1x
//      slower before the mistake was caught (research section 18).
void SvoWorld::build_grid_job(const world::svo::TreeGeometry& g,
                              const world::svo::TerrainSamplerParams& sp,
                              const world::svo::BuildParams& bp, const world::svo::TerrainSampler& seeded,
                              double samplerSeconds) {
    const int cellLog2 = options_.cell_size_log2;
    const auto cellEdge = static_cast<float>(std::ldexp(1.0, cellLog2));
    const int perAxis = 1 << (options_.root_size_log2 - cellLog2);
    const glm::ivec3 originCell{static_cast<int>(std::floor(g.origin.x / cellEdge)),
                                static_cast<int>(std::floor(g.origin.y / cellEdge)),
                                static_cast<int>(std::floor(g.origin.z / cellEdge))};
    world::svo::CellGrid grid{originCell, glm::ivec3{perAxis}, cellLog2, options_.voxel_size_log2};

    const world::svo::TerrainSampler::FocusTiers tiers = seeded.focus_tiers();

    const auto buildStart = std::chrono::steady_clock::now();
    const std::size_t count = grid.cell_count();
    const float finest = static_cast<float>(std::ldexp(1.0, options_.voxel_size_log2));

    // Goal 257: each cell's DETAIL BAND, quantised so it is a step function of camera distance.
    // This is what makes the rebuild incremental at all -- see world/svo/lod_bands.hpp for why the
    // continuous rule made "rebuild what changed" mean "rebuild everything".
    std::vector<int> bands(count);
    for (std::size_t i = 0; i < count; ++i) {
        bands[i] = world::svo::cell_band(bp.lod_center, grid.coord_of(i), cellEdge, options_.effective_lod_radius());
    }

    // A cell can be carried over when the previous grid had it AND its band is unchanged AND the
    // grid has not shifted underneath it. The origin check matters: cell coordinates are absolute,
    // so a grid that moved still names the same cubes -- but only for the coordinates both grids
    // contain, which is what the index remap below handles.
    const bool canReuse = haveLastGrid_ && lastCells_.size() == count && lastBands_.size() == count;

    std::vector<std::shared_ptr<const world::svo::BrickTree>> built(count);
    std::size_t reused = 0;
    std::vector<std::size_t> toBuild;
    toBuild.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const glm::ivec3 coord = grid.coord_of(i);
        if (canReuse) {
            const glm::ivec3 local = coord - lastOriginCell_;
            const glm::ivec3 dims = grid.dims();
            if (local.x >= 0 && local.y >= 0 && local.z >= 0 && local.x < dims.x && local.y < dims.y &&
                local.z < dims.z) {
                const auto old = static_cast<std::size_t>(local.x) +
                                 static_cast<std::size_t>(dims.x) *
                                     (static_cast<std::size_t>(local.y) +
                                      static_cast<std::size_t>(dims.y) * static_cast<std::size_t>(local.z));
                if (lastBands_[old] == bands[i]) {
                    built[i] = lastCells_[old]; // may be null: an EMPTY cell is a valid answer to reuse
                    ++reused;
                    continue;
                }
            }
        }
        toBuild.push_back(i);
    }

    std::vector<std::future<void>> jobs;
    jobs.reserve(toBuild.size());
    for (const std::size_t i : toBuild) {
        jobs.push_back(pool_.submit([&, i] {
            const world::svo::TreeGeometry cg = grid.geometry_for(grid.coord_of(i));
            world::svo::TerrainSampler cellSampler(heightmap_, sp,
                                                   world::svo::Box{cg.origin, cg.max_corner()});
            cellSampler.adopt_focus(tiers);
            world::svo::BuildParams cellParams = bp;
            cellParams.quantized_voxel_edge = world::svo::band_voxel_edge(bands[i], finest, cellEdge);
            world::svo::BrickTree cell =
                world::svo::build_tree(cellSampler, cg, cellParams, nullptr, nullptr);
            if (!cell.empty()) {
                built[i] = std::make_shared<const world::svo::BrickTree>(std::move(cell));
            }
        }));
    }
    for (std::future<void>& f : jobs) {
        f.get();
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (built[i]) {
            grid.set(grid.coord_of(i), built[i]);
        }
    }
    auto flat = std::make_shared<const world::svo::FlatCellGrid>(grid);

    LastBuild last;
    last.stats.seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - buildStart).count();
    // Summed across cells rather than left at zero: the log line prints these, and "0 internal,
    // 0 solid leaves" on a 543 MB world reads as a broken build rather than as an unfilled field.
    for (const std::shared_ptr<const world::svo::BrickTree>& cell : built) {
        if (!cell) {
            continue;
        }
        const world::svo::BrickTree::Stats s = cell->stats();
        last.tree.internal_nodes += s.internal_nodes;
        last.tree.brick_leaves += s.brick_leaves;
        last.tree.solid_leaves += s.solid_leaves;
        last.tree.deepest_level = std::max(last.tree.deepest_level, s.deepest_level);
    }
    last.bricks = flat->bricks().size() / world::svo::kBrickWords;
    last.memory_bytes = static_cast<std::size_t>(flat->memory_bytes());
    last.trees = seeded.trees().size();
    last.sampler_seconds = samplerSeconds;
    last.cells = grid.present_count();
    last.cells_rebuilt = toBuild.size();
    last.cells_reused = reused;
    last.valid = true;

    lastCells_ = built;
    lastBands_ = bands;
    lastOriginCell_ = grid.origin_cell();
    haveLastGrid_ = true;

    const std::lock_guard guard(mutex_);
    finishedGrid_ = std::move(flat);
    lastBuild_ = last;
}

std::shared_ptr<const world::svo::BrickTree> SvoWorld::take_finished() {
    const std::lock_guard guard(mutex_);
    std::shared_ptr<const world::svo::BrickTree> out;
    if (finished_) {
        out = std::move(finished_);
        finished_.reset();
    }
    return out;
}

SvoWorld::LastBuild SvoWorld::last_build() const {
    const std::lock_guard guard(mutex_);
    return lastBuild_;
}


// ---- Goal 264: the streaming producer ---------------------------------------------------------
//
// The shape is fixed once from the camera, exactly as geometry_for does for the single tree, so a
// cell's world position never changes underneath the renderer's resident slots.
world::svo::CellGrid SvoWorld::stream_shape(glm::vec3 camera) const {
    const world::svo::TreeGeometry g = geometry_for(camera);
    const int cellLog2 = std::max(1, options_.cell_size_log2);
    const auto cellEdge = static_cast<float>(std::ldexp(1.0, cellLog2));
    const int perAxis = 1 << (options_.root_size_log2 - cellLog2);
    const glm::ivec3 originCell{static_cast<int>(std::floor(g.origin.x / cellEdge)),
                                static_cast<int>(std::floor(g.origin.y / cellEdge)),
                                static_cast<int>(std::floor(g.origin.z / cellEdge))};
    return world::svo::CellGrid{originCell, glm::ivec3{perAxis}, cellLog2, options_.voxel_size_log2};
}

// The coarse proxy: ONE tree over the whole region at a deliberately coarse voxel size. Built
// synchronously and once -- it is what the first frame draws, so there is nothing to stream it
// behind, and at 1 m voxels over 512 m it is a 9-level tree rather than a 16-level one.
std::shared_ptr<const world::svo::BrickTree> SvoWorld::build_proxy(const world::svo::CellGrid& shape) {
    world::svo::TreeGeometry g;
    g.origin = shape.world_min();
    g.root_size_log2 = options_.root_size_log2;
    g.voxel_size_log2 = options_.proxy_voxel_log2;

    world::svo::TerrainSamplerParams sp;
    sp.seed = options_.seed;
    sp.trees = options_.trees;
    world::svo::TerrainSampler sampler(heightmap_, sp, world::svo::Box{g.origin, g.max_corner()});

    world::svo::BuildParams bp;
    bp.uniform_lod = true; // one level everywhere: it is the fallback, not the detail
    return std::make_shared<const world::svo::BrickTree>(
        world::svo::build_tree(sampler, g, bp, &pool_, nullptr));
}

void SvoWorld::start_stream(const world::svo::CellGrid& shape, glm::vec3 camera) {
    streamShape_ = shape;
    streamCamera_ = camera;
    streamRequested_.assign(shape.cell_count(), 0);
    streamQueueNext_ = 0;
    {
        const std::lock_guard guard(mutex_);
        streamReady_.clear();
    }

    // Nearest-first, which is the ordering that makes the fallback converge where the eye is. The
    // producer is otherwise unordered, so this is the only place priority is expressed.
    streamQueue_.resize(shape.cell_count());
    for (std::size_t i = 0; i < streamQueue_.size(); ++i) {
        streamQueue_[i] = i;
    }
    const float cellEdge = shape.cell_edge();
    std::sort(streamQueue_.begin(), streamQueue_.end(), [&](std::size_t a, std::size_t b) {
        return world::svo::distance_to_cell(camera, shape.coord_of(a), cellEdge) <
               world::svo::distance_to_cell(camera, shape.coord_of(b), cellEdge);
    });

    // The focus tiers, once, shared by every cell (goal 251).
    world::svo::TerrainSamplerParams sp;
    sp.seed = options_.seed;
    sp.trees = options_.trees;
    world::svo::TerrainSampler seed(heightmap_, sp,
                                    world::svo::Box{shape.world_min(), shape.world_max()});
    seed.set_focus(camera, 4.0f * options_.effective_lod_radius());
    streamTiers_ = seed.focus_tiers();
}

void SvoWorld::pump_stream(std::size_t max) {
    if (streamQueueNext_ >= streamQueue_.size()) {
        return;
    }
    world::svo::TerrainSamplerParams sp;
    sp.seed = options_.seed;
    sp.trees = options_.trees;
    world::svo::BuildParams bp;
    bp.lod_center = streamCamera_;
    bp.lod_radius = options_.effective_lod_radius();

    const world::svo::CellGrid shape = streamShape_;
    const float cellEdge = shape.cell_edge();
    const float finest = static_cast<float>(std::ldexp(1.0, options_.voxel_size_log2));
    const glm::vec3 camera = streamCamera_;

    // BOUNDED. The queue is drained a few cells per frame rather than submitted whole, so the pool
    // never holds more outstanding work than the renderer can consume -- and the frame loop never
    // spends more than a few submissions' worth of time here.
    const std::size_t end = std::min(streamQueueNext_ + max, streamQueue_.size());
    for (; streamQueueNext_ < end; ++streamQueueNext_) {
        const std::size_t index = streamQueue_[streamQueueNext_];
        streamRequested_[index] = 1;
        streamInFlight_.fetch_add(1, std::memory_order_relaxed);
        (void)pool_.submit([this, index, shape, sp, bp, cellEdge, finest, camera] {
            const world::svo::TreeGeometry cg = shape.geometry_for(shape.coord_of(index));
            world::svo::TerrainSampler sampler(heightmap_, sp,
                                               world::svo::Box{cg.origin, cg.max_corner()});
            sampler.adopt_focus(streamTiers_);
            world::svo::BuildParams cellParams = bp;
            cellParams.quantized_voxel_edge = world::svo::band_voxel_edge(
                world::svo::cell_band(camera, shape.coord_of(index), cellEdge, options_.effective_lod_radius()),
                finest, cellEdge);
            world::svo::BrickTree cell =
                world::svo::build_tree(sampler, cg, cellParams, nullptr, nullptr);

            BuiltCell out;
            out.index = index;
            if (!cell.empty()) {
                out.tree = std::make_shared<const world::svo::BrickTree>(std::move(cell));
            }
            const std::lock_guard guard(mutex_);
            streamReady_.push_back(std::move(out));
            streamInFlight_.fetch_sub(1, std::memory_order_relaxed);
        });
    }
}

std::vector<SvoWorld::BuiltCell> SvoWorld::take_built_cells(std::size_t max) {
    const std::lock_guard guard(mutex_);
    std::vector<BuiltCell> out;
    const std::size_t take = std::min(max, streamReady_.size());
    out.reserve(take);
    // From the FRONT: the queue fills in completion order, which for a nearest-first submission is
    // approximately nearest-first arrival.
    out.insert(out.end(), std::make_move_iterator(streamReady_.begin()),
               std::make_move_iterator(streamReady_.begin() + static_cast<std::ptrdiff_t>(take)));
    streamReady_.erase(streamReady_.begin(), streamReady_.begin() + static_cast<std::ptrdiff_t>(take));
    return out;
}

std::size_t SvoWorld::stream_pending() const {
    const std::lock_guard guard(mutex_);
    return streamReady_.size() + streamInFlight_.load(std::memory_order_relaxed) +
           (streamQueue_.size() - streamQueueNext_);
}

} // namespace app
