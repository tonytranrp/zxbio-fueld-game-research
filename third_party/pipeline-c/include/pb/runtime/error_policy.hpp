#pragma once

#include <chrono>
#include <cstdlib>
#include <exception>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "pb/runtime/error.hpp"
#include "pb/runtime/observer.hpp"
#include "pb/runtime/result.hpp"

/// @file
/// @brief Production-grade error policy engine wrappers.
///
/// The library's baseline engines (`pb::compile<Pipeline>(...)`) return
/// `pb::result<T>` for both `run()` and `try_run()`.  That is the right
/// default for most users — explicit success/failure values, no hidden
/// control flow, and zero coupling to exceptions.
///
/// Some applications want the opposite trade-off, though: they treat any
/// pipeline failure as a bug and want exceptions to interrupt control flow
/// at the call site instead of percolating a `result<T>` upward.  This
/// header provides a thin, opt-in wrapper that converts the result-based
/// API into a throwing API without modifying the underlying engine:
///
/// @code
///   auto engine = pb::compile<MyPipeline>(pb::runtime::sequential{});
///   auto throwing = pb::with_throw_on_error(std::move(engine));
///   auto value = throwing.run(input);  // throws pb::pipeline_exception on failure
/// @endcode
///
/// `try_run()` remains available on the wrapper and forwards to the
/// underlying engine, so users can opt into result-based handling on a
/// per-call basis even after wrapping.

namespace pb {

/// Exception type thrown by throwing engine wrappers when a stage fails.
/// Carries the runtime diagnostic so handlers can inspect stage identity,
/// category, and message without parsing the `what()` string.
class pipeline_exception : public std::runtime_error {
public:
  explicit pipeline_exception(runtime::error err)
      : std::runtime_error{format_message(err)}, diagnostic_{std::move(err)} {}

  /// Access the underlying runtime error record (stage id, category,
  /// message).  Stable for the lifetime of the exception.
  [[nodiscard]] auto diagnostic() const noexcept -> const runtime::error& {
    return diagnostic_;
  }

private:
  static auto format_message(const runtime::error& value) -> std::string {
    std::string out{"pb::pipeline_exception: "};
    if (runtime::has_stage(value)) {
      out += "[";
      out += value.stage.key.empty() ? value.stage.name : value.stage.key;
      out += "] ";
    }
    out += std::string{runtime::category_name(value.category)};
    if (runtime::has_message(value)) {
      out += " — ";
      out += value.message;
    }
    return out;
  }

  runtime::error diagnostic_{};
};

// `pb::policy::errors::expected` / `terminate` / `ignore` are declared in
// `pb/core/policy.hpp`.  This file pairs each of those tags with a real
// engine wrapper.  The `throw_on_error` tag does not have a tag entry in
// `policy.hpp` yet — it's introduced here because it's specific to the
// throwing engine wrapper API.
namespace policy::errors {

/// Tag selecting the throwing error policy.  Use with `pb::with_throw_on_error`.
struct throw_on_error {};

} // namespace policy::errors

/// Engine wrapper that converts result-returning failures into thrown
/// `pb::pipeline_exception`s on `run()`, while keeping a result-returning
/// `try_run()` for callers that still want the value-based API.
template <class Engine>
class throwing_engine {
public:
  using underlying_engine = Engine;
  using input_type   = typename Engine::input_type;
  using output_type  = typename Engine::output_type;
  using try_result_type = decltype(std::declval<Engine&>().try_run(std::declval<input_type>()));

  explicit throwing_engine(Engine engine) noexcept(std::is_nothrow_move_constructible_v<Engine>)
      : engine_{std::move(engine)} {}

  /// Run the pipeline.  Throws `pb::pipeline_exception` on stage failure,
  /// otherwise returns the value (or returns `void` for void-output
  /// pipelines).
  auto run(input_type input) -> output_type {
    // Route through try_run() because the underlying engine's `run()` is
    // a decltype(auto) helper that conditionally returns the raw value
    // type or a `result<T>`.  `try_run()` always returns `result<T>` and
    // is the production-stable entrypoint for error-policy wrappers.
    auto outcome = engine_.try_run(std::move(input));
    if (!outcome.has_value()) {
      throw pipeline_exception{std::move(outcome).error()};
    }
    if constexpr (std::is_void_v<output_type>) {
      return;
    } else {
      return std::move(outcome).value();
    }
  }

  /// Result-returning escape hatch for callers that want to inspect a
  /// failure without catching an exception.  Forwards directly to the
  /// underlying engine.
  auto try_run(input_type input) -> try_result_type {
    return engine_.try_run(std::move(input));
  }

