#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/coord_containers.hpp"

namespace world::streaming {

// Tuning for the load/unload decision (Phase 1 completion brief §2.2). Two radii plus a time
// delay, not one radius: a single boundary thrashes load/unload for a camera hovering near it --
// the real, shipped fix (PaperMC's delayed-unload patch) is hysteresis in both space and time.
struct StreamingConfig {
    std::int32_t load_radius = 3;   // R_load: desired columns within this HORIZONTAL Chebyshev
                                    // distance (X/Z only -- Y is never distance-tested; see below)
    std::int32_t unload_radius = 5; // R_unload: only columns beyond this are unload candidates.
                                    // Keep >= load_radius + 2: the gap guarantees an unloaded
                                    // chunk is never inside the 1-chunk meshing halo of any
                                    // still-desired chunk, which is what makes dropping its
                                    // voxel data on unload safe by construction.
    double unload_delay_seconds = 2.0; // continuously outside R_unload for this long before unload
    // The world's generated vertical extent in chunk-Y: every desired column loads this FULL
    // band regardless of camera altitude (the ribbon-bug fix, TERRAIN_FIXES_BRIEF Group Q --
    // applying the radius to Y loaded only a camera-relative altitude band and rendered the
    // terrain as thin camera-following ribbons). Derived from the generator, not the camera:
    // heightmap amplitude is +/-64 world-Y around sea level (surface chunks [-3, 1]); [-3, 2]
    // covers that with headroom. Update alongside kAmplitude in heightmap_generator.cpp.
    std::int32_t y_min = -3;
    std::int32_t y_max = 2;
};

// Pure decision logic for chunk streaming: what to start loading, what to unload, and whether a
// finished job's result is still wanted. A system, not storage (ChunkStore keeps owning voxel
// data) and deliberately thread-free -- the caller ticks it from one thread and runs the actual
// pipeline; that split is what makes the hysteresis rules unit-testable with a fake clock.
//
// Chunk lifecycle as this class tracks it:
//   (untracked) -> in-flight (returned in start_loading) -> loaded (mark_loaded)
//                                 |                            |
//                                 v mark_discarded             v returned in unload
//                             (untracked)                  (untracked)
// In-flight jobs are never cancelled (§2.2's deliberate v1: let them finish, check is_desired()
// on completion, discard stale results); upgrading to stop_token cancellation waits for a
// measurement showing discarded work actually costs meaningful CPU.
class ChunkStreamer {
public:
    explicit ChunkStreamer(StreamingConfig config);

    struct TickCommands {
        std::vector<world::chunk::ChunkCoord> start_loading; // newly desired: begin generate->mesh->upload for each
        std::vector<world::chunk::ChunkCoord> unload;        // hysteresis expired: tear down store/ECS/GPU for each
    };

    // Recomputes the desired set around the camera's chunk coordinate and diffs it against what
    // is loaded/in flight. `nowSeconds` is any monotonically increasing clock; the fixed cadence
    // (this does not need to run every frame) is the caller's choice.
    [[nodiscard]] TickCommands tick(world::chunk::ChunkCoord cameraChunk, double nowSeconds);

    // Task 24's completion check: is this coordinate still in the desired set computed by the
    // most recent tick()?
    [[nodiscard]] bool is_desired(world::chunk::ChunkCoord coord) const noexcept;

    // Pipeline finished and the result was applied (uploaded): in-flight -> loaded.
    void mark_loaded(world::chunk::ChunkCoord coord);
    // Pipeline finished but the result was stale and thrown away: in-flight -> untracked. The
    // coordinate is re-requested by a later tick() if it becomes desired again.
    void mark_discarded(world::chunk::ChunkCoord coord);

    [[nodiscard]] std::size_t loaded_count() const noexcept { return loaded_.size(); }
    [[nodiscard]] std::size_t in_flight_count() const noexcept { return in_flight_.size(); }

    // Diagnostic for the ribbon-bug investigation (TERRAIN_FIXES Group Q task 1): the min/max
    // chunk-Y currently loaded. {0,-1} (empty range) when nothing is loaded.
    [[nodiscard]] std::pair<std::int32_t, std::int32_t> loaded_y_range() const noexcept {
        std::int32_t lo = 0;
        std::int32_t hi = -1;
        bool first = true;
        for (const world::chunk::ChunkCoord& coord : loaded_) {
            lo = first ? coord.y : std::min(lo, coord.y);
            hi = first ? coord.y : std::max(hi, coord.y);
            first = false;
        }
        return {lo, hi};
    }
    [[nodiscard]] const StreamingConfig& config() const noexcept { return config_; }

private:
    StreamingConfig config_;
    // CoordSet/CoordMap (Group H task 11): same flat containers as ChunkStore, behind the same
    // alias boundary. desired_ is rebuilt every tick -- flat storage also retires most of the
    // per-tick node churn task 39 flagged (clear() keeps capacity, inserts stop allocating).
    world::chunk::CoordSet desired_;
    world::chunk::CoordSet in_flight_;
    world::chunk::CoordSet loaded_;
    // First instant a loaded chunk was seen outside R_unload; erased the moment it comes back
    // inside ("continuously outside" is the rule, not cumulative time outside).
    world::chunk::CoordMap<double> outside_since_;
};

} // namespace world::streaming
