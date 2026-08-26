#pragma once

#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace pb::runtime {

class thread_pool;

namespace detail_thread_pool {
template <class F>
void post_trusted(thread_pool& pool, F&& f);
} // namespace detail_thread_pool

struct thread_pool_snapshot {
  std::size_t worker_count{};
  std::size_t pending_tasks{};
  std::size_t active_tasks{};

  [[nodiscard]] constexpr std::size_t outstanding_tasks() const noexcept {
    return pending_tasks + active_tasks;
  }

  [[nodiscard]] constexpr bool idle() const noexcept { return outstanding_tasks() == 0; }
};

class thread_pool {
  class active_task_guard;

  template <class F>
  friend void detail_thread_pool::post_trusted(thread_pool& pool, F&& f);

public:
  explicit thread_pool(std::size_t num_threads = std::thread::hardware_concurrency()) {
    if (num_threads == 0) {
      num_threads = 1;
    }
    workers_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i) {
      workers_.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
            ++active_tasks_;
          }
          const active_task_guard guard{*this};
          task();
        }
      });
    }
  }

  ~thread_pool() {
    {
      std::unique_lock lock(mutex_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto& w : workers_) {
      if (w.joinable()) w.join();
    }
  }

  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;
  thread_pool(thread_pool&&) = delete;
  thread_pool& operator=(thread_pool&&) = delete;

  template <class F>
  auto enqueue(F&& f) -> std::future<decltype(f())> {
    using R = decltype(f());
    auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
    auto future = task->get_future();
    {
      std::unique_lock lock(mutex_);
      if (stop_) {
        throw std::runtime_error("enqueue on stopped thread_pool");
      }
      tasks_.emplace([task] { (*task)(); });
    }
    cv_.notify_one();
    return future;
  }

  template <class Iterator>
  auto enqueue_range(Iterator begin, Iterator end)
      -> std::vector<std::future<std::invoke_result_t<typename std::iterator_traits<Iterator>::value_type>>> {
    using R = std::invoke_result_t<typename std::iterator_traits<Iterator>::value_type>;
    std::vector<std::future<R>> futures;
    futures.reserve(std::distance(begin, end));

    {
      std::unique_lock lock(mutex_);
      if (stop_) {
        throw std::runtime_error("enqueue_range on stopped thread_pool");
      }

      for (auto it = begin; it != end; ++it) {
        auto task = std::make_shared<std::packaged_task<R()>>(*it);
        futures.push_back(task->get_future());
        tasks_.emplace([task] { (*task)(); });
      }
    }
    cv_.notify_all();
    return futures;
  }

  [[nodiscard]] std::size_t pending_tasks() const {
    std::unique_lock lock(mutex_);
    return tasks_.size();
  }

  [[nodiscard]] std::size_t queued_tasks() const { return pending_tasks(); }

  [[nodiscard]] std::size_t active_tasks() const {
    std::unique_lock lock(mutex_);
    return active_tasks_;
  }

  [[nodiscard]] thread_pool_snapshot snapshot() const {
    std::unique_lock lock(mutex_);
    return thread_pool_snapshot{.worker_count = workers_.size(),
                                .pending_tasks = tasks_.size(),
                                .active_tasks = active_tasks_};
  }

  void wait_idle() {
    ensure_external_waiter();
    std::unique_lock lock(mutex_);
    idle_cv_.wait(lock, [this] { return tasks_.empty() && active_tasks_ == 0; });
  }

  template <class Rep, class Period>
  [[nodiscard]] bool wait_idle_for(const std::chrono::duration<Rep, Period>& timeout) {
    ensure_external_waiter();
    std::unique_lock lock(mutex_);
    return idle_cv_.wait_for(lock, timeout, [this] { return tasks_.empty() && active_tasks_ == 0; });
  }

  template <class Clock, class Duration>
  [[nodiscard]] bool wait_idle_until(const std::chrono::time_point<Clock, Duration>& deadline) {
    ensure_external_waiter();
    std::unique_lock lock(mutex_);
    return idle_cv_.wait_until(lock, deadline, [this] { return tasks_.empty() && active_tasks_ == 0; });
  }

  [[nodiscard]] std::size_t worker_count() const noexcept { return workers_.size(); }

private:
  template <class F>
  void post(F&& f) {
    {
      std::unique_lock lock(mutex_);
      if (stop_) {
        throw std::runtime_error("post on stopped thread_pool");
      }
      tasks_.emplace(std::forward<F>(f));
    }
    cv_.notify_one();
  }

  class active_task_guard {
  public:
    explicit active_task_guard(thread_pool& owner) noexcept : owner_{owner}, previous_pool_{current_pool_} {
      current_pool_ = &owner_;
    }

    ~active_task_guard() noexcept {
      current_pool_ = previous_pool_;
      owner_.finish_task();
    }

    active_task_guard(const active_task_guard&) = delete;
    active_task_guard& operator=(const active_task_guard&) = delete;

  private:
    thread_pool& owner_;
    const thread_pool* previous_pool_{};
  };

  void ensure_external_waiter() const {
    if (current_pool_ == this) {
      throw std::logic_error{"thread_pool idle waits cannot be called from worker tasks"};
    }
  }

  void finish_task() {
    bool notify_idle = false;
    {
      std::unique_lock lock(mutex_);
      if (active_tasks_ > 0) {
        --active_tasks_;
      }
      notify_idle = tasks_.empty() && active_tasks_ == 0;
    }
    if (notify_idle) {
      idle_cv_.notify_all();
    }
  }

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable idle_cv_;
  std::size_t active_tasks_{0};
  bool stop_{false};
  static inline thread_local const thread_pool* current_pool_{nullptr};
};

namespace detail_thread_pool {

template <class F>
void post_trusted(thread_pool& pool, F&& f) {
  pool.post(std::forward<F>(f));
}

} // namespace detail_thread_pool

} // namespace pb::runtime