  template <class Iterator, class Sentinel>
  [[nodiscard]] auto try_run_range(Iterator first, Sentinel last)
      noexcept(noexcept(engine_.try_run_range(first, last)))
    requires requires(Engine& engine, Iterator begin, Sentinel end) {
      engine.try_run_range(begin, end);
    }
  {
    return engine_.try_run_range(first, last);
  }

  template <class Range>
  [[nodiscard]] auto try_run_each(Range&& inputs)
      noexcept(noexcept(engine_.try_run_each(std::forward<Range>(inputs))))
    requires requires(Engine& engine, Range&& range) {
      engine.try_run_each(std::forward<Range>(range));
    }
  {
    return engine_.try_run_each(std::forward<Range>(inputs));
  }

  /// Observer and descriptor forwarding.  Lets the wrapper compose with
  /// other policy wrappers (e.g. `verbose_engine<throwing_engine<...>>`)
  /// and matches the underlying engine's public observer/introspection
  /// surface so callers don't have to reach through `underlying()`.
  void set_observer(runtime::observer* value) noexcept { engine_.set_observer(value); }
  [[nodiscard]] auto get_observer() const noexcept -> runtime::observer* { return engine_.get_observer(); }
  [[nodiscard]] auto describe() const { return engine_.describe(); }
  [[nodiscard]] auto descriptor() const noexcept { return engine_.descriptor(); }
  [[nodiscard]] auto worker_count() const noexcept(noexcept(engine_.worker_count()))
    requires requires(const Engine& engine) { engine.worker_count(); }
  {
    return engine_.worker_count();
  }
  [[nodiscard]] auto pending_tasks() const noexcept(noexcept(engine_.pending_tasks()))
    requires requires(const Engine& engine) { engine.pending_tasks(); }
  {
    return engine_.pending_tasks();
  }
  [[nodiscard]] auto queued_tasks() const noexcept(noexcept(engine_.queued_tasks()))
    requires requires(const Engine& engine) { engine.queued_tasks(); }
  {
    return engine_.queued_tasks();
  }
  [[nodiscard]] auto active_tasks() const noexcept(noexcept(engine_.active_tasks()))
    requires requires(const Engine& engine) { engine.active_tasks(); }
  {
    return engine_.active_tasks();
  }
  [[nodiscard]] auto snapshot() const noexcept(noexcept(engine_.snapshot()))
    requires requires(const Engine& engine) { engine.snapshot(); }
  {
    return engine_.snapshot();
  }
  [[nodiscard]] auto shared_pool() const noexcept(noexcept(engine_.shared_pool()))
    requires requires(const Engine& engine) { engine.shared_pool(); }
  {
    return engine_.shared_pool();
  }
  void wait_idle() noexcept(noexcept(engine_.wait_idle()))
    requires requires(Engine& engine) { engine.wait_idle(); }
  {
    engine_.wait_idle();
  }
  template <class Rep, class Period>
  [[nodiscard]] auto wait_idle_for(const std::chrono::duration<Rep, Period>& timeout)
      noexcept(noexcept(engine_.wait_idle_for(timeout)))
    requires requires(Engine& engine, const std::chrono::duration<Rep, Period>& value) {
      engine.wait_idle_for(value);
    }
  {
    return engine_.wait_idle_for(timeout);
  }
  template <class Clock, class Duration>
  [[nodiscard]] auto wait_idle_until(const std::chrono::time_point<Clock, Duration>& deadline)
      noexcept(noexcept(engine_.wait_idle_until(deadline)))
    requires requires(Engine& engine, const std::chrono::time_point<Clock, Duration>& value) {
      engine.wait_idle_until(value);
    }
  {
    return engine_.wait_idle_until(deadline);
  }

  /// Mutable access to the wrapped engine for callers that need
  /// backend-specific state not covered by the forwarding surface above.
  [[nodiscard]] auto underlying() & noexcept -> Engine& { return engine_; }
  [[nodiscard]] auto underlying() const& noexcept -> const Engine& { return engine_; }
  [[nodiscard]] auto underlying() && noexcept -> Engine&& { return std::move(engine_); }

private:
  Engine engine_;
};

/// Factory that wraps any engine into a `throwing_engine`.
/// `pb::with_throw_on_error(pb::compile<P>(pb::runtime::sequential{}))` is
/// the canonical opt-in syntax for throwing semantics.
template <class Engine>
[[nodiscard]] auto with_throw_on_error(Engine engine)
    -> throwing_engine<Engine> {
  return throwing_engine<Engine>{std::move(engine)};
}

// ---------------------------------------------------------------------------
// Terminate-on-error policy
// ---------------------------------------------------------------------------

// The `pb::policy::errors::terminate` and `pb::policy::errors::ignore`
// tags are declared in `pb/core/policy.hpp` alongside the other policy
// markers.  The wrappers below pair with those tags.

/// Engine wrapper that calls `std::terminate()` on any stage failure.
/// `run()` returns the raw output value (never `result<>`); `try_run()`
/// still returns `result<>` for callers that want to bypass the terminate
/// policy on a per-call basis.
template <class Engine>
class terminating_engine {
public:
  using underlying_engine = Engine;
  using input_type   = typename Engine::input_type;
  using output_type  = typename Engine::output_type;
  using try_result_type = decltype(std::declval<Engine&>().try_run(std::declval<input_type>()));

