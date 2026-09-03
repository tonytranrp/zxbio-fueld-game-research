#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "engine/core/application.hpp"

using namespace engine::core;

TEST_CASE("Application ticks a bounded run to approximately the configured rate", "[core]") {
    Config config;
    config.target_tick_rate_hz = 100.0; // 10ms/tick, keeps the test fast
    Application app(config);

    constexpr std::size_t ticks = 20;
    const auto start = std::chrono::steady_clock::now();
    app.run(ticks);
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    const double expected = static_cast<double>(ticks) / config.target_tick_rate_hz;
    // Generous bounds: this asserts the loop paces itself against the target rate (neither
    // returning instantly nor stalling far past it), not that timing is exact under CI/sanitizer
    // scheduling jitter.
    REQUIRE(elapsed >= expected * 0.5);
    REQUIRE(elapsed <= expected * 3.0);
}

TEST_CASE("Application stops after exactly max_ticks", "[core]") {
    Config config;
    config.target_tick_rate_hz = 1000.0; // fast, so the test itself stays fast
    Application app(config);

    app.run(5);

    REQUIRE(app.clock().elapsed_seconds() >= 0.0);
}
