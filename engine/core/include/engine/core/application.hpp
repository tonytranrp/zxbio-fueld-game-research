#pragma once

#include <cstddef>

#include "engine/core/clock.hpp"
#include "engine/core/config.hpp"

namespace engine::core {

// The engine skeleton's tick loop. No window/GPU/close-event exists yet (that lands in
// Phase 1 brief's M1.4), so there is no real stop condition beyond an explicit tick budget —
// max_ticks == 0 runs until stop() is called from within a tick callback in a later phase.
class Application {
public:
    explicit Application(Config config) noexcept : config_(config) {}

    void run(std::size_t max_ticks = 0);

    [[nodiscard]] const Clock& clock() const noexcept { return clock_; }

private:
    Config config_;
    Clock clock_;
};

} // namespace engine::core