  explicit terminating_engine(Engine engine) noexcept(std::is_nothrow_move_constructible_v<Engine>)
      : engine_{std::move(engine)} {}

  /// Run the pipeline. Calls `std::terminate()` on stage failure so the
  /// caller never observes an error value.
  auto run(input_type input) -> output_type {
    auto outcome = engine_.try_run(std::move(input));
    if (!outcome.has_value()) {
      // Fully qualified to bypass any namespace shadowing from
      // `pb::policy::errors::terminate`.
      ::std::terminate();
    }
    if constexpr (std::is_void_v<output_type>) {
      return;
    } else {
      return std::move(outcome).value();
    }
  }

  auto try_run(input_type input) -> try_result_type {
    return engine_.try_run(std::move(input));
  }

  template <class Iterator, class Sentinel>
  [[nodiscard]] auto try_run_range(Iterator first, Sentinel last)
      noexcept(noexcept(engine_.try_run_range(first, last)))
    requires requires(Engine& engine, Iterator begin, Sentinel end) {
      engine.try_run_range(begin, end);
    }
  {
    return engine_.try_run_range(first, last);
  }

  template <class Range>
  [[nodiscard]] auto try_run_each(Range&& inputs)
      noexcept(noexcept(engine_.try_run_each(std::forward<Range>(inputs))))
    requires requires(Engine& engine, Range&& range) {
      engine.try_run_each(std::forward<Range>(range));
    }
  {
    return engine_.try_run_each(std::forward<Range>(inputs));
  }

  void set_observer(runtime::observer* value) noexcept { engine_.set_observer(value); }
  [[nodiscard]] auto get_observer() const noexcept -> runtime::observer* { return engine_.get_observer(); }
  [[nodiscard]] auto describe() const { return engine_.describe(); }
  [[nodiscard]] auto descriptor() const noexcept { return engine_.descriptor(); }
  [[nodiscard]] auto worker_count() const noexcept(noexcept(engine_.worker_count()))
    requires requires(const Engine& engine) { engine.worker_count(); }
  {
    return engine_.worker_count();
  }
  [[nodiscard]] auto pending_tasks() const noexcept(noexcept(engine_.pending_tasks()))
    requires requires(const Engine& engine) { engine.pending_tasks(); }
  {
    return engine_.pending_tasks();
  }
  [[nodiscard]] auto queued_tasks() const noexcept(noexcept(engine_.queued_tasks()))
    requires requires(const Engine& engine) { engine.queued_tasks(); }
  {
    return engine_.queued_tasks();
  }
  [[nodiscard]] auto active_tasks() const noexcept(noexcept(engine_.active_tasks()))
    requires requires(const Engine& engine) { engine.active_tasks(); }
  {
    return engine_.active_tasks();
  }
  [[nodiscard]] auto snapshot() const noexcept(noexcept(engine_.snapshot()))
    requires requires(const Engine& engine) { engine.snapshot(); }
  {
    return engine_.snapshot();
  }
  [[nodiscard]] auto shared_pool() const noexcept(noexcept(engine_.shared_pool()))
    requires requires(const Engine& engine) { engine.shared_pool(); }
  {
    return engine_.shared_pool();
  }
  void wait_idle() noexcept(noexcept(engine_.wait_idle()))
    requires requires(Engine& engine) { engine.wait_idle(); }
  {
    engine_.wait_idle();
  }
  template <class Rep, class Period>
  [[nodiscard]] auto wait_idle_for(const std::chrono::duration<Rep, Period>& timeout)
      noexcept(noexcept(engine_.wait_idle_for(timeout)))
    requires requires(Engine& engine, const std::chrono::duration<Rep, Period>& value) {
      engine.wait_idle_for(value);
    }
  {
    return engine_.wait_idle_for(timeout);
  }
  template <class Clock, class Duration>
  [[nodiscard]] auto wait_idle_until(const std::chrono::time_point<Clock, Duration>& deadline)
      noexcept(noexcept(engine_.wait_idle_until(deadline)))
    requires requires(Engine& engine, const std::chrono::time_point<Clock, Duration>& value) {
      engine.wait_idle_until(value);
    }
  {
    return engine_.wait_idle_until(deadline);
  }

