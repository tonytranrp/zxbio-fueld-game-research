#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/jobs/thread_pool.hpp"

using namespace engine::jobs;

TEST_CASE("ThreadPool executes every submitted task exactly once", "[jobs]") {
    ThreadPool pool(4);
    constexpr int task_count = 200;
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    futures.reserve(task_count);
    for (int i = 0; i < task_count; ++i) {
        futures.push_back(pool.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
    }
    for (auto& f : futures) {
        f.wait();
    }

    REQUIRE(counter.load() == task_count);
}

TEST_CASE("ThreadPool propagates return values through futures", "[jobs]") {
    ThreadPool pool(2);
    auto future = pool.submit([](int a, int b) { return a + b; }, 3, 4);
    REQUIRE(future.get() == 7);
}

TEST_CASE("ThreadPool reports the requested thread count", "[jobs]") {
    ThreadPool pool(3);
    REQUIRE(pool.thread_count() == 3);
}

TEST_CASE("ThreadPool joins cleanly on destruction with work still pending", "[jobs]") {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 50; ++i) {
            pool.submit([&counter] {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        // Pool destructs here; the destructor must drain the queue and join, not abandon work.
    }
    REQUIRE(counter.load() == 50);
}
