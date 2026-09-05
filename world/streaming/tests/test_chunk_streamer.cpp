#include <algorithm>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/streaming/chunk_streamer.hpp"

using world::chunk::ChunkCoord;
using world::streaming::ChunkStreamer;
using world::streaming::StreamingConfig;

namespace {

StreamingConfig small_config() {
    StreamingConfig config;
    config.load_radius = 1;
    config.unload_radius = 3;
    config.unload_delay_seconds = 2.0;
    config.y_min = -3;
    config.y_max = 2;
    return config;
}

bool contains(const std::vector<ChunkCoord>& coords, ChunkCoord coord) {
    return std::find(coords.begin(), coords.end(), coord) != coords.end();
}

// Drive every start_loading command straight to loaded -- the tests below are about the
// decision rules, not the pipeline the real caller runs in between.
void complete_all(ChunkStreamer& streamer, const std::vector<ChunkCoord>& coords) {
    for (const ChunkCoord& coord : coords) {
        streamer.mark_loaded(coord);
    }
}

} // namespace

TEST_CASE("First tick requests full-band columns within the horizontal radius", "[streaming]") {
    ChunkStreamer streamer(small_config());
    const auto commands = streamer.tick({0, 0, 0}, 0.0);

    // radius 1 => x,z in [-1,1]; y ALWAYS spans the whole band [-3,2] (the ribbon-bug fix:
    // render distance is horizontal, columns are full-height) => 3*3*6 = 54.
    CHECK(commands.start_loading.size() == 54);
    CHECK(commands.unload.empty());
    CHECK(streamer.in_flight_count() == 54);
    CHECK(contains(commands.start_loading, {1, 1, 1}));
    CHECK(contains(commands.start_loading, {0, -3, 0}));      // band bottom loads even at camera y=0
    CHECK(contains(commands.start_loading, {0, 2, 0}));       // band top too
    CHECK_FALSE(contains(commands.start_loading, {2, 0, 0})); // outside the horizontal radius

    // Same camera, nothing completed: no duplicate requests while in flight.
    const auto again = streamer.tick({0, 0, 0}, 0.1);
    CHECK(again.start_loading.empty());
    CHECK(again.unload.empty());
}

TEST_CASE("Camera altitude never narrows the loaded band (the ribbon-bug regression test)", "[streaming]") {
    ChunkStreamer streamer(small_config());

    // Camera far above the world: every desired column still spans the FULL band -- the old
    // radius-on-Y behavior cropped this to a camera-relative slice and rendered ribbons.
    const auto commands = streamer.tick({0, 20, 0}, 0.0);
    CHECK(commands.start_loading.size() == 54);
    std::int32_t yLo = 100;
    std::int32_t yHi = -100;
    for (const ChunkCoord& coord : commands.start_loading) {
        yLo = std::min(yLo, coord.y);
        yHi = std::max(yHi, coord.y);
    }
    CHECK(yLo == -3);
    CHECK(yHi == 2);
    complete_all(streamer, commands.start_loading);

    // Climbing even higher must NOT unload the terrain underneath (distance is horizontal-only).
    const auto higher = streamer.tick({0, 100, 0}, 10.0);
    CHECK(higher.unload.empty());
    const auto muchLater = streamer.tick({0, 100, 0}, 100.0);
    CHECK(muchLater.unload.empty());
}

TEST_CASE("Moving one chunk sideways requests the new edge and unloads nothing (spatial hysteresis)",
          "[streaming]") {
    ChunkStreamer streamer(small_config());
    complete_all(streamer, streamer.tick({0, 0, 0}, 0.0).start_loading);

    // Camera to x=1: new x=2 column desired; the x=-1 column is now at distance 2 <= R_unload=3,
    // so it stays loaded even though it left the load radius -- the exact single-radius thrash
    // case the two-radii design exists to prevent.
    const auto commands = streamer.tick({1, 0, 0}, 1.0);
    CHECK(commands.unload.empty());
    CHECK(commands.start_loading.size() == 18); // one new x-slab: 3 z-columns x 6 band layers
    for (const ChunkCoord& coord : commands.start_loading) {
        CHECK(coord.x == 2);
    }
}

TEST_CASE("Unload requires being continuously outside R_unload for the configured delay", "[streaming]") {
    ChunkStreamer streamer(small_config());
    complete_all(streamer, streamer.tick({0, 0, 0}, 0.0).start_loading);

    // Jump far away: everything is outside R_unload, but the delay clock starts now.
    CHECK(streamer.tick({100, 0, 0}, 10.0).unload.empty());
    // Half the delay later: still nothing.
    CHECK(streamer.tick({100, 0, 0}, 11.0).unload.empty());
    // Past the delay: all 54 old chunks unload.
    const auto expired = streamer.tick({100, 0, 0}, 12.5).unload;
    CHECK(expired.size() == 54);
    CHECK(streamer.loaded_count() == 0);
}

TEST_CASE("Returning inside R_unload resets the delay clock -- outside time is continuous, not cumulative",
          "[streaming]") {
    ChunkStreamer streamer(small_config());
    complete_all(streamer, streamer.tick({0, 0, 0}, 0.0).start_loading);

    CHECK(streamer.tick({100, 0, 0}, 10.0).unload.empty()); // outside, clock starts (1.5s will elapse)
    CHECK(streamer.tick({0, 0, 0}, 11.5).unload.empty());   // back inside: clock must reset
    CHECK(streamer.tick({100, 0, 0}, 12.0).unload.empty()); // outside again: clock restarts here
    // 1.5s after the restart: cumulative outside time is 3s > 2s, continuous is only 1.5s < 2s.
    CHECK(streamer.tick({100, 0, 0}, 13.5).unload.empty());
    // 2s after the restart: now it goes.
    CHECK(streamer.tick({100, 0, 0}, 14.0).unload.size() == 54);
}

TEST_CASE("A stale completion is discardable and the coordinate is re-requested when desired again",
          "[streaming]") {
    ChunkStreamer streamer(small_config());
    const auto first = streamer.tick({0, 0, 0}, 0.0);
    const ChunkCoord probe{0, 0, 0};
    REQUIRE(contains(first.start_loading, probe));

    // Camera leaves before the job finishes: the coordinate is no longer desired (task 24's
    // completion-time check), so the caller discards the result.
    (void)streamer.tick({100, 0, 0}, 1.0);
    CHECK_FALSE(streamer.is_desired(probe));
    streamer.mark_discarded(probe);
    CHECK(streamer.loaded_count() == 0);

    // Camera comes back: the discarded coordinate must be requested again, not remembered as
    // loaded or stuck as in-flight.
    const auto back = streamer.tick({0, 0, 0}, 2.0);
    CHECK(contains(back.start_loading, probe));
}