  [[nodiscard]] auto underlying() & noexcept -> Engine& { return engine_; }
  [[nodiscard]] auto underlying() const& noexcept -> const Engine& { return engine_; }
  [[nodiscard]] auto underlying() && noexcept -> Engine&& { return std::move(engine_); }

private:
  Engine engine_;
};

/// Factory producing a `terminating_engine`.  Suitable for pipelines where
/// any failure is by definition a programming bug — the wrapper short-
/// circuits to `std::terminate()` rather than propagating an error.
template <class Engine>
[[nodiscard]] auto with_terminate_on_error(Engine engine)
    -> terminating_engine<Engine> {
  return terminating_engine<Engine>{std::move(engine)};
}

// ---------------------------------------------------------------------------
// Ignore-errors policy (fallback value)
// ---------------------------------------------------------------------------

/// Engine wrapper that substitutes a user-supplied fallback value when the
/// underlying pipeline fails.  The wrapper *never* throws and never returns
/// a `result<>` from `run()` — it's the right policy for best-effort or
/// fire-and-forget workloads where the caller wants a value unconditionally.
///
/// `try_run()` still returns `result<>` so callers can inspect a failure
/// when they actually want to know one happened.
///
/// Not available for void-output pipelines (a fallback value is meaningless
/// for `void`).  Compile-time `static_assert` enforces this.
template <class Engine>
class ignoring_engine {
public:
  using underlying_engine = Engine;
  using input_type   = typename Engine::input_type;
  using output_type  = typename Engine::output_type;
  using try_result_type = decltype(std::declval<Engine&>().try_run(std::declval<input_type>()));

  static_assert(!std::is_void_v<output_type>,
                "pb::with_ignore_errors requires a non-void output type — a fallback value "
                "is meaningless for void-returning pipelines");

  ignoring_engine(Engine engine, output_type fallback)
      : engine_{std::move(engine)}, fallback_{std::move(fallback)} {}

  /// Run the pipeline.  Returns the underlying value on success, or a copy
  /// of the user-supplied fallback on stage failure / caught exception.
  auto run(input_type input) -> output_type {
    auto outcome = engine_.try_run(std::move(input));
    if (!outcome.has_value()) {
      return fallback_;
    }
    return std::move(outcome).value();
  }

  auto try_run(input_type input) -> try_result_type {
    return engine_.try_run(std::move(input));
  }

  template <class Iterator, class Sentinel>
  [[nodiscard]] auto try_run_range(Iterator first, Sentinel last)
      noexcept(noexcept(engine_.try_run_range(first, last)))
    requires requires(Engine& engine, Iterator begin, Sentinel end) {
      engine.try_run_range(begin, end);
    }
  {
    return engine_.try_run_range(first, last);
  }

  template <class Range>
  [[nodiscard]] auto try_run_each(Range&& inputs)
      noexcept(noexcept(engine_.try_run_each(std::forward<Range>(inputs))))
    requires requires(Engine& engine, Range&& range) {
      engine.try_run_each(std::forward<Range>(range));
    }
  {
    return engine_.try_run_each(std::forward<Range>(inputs));
  }

  /// Replace the fallback value at runtime.  Useful when the fallback
  /// depends on call-site context (e.g. a default that changes per
  /// request).
  void set_fallback(output_type value) noexcept(std::is_nothrow_move_assignable_v<output_type>) {
    fallback_ = std::move(value);
  }

  [[nodiscard]] auto get_fallback() const& noexcept -> const output_type& { return fallback_; }

