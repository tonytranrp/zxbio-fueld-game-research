#pragma once

#include <chrono>

namespace engine::core {

class Clock {
public:
    Clock() noexcept;

    void tick() noexcept;

    [[nodiscard]] double delta_seconds() const noexcept { return delta_seconds_; }
    [[nodiscard]] double elapsed_seconds() const noexcept { return elapsed_seconds_; }

private:
    using ClockType = std::chrono::steady_clock;

    ClockType::time_point start_;
    ClockType::time_point last_tick_;
    double delta_seconds_ = 0.0;
    double elapsed_seconds_ = 0.0;
};

} // namespace engine::core
