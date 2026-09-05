#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>

namespace engine::jobs {

// A std::jthread-based pool. Since the Group I hardening pass, the interior queue is
// moodycamel::BlockingConcurrentQueue (lock-free MPMC; a modified copy already ships in this
// binary via Tracy's client) instead of mutex + std::queue + condition_variable_any. The public
// API and its guarantees are unchanged: submit() still returns a std::future, and destruction
// still finishes every already-submitted job rather than abandoning it.
//
// Ordering note (Group I task 15): the old queue was global-FIFO; the new one is per-producer
// FIFO. Verified before the swap: nothing depends on cross-producer submission order -- the only
// production submitter is WorldLoader's main-thread begin()/pump() (single producer, so
// per-producer FIFO degenerates to the old global FIFO anyway), and gen->mesh sequencing is
// enforced by its explicit state machine, never by queue order.
//
// The concrete queue type lives behind Impl (engine/jobs/src/thread_pool.cpp) so this public
// header stays free of third-party includes; submit() type-erases into std::function exactly as
// the old design already did.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count = default_thread_count());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ReturnType> result = task->get_future();
        enqueue_task([task] { (*task)(); });
        return result;
    }

    [[nodiscard]] std::size_t thread_count() const noexcept;

    // Public forward declaration only so thread_pool.cpp's file-local worker loop can name it
    // (same MSVC C2248 lesson as TerrainRenderer::Impl); complete type never leaves the .cpp.
    struct Impl;

private:
    static std::size_t default_thread_count() noexcept;
    void enqueue_task(std::function<void()> task);

    // All state -- queue AND workers -- lives in Impl, where the §7 member-order rule is applied
    // (workers declared last so they join before the queue dies); see the .cpp.
    std::unique_ptr<Impl> impl_;
};

} // namespace engine::jobs
