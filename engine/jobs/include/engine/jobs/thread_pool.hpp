#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace engine::jobs {

// A std::jthread-based pool: request_stop() + automatic join on destruction, no hand-rolled
// atomic-bool cancellation flag. Workers drain whatever is already queued before noticing a
// stop request (see worker_loop) — destruction finishes pending work, it doesn't abandon it.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count = std::jthread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ReturnType> result = task->get_future();

        {
            std::lock_guard lock(mutex_);
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    [[nodiscard]] std::size_t thread_count() const noexcept { return workers_.size(); }

private:
    void worker_loop(std::stop_token stop_token);

    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace engine::jobs
