#include <pb/runtime/thread_pool.hpp>
#include <pb/pipeline.hpp>

#include <cassert>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

int main() {
  // Test 1: Construct and destruct
  {
    pb::runtime::thread_pool pool{4};
    assert(pool.worker_count() == 4);
  }

  // Test 2: Enqueue a simple task
  {
    pb::runtime::thread_pool pool{2};
    auto future = pool.enqueue([] { return 42; });
    assert(future.get() == 42);
  }

  // Test 3: Run multiple tasks in parallel
  {
    pb::runtime::thread_pool pool{4};
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 8; ++i) {
      futures.push_back(pool.enqueue([i] { return i * i; }));
    }
    for (int i = 0; i < 8; ++i) {
      assert(futures[static_cast<std::size_t>(i)].get() == i * i);
    }
  }

  // Test 4: Move-only return types
  {
    pb::runtime::thread_pool pool{1};
    auto future = pool.enqueue([] { return std::make_unique<int>(7); });
    auto result = future.get();
    assert(*result == 7);
  }

  // Test 5: Void return type
  {
    pb::runtime::thread_pool pool{1};
    int counter = 0;
    auto future = pool.enqueue([&counter] { ++counter; });
    future.get();
    assert(counter == 1);
  }

  // Test 6: Default thread count (hardware_concurrency)
  {
    pb::runtime::thread_pool pool;
    assert(pool.worker_count() > 0);
  }

  // Test 7: Explicit zero worker count falls back to one worker
  {
    pb::runtime::thread_pool pool{0};
    assert(pool.worker_count() == 1);
    auto future = pool.enqueue([] { return 5; });
    assert(future.get() == 5);
  }

  // Test 8: Task exceptions propagate through futures
  {
    pb::runtime::thread_pool pool{1};
    auto future = pool.enqueue([]() -> int { throw std::runtime_error{"boom"}; });
    try {
      (void)future.get();
      assert(false);
    } catch (const std::runtime_error& exception) {
      assert(std::string_view{exception.what()} == "boom");
    }
  }

  // Test 9: Pool snapshots distinguish queued work from active work
  {
    pb::thread_pool pool{1};
    std::promise<void> started;
    std::promise<void> release;
    auto started_signal = started.get_future();
    auto release_signal = release.get_future();

    auto first = pool.enqueue([&] {
      started.set_value();
      release_signal.wait();
      return 1;
    });
    started_signal.wait();

    auto second = pool.enqueue([] { return 2; });
    const auto busy = pool.snapshot();
    assert(busy.worker_count == 1);
    assert(busy.active_tasks == 1);
    assert(busy.pending_tasks == 1);
    assert(busy.outstanding_tasks() == 2);
    assert(!busy.idle());

    release.set_value();
    pool.wait_idle();
    assert(pool.snapshot().idle());
    assert(first.get() == 1);
    assert(second.get() == 2);
  }

  // Test 10: Workers cannot wait on their own pool becoming idle
  {
    pb::thread_pool pool{1};
    auto future = pool.enqueue([&pool] {
      try {
        pool.wait_idle();
      } catch (const std::logic_error& exception) {
        return std::string_view{exception.what()} ==
               "thread_pool idle waits cannot be called from worker tasks";
      }
      return false;
    });
    assert(future.get());
  }

  // Test 11: Timed idle waits report busy pools and completed drains
  {
    pb::thread_pool pool{1};
    std::promise<void> started;
    std::promise<void> release;
    auto started_signal = started.get_future();
    auto release_signal = release.get_future();

    auto first = pool.enqueue([&] {
      started.set_value();
      release_signal.wait();
      return 3;
    });
    started_signal.wait();

    assert(!pool.wait_idle_for(std::chrono::milliseconds{1}));
    assert(!pool.wait_idle_until(std::chrono::steady_clock::now() + std::chrono::milliseconds{1}));

    release.set_value();
    assert(pool.wait_idle_for(std::chrono::seconds{1}));
    assert(pool.wait_idle_until(std::chrono::steady_clock::now() + std::chrono::seconds{1}));
    assert(first.get() == 3);
  }

  // Test 12: Timed waits use the same worker self-wait guard
  {
    pb::thread_pool pool{1};
    auto future = pool.enqueue([&pool] {
      bool for_guarded = false;
      bool until_guarded = false;
      try {
        (void)pool.wait_idle_for(std::chrono::milliseconds{1});
      } catch (const std::logic_error& exception) {
        for_guarded = std::string_view{exception.what()} ==
                      "thread_pool idle waits cannot be called from worker tasks";
      }
      try {
        (void)pool.wait_idle_until(std::chrono::steady_clock::now() + std::chrono::milliseconds{1});
      } catch (const std::logic_error& exception) {
        until_guarded = std::string_view{exception.what()} ==
                        "thread_pool idle waits cannot be called from worker tasks";
      }
      return for_guarded && until_guarded;
    });
    assert(future.get());
  }

  return 0;
}
