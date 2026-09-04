#include <algorithm>
#include <cassert>
#include <cstdlib>

#include "world/streaming/chunk_streamer.hpp"

namespace world::streaming {

namespace {

using world::chunk::ChunkCoord;

std::int32_t chebyshev(ChunkCoord a, ChunkCoord b) noexcept {
    const std::int32_t dx = std::abs(a.x - b.x);
    const std::int32_t dy = std::abs(a.y - b.y);
    const std::int32_t dz = std::abs(a.z - b.z);
    return std::max({dx, dy, dz});
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

    // All distances are measured from the camera clamped into the terrain band -- the cube
    // follows the camera horizontally but never chases it into empty sky/underground.
    const ChunkCoord anchor{cameraChunk.x, std::clamp(cameraChunk.y, config_.y_min, config_.y_max), cameraChunk.z};

    // Desired set: Chebyshev cube (max-norm, pure integer -- the genre's "render distance"
    // convention) intersected with the vertical band.
    desired_.clear();
    const std::int32_t yLo = std::max(anchor.y - config_.load_radius, config_.y_min);
    const std::int32_t yHi = std::min(anchor.y + config_.load_radius, config_.y_max);
    for (std::int32_t z = anchor.z - config_.load_radius; z <= anchor.z + config_.load_radius; ++z) {
        for (std::int32_t x = anchor.x - config_.load_radius; x <= anchor.x + config_.load_radius; ++x) {
            for (std::int32_t y = yLo; y <= yHi; ++y) {
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
    // R_load) AND temporal hysteresis (continuously outside for unload_delay_seconds).
    for (const ChunkCoord& coord : loaded_) {
        if (chebyshev(coord, anchor) <= config_.unload_radius) {
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
