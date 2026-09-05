#include "engine/jobs/thread_pool.hpp"

#include <stop_token>
#include <thread>
#include <vector>

#include <blockingconcurrentqueue.h>

namespace engine::jobs {

// The §7 rule, re-verified on the redesigned class (Group I task 14's check): members destruct
// in reverse declaration order, so workers_ is declared LAST -- its destructor (join) runs
// before queue_ is torn down. The explicit ~ThreadPool below makes the join happen even earlier
// (after waking every worker), but the declaration order stays correct on its own so a future
// refactor of the destructor can't silently reintroduce M1.1's bug.
struct ThreadPool::Impl {
    moodycamel::BlockingConcurrentQueue<std::function<void()>> queue;
    std::vector<std::jthread> workers;
};

namespace {

// Worker loop. An EMPTY std::function is the shutdown sentinel: BlockingConcurrentQueue has no
// std::stop_token-aware wait (long-standing upstream limitation), so instead of a timed-wait
// polling loop the destructor wakes each worker exactly once with one sentinel per worker. Each
// worker consumes at most one sentinel and exits WITHOUT draining -- if it drained, it could
// swallow sibling workers' sentinels and deadlock them in wait_dequeue; leftover real jobs are
// instead finished by the destructor thread after the join (same "destruction finishes pending
// work, it doesn't abandon it" contract as the original mutex+condvar design).
//
// The std::stop_token stays in the signature deliberately: std::jthread + stop_token remains
// this project's cooperative-cancellation shape (§7), and a future long-running job type can be
// handed the token; the sentinel replaces only the WAKE mechanism, not the cancellation model.
void worker_loop(const std::stop_token& /*stop_token*/, ThreadPool::Impl& impl) {
    std::function<void()> job;
    for (;;) {
        impl.queue.wait_dequeue(job);
        if (!job) {
            return; // sentinel
        }
        job();
        job = nullptr; // release captures promptly, not only on next dequeue
    }
}

} // namespace

ThreadPool::ThreadPool(std::size_t thread_count) : impl_(std::make_unique<Impl>()) {
    if (thread_count == 0) {
        thread_count = 1;
    }
    impl_->workers.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        impl_->workers.emplace_back(
            [impl = impl_.get()](const std::stop_token& stopToken) { worker_loop(stopToken, *impl); });
    }
}

ThreadPool::~ThreadPool() {
    for (auto& worker : impl_->workers) {
        worker.request_stop(); // keeps the cooperative-cancellation channel truthful for jobs
    }
    for (std::size_t i = 0; i < impl_->workers.size(); ++i) {
        impl_->queue.enqueue(std::function<void()>{}); // one wake sentinel per worker
    }
    impl_->workers.clear(); // joins

    // Per-producer FIFO means another producer's job can sit behind a worker's sentinel; finish
    // whatever the workers didn't get to, preserving the original drain-on-destruction contract.
    std::function<void()> job;
    while (impl_->queue.try_dequeue(job)) {
        if (job) {
            job();
        }
    }
}

std::size_t ThreadPool::thread_count() const noexcept {
    return impl_->workers.size();
}

std::size_t ThreadPool::default_thread_count() noexcept {
    const unsigned int n = std::jthread::hardware_concurrency();
    return n == 0 ? 1 : n;
}

void ThreadPool::enqueue_task(std::function<void()> task) {
    impl_->queue.enqueue(std::move(task));
}

} // namespace engine::jobs