  void set_observer(runtime::observer* value) noexcept { engine_.set_observer(value); }
  [[nodiscard]] auto get_observer() const noexcept -> runtime::observer* { return engine_.get_observer(); }
  [[nodiscard]] auto describe() const { return engine_.describe(); }
  [[nodiscard]] auto descriptor() const noexcept { return engine_.descriptor(); }
  [[nodiscard]] auto worker_count() const noexcept(noexcept(engine_.worker_count()))
    requires requires(const Engine& engine) { engine.worker_count(); }
  {
    return engine_.worker_count();
  }
  [[nodiscard]] auto pending_tasks() const noexcept(noexcept(engine_.pending_tasks()))
    requires requires(const Engine& engine) { engine.pending_tasks(); }
  {
    return engine_.pending_tasks();
  }
  [[nodiscard]] auto queued_tasks() const noexcept(noexcept(engine_.queued_tasks()))
    requires requires(const Engine& engine) { engine.queued_tasks(); }
  {
    return engine_.queued_tasks();
  }
  [[nodiscard]] auto active_tasks() const noexcept(noexcept(engine_.active_tasks()))
    requires requires(const Engine& engine) { engine.active_tasks(); }
  {
    return engine_.active_tasks();
  }
  [[nodiscard]] auto snapshot() const noexcept(noexcept(engine_.snapshot()))
    requires requires(const Engine& engine) { engine.snapshot(); }
  {
    return engine_.snapshot();
  }
  [[nodiscard]] auto shared_pool() const noexcept(noexcept(engine_.shared_pool()))
    requires requires(const Engine& engine) { engine.shared_pool(); }
  {
    return engine_.shared_pool();
  }
  void wait_idle() noexcept(noexcept(engine_.wait_idle()))
    requires requires(Engine& engine) { engine.wait_idle(); }
  {
    engine_.wait_idle();
  }
  template <class Rep, class Period>
  [[nodiscard]] auto wait_idle_for(const std::chrono::duration<Rep, Period>& timeout)
      noexcept(noexcept(engine_.wait_idle_for(timeout)))
    requires requires(Engine& engine, const std::chrono::duration<Rep, Period>& value) {
      engine.wait_idle_for(value);
    }
  {
    return engine_.wait_idle_for(timeout);
  }
  template <class Clock, class Duration>
  [[nodiscard]] auto wait_idle_until(const std::chrono::time_point<Clock, Duration>& deadline)
      noexcept(noexcept(engine_.wait_idle_until(deadline)))
    requires requires(Engine& engine, const std::chrono::time_point<Clock, Duration>& value) {
      engine.wait_idle_until(value);
    }
  {
    return engine_.wait_idle_until(deadline);
  }

  [[nodiscard]] auto underlying() & noexcept -> Engine& { return engine_; }
  [[nodiscard]] auto underlying() const& noexcept -> const Engine& { return engine_; }
  [[nodiscard]] auto underlying() && noexcept -> Engine&& { return std::move(engine_); }

private:
  Engine engine_;
  output_type fallback_;
};

/// Factory producing an `ignoring_engine` with the supplied fallback.
template <class Engine, class Fallback>
[[nodiscard]] auto with_ignore_errors(Engine engine, Fallback&& fallback)
    -> ignoring_engine<Engine> {
  return ignoring_engine<Engine>{std::move(engine),
                                 typename Engine::output_type{std::forward<Fallback>(fallback)}};
}

// ---------------------------------------------------------------------------
// Propagate-exceptions policy
// ---------------------------------------------------------------------------

// `pb::policy::exceptions::propagate` lives in `pb/core/policy.hpp`.

/// Engine wrapper that converts stage *exceptions* into thrown
/// `pb::pipeline_exception`s, while leaving non-exception stage failures
/// as the original `result<>` value.  The opposite of the default — the
/// default behaviour catches exceptions into `result<>`; this policy
/// surfaces them back to the caller while still preserving the
/// result-based path for declared stage failures.
template <class Engine>
class propagating_engine {
public:
  using underlying_engine = Engine;
  using input_type   = typename Engine::input_type;
  using output_type  = typename Engine::output_type;
  using try_result_type = decltype(std::declval<Engine&>().try_run(std::declval<input_type>()));

  explicit propagating_engine(Engine engine) noexcept(std::is_nothrow_move_constructible_v<Engine>)
      : engine_{std::move(engine)} {}

  /// Run the pipeline.  Returns the underlying `result<>` on a declared
  /// stage failure; rethrows `pb::pipeline_exception` when the underlying
  /// engine caught a thrown exception inside a stage.
  auto run(input_type input) -> try_result_type {
    auto outcome = engine_.try_run(std::move(input));
    if (!outcome.has_value() && outcome.error().category == runtime::error_category::exception) {
      throw pipeline_exception{std::move(outcome).error()};
    }
    return outcome;
  }

  auto try_run(input_type input) -> try_result_type {
    return engine_.try_run(std::move(input));
  }

  template <class Iterator, class Sentinel>
  [[nodiscard]] auto try_run_range(Iterator first, Sentinel last)
      noexcept(noexcept(engine_.try_run_range(first, last)))
    requires requires(Engine& engine, Iterator begin, Sentinel end) {
      engine.try_run_range(begin, end);
    }
  {
    return engine_.try_run_range(first, last);
  }

  template <class Range>
  [[nodiscard]] auto try_run_each(Range&& inputs)
      noexcept(noexcept(engine_.try_run_each(std::forward<Range>(inputs))))
    requires requires(Engine& engine, Range&& range) {
      engine.try_run_each(std::forward<Range>(range));
    }
  {
    return engine_.try_run_each(std::forward<Range>(inputs));
  }

