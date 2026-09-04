#include "engine/jobs/thread_pool.hpp"

namespace engine::jobs {

ThreadPool::ThreadPool(std::size_t thread_count) {
    if (thread_count == 0) {
        thread_count = 1;
    }
    workers_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this](const std::stop_token& stop_token) { worker_loop(stop_token); });
    }
}

ThreadPool::~ThreadPool() {
    for (auto& worker : workers_) {
        worker.request_stop();
    }
    cv_.notify_all();
}

void ThreadPool::worker_loop(std::stop_token stop_token) {
    // Deliberately NOT condition_variable_any::wait(lock, stop_token, pred): that overload
    // reproducibly hit real UB on this toolchain ("unlock of unowned mutex" from the MSVC STL's
    // own mutex.cpp, plus multi-minute hangs) under concurrent load. A plain condition_variable
    // plus an explicit stop_callback is the well-established, far more battle-tested pattern for
    // the same jthread/stop_token wakeup and avoids that overload entirely.
    std::stop_callback wake_on_stop(stop_token, [this] { cv_.notify_all(); });

    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this, &stop_token] { return !tasks_.empty() || stop_token.stop_requested(); });
            if (tasks_.empty()) {
                return; // stop requested (or a spurious wake) with nothing left to drain
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

} // namespace engine::jobs
