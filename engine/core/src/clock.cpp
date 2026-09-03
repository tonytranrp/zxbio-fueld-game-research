#include "engine/core/clock.hpp"

namespace engine::core {

Clock::Clock() noexcept : start_(ClockType::now()), last_tick_(start_) {}

void Clock::tick() noexcept {
    const auto now = ClockType::now();
    delta_seconds_ = std::chrono::duration<double>(now - last_tick_).count();
    elapsed_seconds_ = std::chrono::duration<double>(now - start_).count();
    last_tick_ = now;
}

} // namespace engine::core