  void set_observer(runtime::observer* value) noexcept { engine_.set_observer(value); }
  [[nodiscard]] auto get_observer() const noexcept -> runtime::observer* { return engine_.get_observer(); }
  [[nodiscard]] auto describe() const { return engine_.describe(); }
  [[nodiscard]] auto descriptor() const noexcept { return engine_.descriptor(); }
  [[nodiscard]] auto worker_count() const noexcept(noexcept(engine_.worker_count()))
    requires requires(const Engine& engine) { engine.worker_count(); }
  {
    return engine_.worker_count();
  }
  [[nodiscard]] auto pending_tasks() const noexcept(noexcept(engine_.pending_tasks()))
    requires requires(const Engine& engine) { engine.pending_tasks(); }
  {
    return engine_.pending_tasks();
  }
  [[nodiscard]] auto queued_tasks() const noexcept(noexcept(engine_.queued_tasks()))
    requires requires(const Engine& engine) { engine.queued_tasks(); }
  {
    return engine_.queued_tasks();
  }
  [[nodiscard]] auto active_tasks() const noexcept(noexcept(engine_.active_tasks()))
    requires requires(const Engine& engine) { engine.active_tasks(); }
  {
    return engine_.active_tasks();
  }
  [[nodiscard]] auto snapshot() const noexcept(noexcept(engine_.snapshot()))
    requires requires(const Engine& engine) { engine.snapshot(); }
  {
    return engine_.snapshot();
  }
  [[nodiscard]] auto shared_pool() const noexcept(noexcept(engine_.shared_pool()))
    requires requires(const Engine& engine) { engine.shared_pool(); }
  {
    return engine_.shared_pool();
  }
  void wait_idle() noexcept(noexcept(engine_.wait_idle()))
    requires requires(Engine& engine) { engine.wait_idle(); }
  {
    engine_.wait_idle();
  }
  template <class Rep, class Period>
  [[nodiscard]] auto wait_idle_for(const std::chrono::duration<Rep, Period>& timeout)
      noexcept(noexcept(engine_.wait_idle_for(timeout)))
    requires requires(Engine& engine, const std::chrono::duration<Rep, Period>& value) {
      engine.wait_idle_for(value);
    }
  {
    return engine_.wait_idle_for(timeout);
  }
  template <class Clock, class Duration>
  [[nodiscard]] auto wait_idle_until(const std::chrono::time_point<Clock, Duration>& deadline)
      noexcept(noexcept(engine_.wait_idle_until(deadline)))
    requires requires(Engine& engine, const std::chrono::time_point<Clock, Duration>& value) {
      engine.wait_idle_until(value);
    }
  {
    return engine_.wait_idle_until(deadline);
  }

  [[nodiscard]] auto underlying() & noexcept -> Engine& { return engine_; }
  [[nodiscard]] auto underlying() const& noexcept -> const Engine& { return engine_; }
  [[nodiscard]] auto underlying() && noexcept -> Engine&& { return std::move(engine_); }

private:
  Engine engine_;
};

/// Factory producing a `propagating_engine`.
template <class Engine>
[[nodiscard]] auto with_propagate_exceptions(Engine engine)
    -> propagating_engine<Engine> {
  return propagating_engine<Engine>{std::move(engine)};
}

// ---------------------------------------------------------------------------
// Verbose-diagnostics policy
// ---------------------------------------------------------------------------

// `pb::policy::diagnostics::verbose` lives in `pb/core/policy.hpp`.

/// `pb::runtime::observer` implementation that logs every stage transition
/// to a `std::ostream`.  Owned by `pb::verbose_engine` so callers don't
/// have to wire it manually.  The schema of the logged lines is stable:
/// each line begins with the prefix `[pb.verbose]` and the event kind so
/// downstream tooling can grep for events without parsing free-form text.
class verbose_observer final : public runtime::observer {
public:
  verbose_observer() = default;
  explicit verbose_observer(std::ostream* sink) noexcept : sink_{sink} {}

  void set_sink(std::ostream* sink) noexcept { sink_ = sink; }
  [[nodiscard]] auto sink() const noexcept -> std::ostream* { return sink_; }

