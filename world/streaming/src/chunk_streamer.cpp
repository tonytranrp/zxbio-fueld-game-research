#include <algorithm>
#include <cassert>
#include <cstdlib>

#include "world/streaming/chunk_streamer.hpp"

namespace world::streaming {

namespace {

using world::chunk::ChunkCoord;

// HORIZONTAL Chebyshev only -- Y is deliberately not part of streaming distance since the
// ribbon-bug fix (TERRAIN_FIXES_BRIEF Group Q): render distance is a horizontal radius over
// full-height columns (how Minecraft-style streaming actually works), never a vertical band
// around the camera's altitude.
std::int32_t chebyshev_xz(ChunkCoord a, ChunkCoord b) noexcept {
    const std::int32_t dx = std::abs(a.x - b.x);
    const std::int32_t dz = std::abs(a.z - b.z);
    return std::max(dx, dz);
}

} // namespace

ChunkStreamer::ChunkStreamer(StreamingConfig config) : config_(config) {
    assert(config_.unload_radius >= config_.load_radius + 2 &&
           "hysteresis gap < 2 breaks both the anti-thrash rationale and the drop-voxel-data-on-"
           "unload safety argument (see StreamingConfig)");
    assert(config_.y_min <= config_.y_max);
}

ChunkStreamer::TickCommands ChunkStreamer::tick(ChunkCoord cameraChunk, double nowSeconds) {
    TickCommands commands;

    // Horizontal anchor only. The camera's altitude deliberately plays NO part in the desired
    // set -- the ribbon bug (TERRAIN_FIXES_BRIEF §0): applying the radius to Y loaded only a
    // camera-relative altitude band, so valleys/hilltops outside that band never streamed in and
    // the terrain rendered as thin camera-following ribbons. Verified before fixing: the loaded
    // Y-range log showed exactly the clamped band (e.g. 196 ready = 7x7 columns x only 4 of the
    // 6 band layers with the camera above the band).
    const ChunkCoord anchor{cameraChunk.x, 0, cameraChunk.z};

    // Desired set: horizontal Chebyshev square (the genre's actual "render distance" meaning)
    // of FULL vertical columns spanning the terrain band. The band is the world's generated
    // vertical extent, not a camera window: heightmap amplitude is +/-64 world units around sea
    // level 0 (heightmap_generator.cpp kAmplitude), i.e. surface chunks span [-3, 1]; config's
    // [y_min, y_max] = [-3, 2] covers that with one chunk of headroom (Group Q task 3's
    // documented bound -- a config constant derived from the generator, not "unbounded").
    desired_.clear();
    for (std::int32_t z = anchor.z - config_.load_radius; z <= anchor.z + config_.load_radius; ++z) {
        for (std::int32_t x = anchor.x - config_.load_radius; x <= anchor.x + config_.load_radius; ++x) {
            for (std::int32_t y = config_.y_min; y <= config_.y_max; ++y) {
                desired_.insert(ChunkCoord{x, y, z});
            }
        }
    }

    // Diff desired against loaded + in-flight -> new load requests.
    for (const ChunkCoord& coord : desired_) {
        if (!loaded_.contains(coord) && !in_flight_.contains(coord)) {
            in_flight_.insert(coord);
            commands.start_loading.push_back(coord);
        }
    }

    // Unload pass over loaded chunks: spatial hysteresis (beyond R_unload, not merely outside
    // R_load) AND temporal hysteresis (continuously outside for unload_delay_seconds). Distance
    // is horizontal-only, matching the desired set -- whole columns load and unload together.
    for (const ChunkCoord& coord : loaded_) {
        if (chebyshev_xz(coord, anchor) <= config_.unload_radius) {
            outside_since_.erase(coord); // back inside: the "continuously" clock restarts from zero
            continue;
        }
        const auto [it, inserted] = outside_since_.try_emplace(coord, nowSeconds);
        // First-seen entries have elapsed == 0, so they only pass immediately when the
        // configured delay is zero -- which is exactly what a zero delay should mean.
        if (nowSeconds - it->second >= config_.unload_delay_seconds) {
            commands.unload.push_back(coord);
        }
    }
    for (const ChunkCoord& coord : commands.unload) {
        loaded_.erase(coord);
        outside_since_.erase(coord);
    }

    return commands;
}

bool ChunkStreamer::is_desired(ChunkCoord coord) const noexcept {
    return desired_.contains(coord);
}

void ChunkStreamer::mark_loaded(ChunkCoord coord) {
    in_flight_.erase(coord);
    loaded_.insert(coord);
}

void ChunkStreamer::mark_discarded(ChunkCoord coord) {
    in_flight_.erase(coord);
}

} // namespace world::streaming
