#include <cstdlib>

#include "engine/core/application.hpp"
#include "engine/core/config.hpp"
#include "engine/core/log.hpp"
#include "engine/jobs/thread_pool.hpp"

int main() {
    using namespace engine::core;

    log(LogLevel::Info, "voxel_app starting");

    engine::jobs::ThreadPool pool;
    log(LogLevel::Info, "job pool started with {} worker thread(s)", pool.thread_count());

    Config config;
    Application app(config);

    // No window/close-event exists yet (that lands in PHASE_1_BRIEF.md's M1.4), so there's no
    // real stop condition yet — run a bounded demo duration instead of hanging forever.
    const auto demo_ticks = static_cast<std::size_t>(config.target_tick_rate_hz * 3.0);
    app.run(demo_ticks);

    log(LogLevel::Info, "voxel_app exiting after {} ticks", demo_ticks);
    return EXIT_SUCCESS;
}