  void on_stage_start(const runtime::stage_id& id) override {
    write("stage_start", id);
  }
  void on_stage_success(const runtime::stage_id& id) override {
    write("stage_success", id);
  }
  void on_stage_failure(const runtime::stage_id& id, const runtime::error& diag) override {
    write_with_error("stage_failure", id, diag);
  }
  void on_stage_exception(const runtime::stage_id& id, const runtime::error& diag) override {
    write_with_error("stage_exception", id, diag);
  }
  void on_case_selected(const runtime::stage_id& branch_id, std::size_t case_index,
                        const runtime::stage_id& predicate_id) override {
    write_case("case_selected", branch_id, case_index, predicate_id);
  }
  void on_case_skipped(const runtime::stage_id& branch_id, std::size_t case_index,
                       const runtime::stage_id& predicate_id) override {
    write_case("case_skipped", branch_id, case_index, predicate_id);
  }
  void on_case_failed(const runtime::stage_id& branch_id, std::size_t case_index,
                      const runtime::stage_id& case_stage_id, const runtime::error& diag) override {
    if (!sink_) return;
    *sink_ << "[pb.verbose] case_failed branch=" << format_stage(branch_id) << " case=" << case_index
           << " stage=" << format_stage(case_stage_id) << " category="
           << runtime::category_name(diag.category);
    if (runtime::has_message(diag)) {
      *sink_ << " message=" << diag.message;
    }
    *sink_ << '\n';
  }
  void on_fan_in_started(const runtime::stage_id& branch_id, std::size_t case_count) override {
    if (!sink_) return;
    *sink_ << "[pb.verbose] fan_in_started branch=" << format_stage(branch_id)
           << " cases=" << case_count << '\n';
  }
  void on_fan_in_case_scheduled(const runtime::stage_id& branch_id, std::size_t case_index,
                                const runtime::stage_id& case_stage_id) override {
    if (!sink_) return;
    *sink_ << "[pb.verbose] fan_in_case_scheduled branch=" << format_stage(branch_id)
           << " case=" << case_index << " stage=" << format_stage(case_stage_id) << '\n';
  }
  void on_fan_in_case_completed(const runtime::stage_id& branch_id, std::size_t case_index,
                                const runtime::stage_id& case_stage_id, bool success) override {
    if (!sink_) return;
    *sink_ << "[pb.verbose] fan_in_case_completed branch=" << format_stage(branch_id)
           << " case=" << case_index << " stage=" << format_stage(case_stage_id)
           << " success=" << (success ? "true" : "false") << '\n';
  }
  void on_fan_in_completed(const runtime::stage_id& branch_id, std::size_t selected_count,
                           std::size_t completed_count, std::size_t failed_count) override {
    if (!sink_) return;
    *sink_ << "[pb.verbose] fan_in_completed branch=" << format_stage(branch_id)
           << " selected=" << selected_count << " completed=" << completed_count
           << " failed=" << failed_count << '\n';
  }

private:
  static auto format_stage(const runtime::stage_id& id) -> std::string {
    if (!id.key.empty()) return id.key;
    return id.name;
  }

  void write(const char* event, const runtime::stage_id& id) {
    if (!sink_) return;
    *sink_ << "[pb.verbose] " << event << " stage=" << format_stage(id) << '\n';
  }

  void write_with_error(const char* event, const runtime::stage_id& id, const runtime::error& diag) {
    if (!sink_) return;
    *sink_ << "[pb.verbose] " << event << " stage=" << format_stage(id) << " category="
           << runtime::category_name(diag.category);
    if (runtime::has_message(diag)) {
      *sink_ << " message=" << diag.message;
    }
    *sink_ << '\n';
  }

  void write_case(const char* event, const runtime::stage_id& branch_id, std::size_t case_index,
                  const runtime::stage_id& predicate_id) {
    if (!sink_) return;
    *sink_ << "[pb.verbose] " << event << " branch=" << format_stage(branch_id) << " case="
           << case_index << " predicate=" << format_stage(predicate_id) << '\n';
  }

  std::ostream* sink_{};
};

/// Engine wrapper that owns a `verbose_observer` and attaches it to the
/// underlying engine for the wrapper's lifetime.  Any previously-attached
/// observer is restored when the verbose wrapper is destroyed.  This is the
/// production-grade equivalent of "set verbose=ON" without polluting user
/// code with manual observer wiring.
template <class Engine>
class verbose_engine {
public:
  using underlying_engine = Engine;
  using input_type   = typename Engine::input_type;
  using output_type  = typename Engine::output_type;
  using try_result_type = decltype(std::declval<Engine&>().try_run(std::declval<input_type>()));

  verbose_engine(Engine engine, std::ostream* sink)
      : engine_{std::move(engine)}, observer_{sink} {
    previous_observer_ = engine_.get_observer();
    engine_.set_observer(&observer_);
  }

  ~verbose_engine() noexcept {
    engine_.set_observer(previous_observer_);
  }

  verbose_engine(const verbose_engine&) = delete;
  verbose_engine& operator=(const verbose_engine&) = delete;
  verbose_engine(verbose_engine&&) = delete;
  verbose_engine& operator=(verbose_engine&&) = delete;

  /// Run the pipeline.  Forwards directly to the underlying engine; the
  /// owned observer logs every transition to the sink.
  decltype(auto) run(input_type input) {
    return engine_.run(std::move(input));
  }

  auto try_run(input_type input) -> try_result_type {
    return engine_.try_run(std::move(input));
  }

