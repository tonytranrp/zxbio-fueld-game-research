#include "engine/core/application.hpp"

#include <chrono>
#include <thread>

namespace engine::core {

void Application::run(std::size_t max_ticks) {
    using Duration = std::chrono::steady_clock::duration;
    const auto target_frame_time = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>(1.0 / config_.target_tick_rate_hz));

    std::size_t tick_count = 0;
    while (true) {
        const auto frame_start = std::chrono::steady_clock::now();

        clock_.tick();
        ++tick_count;

        if (max_ticks != 0 && tick_count >= max_ticks) {
            break;
        }

        const auto elapsed = std::chrono::steady_clock::now() - frame_start;
        const auto sleep_duration = target_frame_time - elapsed;
        if (sleep_duration > Duration::zero()) {
            std::this_thread::sleep_for(sleep_duration);
        }
    }
}

} // namespace engine::core
