#pragma once

#include <coroutine>
#include <concepts>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include "pb/core/concepts.hpp"

namespace pb {

// ===========================================================================
// pb::coro::sync_task<T>
//
// A minimal, library-owned, *eager* coroutine task type intended for use with
// the SYNCHRONOUS sequential engine.  The coroutine body runs to completion
// eagerly (the promise's initial suspend point is std::suspend_never), so by
// the time the sync_task object is handed back to the caller its result is
// already available and can be retrieved synchronously via .get().
//
// This deliberately does NOT introduce any asynchronous scheduling: there is no
// awaiter that suspends the calling thread, no executor, and no continuation
// machinery.  A sync_task therefore behaves like an ordinary value-returning
// function for the purposes of the sequential engine.  True async / sender
// backends (e.g. suspending awaiters resumed by a scheduler) remain future
// work; this type is the smallest building block needed to make coroutine
// stages runnable today.
//
// Supported coroutine shape:
//   pb::coro::sync_task<R> my_stage(InputType in) { ... co_return value; }
// where value is convertible to R.  co_await inside the body is permitted as
// long as every awaited expression is itself ready synchronously (e.g. another
// eager sync_task); awaiting something that genuinely suspends is undefined for
// this synchronous adapter.
// ===========================================================================
namespace coro {

template <class T>
class sync_task;

namespace detail {

template <class T>
struct sync_task_promise_base {
  std::exception_ptr error_{};

  std::suspend_never initial_suspend() const noexcept { return {}; }
  std::suspend_always final_suspend() const noexcept { return {}; }

  void unhandled_exception() noexcept { error_ = std::current_exception(); }
};

} // namespace detail

/// Eager coroutine task whose result is retrievable synchronously.
///
/// \tparam T The awaited result type produced by `co_return`.
template <class T>
class sync_task {
 public:
  using value_type = T;

  struct promise_type : detail::sync_task_promise_base<T> {
    std::optional<T> value_{};

    sync_task get_return_object() noexcept {
      return sync_task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    template <class U>
      requires std::convertible_to<U&&, T>
    void return_value(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) {
      value_.emplace(std::forward<U>(value));
    }
  };

  sync_task() noexcept = default;

  explicit sync_task(std::coroutine_handle<promise_type> handle) noexcept : handle_{handle} {}

  sync_task(sync_task&& other) noexcept
      : handle_{std::exchange(other.handle_, {})},
        consumed_{std::exchange(other.consumed_, false)} {}

  sync_task& operator=(sync_task&& other) noexcept {
    if (this != &other) {
      destroy();
      handle_ = std::exchange(other.handle_, {});
      consumed_ = std::exchange(other.consumed_, false);
    }
    return *this;
  }

  sync_task(const sync_task&) = delete;
  sync_task& operator=(const sync_task&) = delete;

  ~sync_task() { destroy(); }

  /// Returns true once the coroutine has run to completion (always true for an
  /// eager sync_task that was not moved-from).
  [[nodiscard]] bool done() const noexcept { return handle_ && handle_.done(); }

  /// Synchronously extract the coroutine result.  Rethrows any exception that
  /// escaped the coroutine body.  Must only be called once.
  [[nodiscard]] T get() {
    if (!handle_) {
      throw std::logic_error{"pb::coro::sync_task: no coroutine state"};
    }
    if (consumed_) {
      throw std::logic_error{"pb::coro::sync_task: result already consumed"};
    }
    consumed_ = true;

    promise_type& promise = handle_.promise();
    if (promise.error_) {
      std::rethrow_exception(promise.error_);
    }
    if (!promise.value_.has_value()) {
      throw std::logic_error{"pb::coro::sync_task: coroutine produced no value"};
    }
    return std::move(*promise.value_);
  }

 private:
  void destroy() noexcept {
    if (handle_) {
      handle_.destroy();
      handle_ = {};
    }
  }

  std::coroutine_handle<promise_type> handle_{};
  bool consumed_{false};
};

namespace detail {

template <class T>
struct is_sync_task : std::false_type {};

template <class T>
struct is_sync_task<sync_task<T>> : std::true_type {};

} // namespace detail

/// Concept satisfied by `pb::coro::sync_task<T>` specializations.
template <class T>
concept sync_task_type = detail::is_sync_task<std::remove_cvref_t<T>>::value;

} // namespace coro

namespace adapt_detail {

/// Trait extracting the awaited result type from a `pb::coro::sync_task<T>`.
template <class Task>
struct coroutine_result;

template <class T>
struct coroutine_result<coro::sync_task<T>> {
  using type = T;
};

template <class Task>
using coroutine_result_t = typename coroutine_result<std::remove_cvref_t<Task>>::type;

/// Models a callable that, given an Input, returns a `pb::coro::sync_task<T>`.
template <class Coro, class Input>
concept coroutine_callable =
    std::invocable<Coro&, Input> &&
    coro::sync_task_type<std::invoke_result_t<Coro&, Input>>;

} // namespace adapt_detail

// ===========================================================================
// pb::coroutine_stage<Coro>
//
// Adapts a coroutine-returning callable (a function object whose operator() or
// a free function captured in a functor returns pb::coro::sync_task<T>) into a
// valid pb stage that runs on the SEQUENTIAL engine.
//
//   - input_type  is deduced from the coroutine's single parameter (Input).
//   - output_type is the coroutine's awaited result type T (from sync_task<T>).
//   - operator()(input) invokes the coroutine (which runs eagerly to
//     completion) and synchronously extracts the result via .get().
//
// Coro must be default-constructible so the runtime engine can construct the
// stage under its default construct_stages_per_run policy.  The simplest way to
// satisfy this is to wrap a coroutine free function in a small stateless
// functor whose operator() forwards to it.
//
// This is a SYNCHRONOUS coroutine adapter: it does not introduce async
// scheduling.  True async / sender-based backends remain future work.
// ===========================================================================
template <class Coro, class Input>
struct coroutine_stage {
  static_assert(std::default_initializable<Coro>,
                "pb::coroutine_stage: Coro must be default-constructible so the runtime engine "
                "can construct the stage with its default policy. Wrap a coroutine free function "
                "in a stateless functor whose operator() returns pb::coro::sync_task<T>.");
  static_assert(adapt_detail::coroutine_callable<Coro, Input>,
                "pb::coroutine_stage: Coro must be invocable with Input and return a "
                "pb::coro::sync_task<T>.");

  using input_type = Input;
  using output_type = adapt_detail::coroutine_result_t<std::invoke_result_t<Coro&, Input>>;

  static constexpr std::string_view stage_name() noexcept { return "coroutine_stage"; }

  [[nodiscard]] output_type operator()(input_type input) const {
    Coro coro{};
    auto task = coro(std::move(input));
    return task.get();
  }
};

/// Convenience alias mirroring pb::adapt_fn / pb::adapt_functor naming.
///
/// \tparam Coro  A default-constructible functor returning pb::coro::sync_task<T>.
/// \tparam Input The stage input type (the coroutine's parameter type).
template <class Coro, class Input>
using adapt_coroutine = coroutine_stage<Coro, Input>;

} // namespace pb
