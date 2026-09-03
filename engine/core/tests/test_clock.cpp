#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "engine/core/clock.hpp"

using namespace engine::core;

TEST_CASE("Clock measures elapsed and delta time", "[core]") {
    Clock clock;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    clock.tick();

    REQUIRE(clock.delta_seconds() >= 0.005);
    REQUIRE(clock.elapsed_seconds() >= 0.005);
}

TEST_CASE("Clock accumulates elapsed time across multiple ticks", "[core]") {
    Clock clock;
    clock.tick();
    const double first_elapsed = clock.elapsed_seconds();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    clock.tick();

    REQUIRE(clock.elapsed_seconds() >= first_elapsed);
}