  template <class Iterator, class Sentinel>
  [[nodiscard]] auto try_run_range(Iterator first, Sentinel last)
      noexcept(noexcept(engine_.try_run_range(first, last)))
    requires requires(Engine& engine, Iterator begin, Sentinel end) {
      engine.try_run_range(begin, end);
    }
  {
    return engine_.try_run_range(first, last);
  }

  template <class Range>
  [[nodiscard]] auto try_run_each(Range&& inputs)
      noexcept(noexcept(engine_.try_run_each(std::forward<Range>(inputs))))
    requires requires(Engine& engine, Range&& range) {
      engine.try_run_each(std::forward<Range>(range));
    }
  {
    return engine_.try_run_each(std::forward<Range>(inputs));
  }

  /// Re-target the log sink at runtime.
  void set_sink(std::ostream* sink) noexcept { observer_.set_sink(sink); }

  [[nodiscard]] auto sink() const noexcept -> std::ostream* { return observer_.sink(); }

  /// Descriptor forwarding for ergonomic access through wrapper stacks.
  /// `set_observer` / `get_observer` are intentionally *not* exposed —
  /// the verbose wrapper owns its observer slot and exposing those would
  /// let callers detach the verbose observer mid-run, defeating the
  /// wrapper's purpose.  Use `set_sink(nullptr)` to silence the wrapper
  /// instead.
  [[nodiscard]] auto describe() const { return engine_.describe(); }
  [[nodiscard]] auto descriptor() const noexcept { return engine_.descriptor(); }
  [[nodiscard]] auto worker_count() const noexcept(noexcept(engine_.worker_count()))
    requires requires(const Engine& engine) { engine.worker_count(); }
  {
    return engine_.worker_count();
  }
  [[nodiscard]] auto pending_tasks() const noexcept(noexcept(engine_.pending_tasks()))
    requires requires(const Engine& engine) { engine.pending_tasks(); }
  {
    return engine_.pending_tasks();
  }
  [[nodiscard]] auto queued_tasks() const noexcept(noexcept(engine_.queued_tasks()))
    requires requires(const Engine& engine) { engine.queued_tasks(); }
  {
    return engine_.queued_tasks();
  }
  [[nodiscard]] auto active_tasks() const noexcept(noexcept(engine_.active_tasks()))
    requires requires(const Engine& engine) { engine.active_tasks(); }
  {
    return engine_.active_tasks();
  }
  [[nodiscard]] auto snapshot() const noexcept(noexcept(engine_.snapshot()))
    requires requires(const Engine& engine) { engine.snapshot(); }
  {
    return engine_.snapshot();
  }
  [[nodiscard]] auto shared_pool() const noexcept(noexcept(engine_.shared_pool()))
    requires requires(const Engine& engine) { engine.shared_pool(); }
  {
    return engine_.shared_pool();
  }
  void wait_idle() noexcept(noexcept(engine_.wait_idle()))
    requires requires(Engine& engine) { engine.wait_idle(); }
  {
    engine_.wait_idle();
  }
  template <class Rep, class Period>
  [[nodiscard]] auto wait_idle_for(const std::chrono::duration<Rep, Period>& timeout)
      noexcept(noexcept(engine_.wait_idle_for(timeout)))
    requires requires(Engine& engine, const std::chrono::duration<Rep, Period>& value) {
      engine.wait_idle_for(value);
    }
  {
    return engine_.wait_idle_for(timeout);
  }
  template <class Clock, class Duration>
  [[nodiscard]] auto wait_idle_until(const std::chrono::time_point<Clock, Duration>& deadline)
      noexcept(noexcept(engine_.wait_idle_until(deadline)))
    requires requires(Engine& engine, const std::chrono::time_point<Clock, Duration>& value) {
      engine.wait_idle_until(value);
    }
  {
    return engine_.wait_idle_until(deadline);
  }

  [[nodiscard]] auto underlying() & noexcept -> Engine& { return engine_; }
  [[nodiscard]] auto underlying() const& noexcept -> const Engine& { return engine_; }

private:
  Engine engine_;
  verbose_observer observer_;
  runtime::observer* previous_observer_{};
};

/// Factory producing a `verbose_engine`.  Pass a pointer to the desired
/// log sink (e.g. `&std::clog`).  Passing `nullptr` makes the verbose
/// engine a no-op observer — useful for benchmarking the cost of the
/// observer indirection itself.
template <class Engine>
[[nodiscard]] auto with_verbose_diagnostics(Engine&& engine, std::ostream* sink) {
  using engine_type = std::remove_cvref_t<Engine>;
  return verbose_engine<engine_type>{std::forward<Engine>(engine), sink};
}

} // namespace pb
