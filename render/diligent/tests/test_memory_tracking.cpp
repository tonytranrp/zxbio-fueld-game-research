#include <cstdint>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "render/diligent/memory_tracking.hpp"

using render::diligent::GpuAllocationTracker;

TEST_CASE("Allocation tracker counts allocate/free pairs and remembers the peak", "[memory]") {
    GpuAllocationTracker tracker;
    CHECK(tracker.allocated_bytes() == 0);
    CHECK(tracker.peak_bytes() == 0);

    tracker.on_allocate(1000);
    tracker.on_allocate(500);
    CHECK(tracker.allocated_bytes() == 1500);
    CHECK(tracker.peak_bytes() == 1500);

    tracker.on_free(1000);
    CHECK(tracker.allocated_bytes() == 500);
    CHECK(tracker.peak_bytes() == 1500); // peak survives frees

    tracker.on_allocate(200);
    CHECK(tracker.allocated_bytes() == 700);
    CHECK(tracker.peak_bytes() == 1500); // a lower high-water mark never lowers the peak
}

TEST_CASE("Allocation tracker is consistent under concurrent allocate/free", "[memory]") {
    GpuAllocationTracker tracker;
    constexpr int kThreads = 8;
    constexpr int kIterations = 10000;
    constexpr std::uint64_t kBytes = 64;

    {
        std::vector<std::jthread> workers;
        workers.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            workers.emplace_back([&tracker] {
                for (int i = 0; i < kIterations; ++i) {
                    tracker.on_allocate(kBytes);
                    tracker.on_free(kBytes);
                }
            });
        }
    } // jthreads join here

    CHECK(tracker.allocated_bytes() == 0);
    CHECK(tracker.peak_bytes() >= kBytes);
    CHECK(tracker.peak_bytes() <= kBytes * kThreads);
}
