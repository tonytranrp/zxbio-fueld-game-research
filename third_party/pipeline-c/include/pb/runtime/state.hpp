#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pb/runtime/error.hpp"
#include "pb/runtime/error_policy.hpp"
#include "pb/runtime/observer.hpp"
#include "pb/runtime/result.hpp"

/// @file
/// @brief Stateful storage DSL — orthogonal lifetime layer for pipeline state.
///
/// Pipelines built with `pb::compile<...>(pb::sequential{})` are pure
/// function compositions: every stage receives an `Input` and returns an
/// `Output`, with no read/write access to shared state.  Real-world
/// pipelines, however, almost always need *some* shared state across
/// stages: a cache, an accumulator, a resource pool, a configuration
/// snapshot, a request-scoped log buffer.  Writing those as captured
/// references on every stage type is invasive and breaks the value-type
/// stage model.
///
/// This header provides the **stateful storage DSL** — an orthogonal
/// layer that wraps any engine (sequential, thread-pool, even an
/// error-policy stack) and threads a typed state through every stage
/// invocation using a scoped thread-local context.
///
/// @section state-surface User-facing surface
///
/// Stages access state via `pb::current_state<S>()` inside their
/// `operator()`:
///
/// ```cpp
/// struct cache_state {
///   std::unordered_map<int, std::string> cache;
/// };
///
/// struct lookup_or_fetch {
///   using input_type = int;
///   using output_type = std::string;
///   static constexpr auto stage_key = "lookup_or_fetch";
///
///   auto operator()(int key) const -> std::string {
///     auto& s = pb::current_state<cache_state>();
///     if (auto it = s.cache.find(key); it != s.cache.end()) return it->second;
///     auto fetched = fetch(key);
///     s.cache[key] = fetched;
///     return fetched;
///   }
/// };
///
/// auto base    = pb::compile<MyPipeline>(pb::sequential{});
/// auto stateful = pb::with_state<cache_state>(std::move(base));
/// stateful.run(42);          // miss
/// stateful.run(42);          // hit (state persisted across runs)
/// ```
///
/// @section state-storage Storage policies
///
/// Five policies cover the lifetime patterns users actually need:
///
/// - `pb::policy::state::owned` (default) — engine owns the state,
///   persisted across every `run()` until the engine is destroyed.
///   Reset explicitly via `reset_state()`.
/// - `pb::policy::state::borrowed` — caller supplies `state&` per call
///   via `run_with_state(input, state)`.  Useful when the state's
///   lifetime is governed by the call site (e.g. request scope).
/// - `pb::policy::state::shared` — engine holds a `std::shared_ptr<S>`
///   so multiple engine instances (or threads) can share the same
///   state.  The caller is responsible for any synchronization needed
///   inside the state type.
/// - `pb::policy::state::reset_per_run` — owned, but reset to a
///   default-constructed value before every `run()`.  Useful for
///   per-run scratch state that must not leak between calls.
/// - `pb::policy::state::thread_local_owned` — owned, but the storage is
///   a per-thread instance.  When the same wrapped engine is invoked
///   concurrently from multiple threads, each thread gets its own
///   isolated `State`.  This is the storage policy required before
///   parallel backends can safely carry stateful stages.
///
/// @section state-thread-safety Thread safety
///
/// The thread-local stack means concurrent calls on different threads
/// do not interfere — each thread maintains its own active-state stack.
/// Nested wrappers (e.g. `with_state<A>(with_state<B>(engine))`) work
/// because each state type has an independent stack.  Reentrant calls
/// from inside a stage are also safe — they push a new frame.
///
/// @section state-schema Schema version
///
/// The wrapper's externally-observable contract (factory names,
/// `run_with_state` signature, `current_state<S>()` semantics, observer
/// surface) is locked under the schema constant `pb.state.v1` — see
/// `docs/state-dsl.md` for the formal stability promise.
namespace pb {

/// Stable schema-version identifier for the stateful storage DSL.
/// Bumped only when the externally-observable contract changes in a
/// way that breaks downstream consumers (factory names, run signatures,
/// or observer surface).  The internal implementation may evolve
/// freely within `pb.state.v1`.
inline constexpr std::string_view state_dsl_schema_version = "pb.state.v1";

namespace policy::state {

/// Default policy — the engine owns the state for its lifetime.
/// State persists across every `run()` call.  Reset explicitly via
/// `reset_state()` or replace via `set_state()`.
struct owned {};

/// Caller supplies `state&` per call via `run_with_state(input, state)`.
/// The wrapper does not own any state.  Useful when the state's
/// lifetime is governed by the call site (e.g. per-request state).
struct borrowed {};

/// Engine holds a `std::shared_ptr<State>` so the same state can be
/// shared across multiple engine instances or threads.  Synchronization
/// inside the state type is the caller's responsibility.
struct shared {};

/// Engine owns the state, but resets it to a default-constructed value
/// before every `run()`.  Useful for per-run scratch state that must
/// not leak between calls.
struct reset_per_run {};

/// Engine owns the state, but the storage is a per-**thread** instance
/// (`thread_local`).  When the same wrapped engine is invoked
/// concurrently from multiple threads, each thread sees its own
/// isolated `State` rather than a single shared one.  This is the
/// storage policy required before parallel backends can safely carry
/// stateful stages — each worker thread carries an independent state.
/// Synchronization is therefore unnecessary because no two threads ever
/// touch the same `State` object.
struct thread_local_owned {};

} // namespace policy::state

namespace runtime::detail {

/// Thread-local active-state stack for type `State`.  Each thread holds
/// its own stack; nested `state_context<State>` frames push and pop the
/// active pointer in LIFO order.  `current_state<State>()` returns the
/// top of the stack.
template <class State>
class state_stack {
public:
  state_stack() = delete;

  static auto storage() noexcept -> std::vector<State*>& {
    thread_local std::vector<State*> stack;
    return stack;
  }

  [[nodiscard]] static auto top() noexcept -> State* {
    auto& s = storage();
    return s.empty() ? nullptr : s.back();
  }

  static void push(State* value) { storage().push_back(value); }

  static void pop(State* value) noexcept {
    auto& s = storage();
    if (s.empty()) {
      return;
    }
    if (s.back() == value) {
      s.pop_back();
      return;
    }
    const auto match = std::find(s.rbegin(), s.rend(), value);
    if (match != s.rend()) {
      s.erase(std::next(match).base());
    }
  }
};

} // namespace runtime::detail

/// RAII frame that makes `state` the active state for type `State` for
/// the lifetime of the frame on the current thread.  Nested frames
/// stack — the innermost frame wins for `current_state<State>()`.
/// Move/copy are deleted because frame lifetime must be lexically
/// scoped to its constructor / destructor pair.
template <class State>
class state_context {
public:
  explicit state_context(State& value) noexcept : value_{&value}, valid_{true} {
    runtime::detail::state_stack<State>::push(value_);
  }

  state_context() = delete;
  state_context(const state_context&) = delete;
  state_context& operator=(const state_context&) = delete;
  state_context(state_context&&) = delete;
  state_context& operator=(state_context&&) = delete;

  ~state_context() noexcept {
    if (valid_) {
      runtime::detail::state_stack<State>::pop(value_);
    }
  }

private:
  State* value_{nullptr};
  bool valid_{false};
};

/// Access the currently-active state of type `State` on this thread.
/// Returns a reference to the innermost `state_context<State>` frame.
/// Throws `pb::pipeline_exception` (category `contract_violation`) if
/// no frame is active — stages that call this without being inside a
/// `pb::with_state<State>(...)` scope are programmer errors.
template <class State>
[[nodiscard]] auto current_state() -> State& {
  auto* ptr = runtime::detail::state_stack<State>::top();
  if (ptr == nullptr) {
    throw pipeline_exception{runtime::error{
        .category = runtime::error_category::contract_violation,
        .message =
            "pb::current_state<T>() called outside a pb::with_state<T>(...) scope — "
            "wrap the engine with pb::with_state<T>(engine) before invoking it, or use "
            "pb::try_current_state<T>() if state is optional"}};
  }
  return *ptr;
}

/// Non-throwing variant of `current_state<State>()`.  Returns a
/// pointer to the innermost active state, or `nullptr` if no state of
/// type `State` is active on this thread.  Use this when a stage's
/// state dependency is optional.
template <class State>
[[nodiscard]] auto try_current_state() noexcept -> State* {
  return runtime::detail::state_stack<State>::top();
}

namespace runtime::detail {

/// Empty marker — the storage policy carries no state instance.
struct no_state_storage {};

template <class State, class StoragePolicy>
struct stateful_storage;

template <class State>
struct stateful_storage<State, ::pb::policy::state::owned> {
  State value_{};

  stateful_storage() = default;
  explicit stateful_storage(State init) noexcept(std::is_nothrow_move_constructible_v<State>)
      : value_{std::move(init)} {}

  [[nodiscard]] auto active(State* /*caller*/ = nullptr) noexcept -> State* { return &value_; }
};

template <class State>
struct stateful_storage<State, ::pb::policy::state::reset_per_run> {
  State value_{};

  stateful_storage() = default;
  explicit stateful_storage(State init) noexcept(std::is_nothrow_move_constructible_v<State>)
      : value_{std::move(init)} {}

  [[nodiscard]] auto active(State* /*caller*/ = nullptr) noexcept -> State* { return &value_; }

  void reset_before_run() { value_ = State{}; }
};

template <class State>
struct stateful_storage<State, ::pb::policy::state::thread_local_owned> {
  /// Per-thread `State` instances owned by *this* storage object.  Keyed
  /// by `std::thread::id` so each thread that runs through the wrapping
  /// engine gets its own isolated instance, and the whole table dies
  /// with the engine (so a recycled engine address can never resurrect a
  /// stale state — unlike a process-wide `thread_local`).  The map is
  /// guarded by `mutex_` only for the insert/lookup of the per-thread
  /// slot; the `State` objects themselves are touched exclusively by
  /// their owning thread, so stage access never races.
  struct thread_table {
    std::mutex mutex{};
    std::unordered_map<std::thread::id, std::unique_ptr<State>> slots{};
  };

  /// Optional seed copied into every thread's slot on first access.
  std::shared_ptr<const State> seed_{};
  /// Heap-held so the storage object stays movable (mutex is not).
  std::unique_ptr<thread_table> table_{std::make_unique<thread_table>()};

  stateful_storage() = default;
  explicit stateful_storage(State init)
      : seed_{std::make_shared<const State>(std::move(init))} {}

  /// Return the calling thread's own `State` instance, lazily created on
  /// first access from that thread (copy-constructed from `seed_` when a
  /// seed was supplied, otherwise default-constructed) and persisted for
  /// the lifetime of this storage object.
  [[nodiscard]] auto active(State* /*caller*/ = nullptr) -> State* {
    const auto id = std::this_thread::get_id();
    std::lock_guard<std::mutex> guard{table_->mutex};
    auto it = table_->slots.find(id);
    if (it == table_->slots.end()) {
      auto fresh = seed_ ? std::make_unique<State>(*seed_) : std::make_unique<State>();
      it = table_->slots.emplace(id, std::move(fresh)).first;
    }
    return it->second.get();
  }
};

template <class State>
struct stateful_storage<State, ::pb::policy::state::shared> {
  std::shared_ptr<State> value_{std::make_shared<State>()};

  stateful_storage() = default;
  explicit stateful_storage(std::shared_ptr<State> ptr) noexcept : value_{std::move(ptr)} {}
  explicit stateful_storage(State init) : value_{std::make_shared<State>(std::move(init))} {}

  [[nodiscard]] auto active(State* /*caller*/ = nullptr) noexcept -> State* { return value_.get(); }
};

template <class State>
struct stateful_storage<State, ::pb::policy::state::borrowed> {
  stateful_storage() = default;
  [[nodiscard]] auto active(State* caller) noexcept -> State* { return caller; }
};

} // namespace runtime::detail

/// Engine wrapper that threads a typed `State` through every stage
/// invocation via a thread-local context.  Composes with any engine
/// that satisfies the standard engine surface (`run`, `try_run`,
/// `set_observer`, `get_observer`, `describe`, `descriptor`).
///
/// Template parameters:
/// - `Engine`         — the wrapped engine (sequential, thread-pool, or
///                      another wrapper).
/// - `State`          — the state type accessible inside stages via
///                      `pb::current_state<State>()`.
/// - `StoragePolicy`  — one of `pb::policy::state::owned`,
///                      `pb::policy::state::reset_per_run`,
///                      `pb::policy::state::shared`,
///                      `pb::policy::state::borrowed`.  Defaults to
///                      `owned`.
template <class Engine, class State, class StoragePolicy = policy::state::owned>
class stateful_engine {
public:
  using underlying_engine = Engine;
  using state_type        = State;
  using storage_policy    = StoragePolicy;
  using input_type        = typename Engine::input_type;
  using output_type       = typename Engine::output_type;
  using try_result_type   = decltype(std::declval<Engine&>().try_run(std::declval<input_type>()));

  /// Default-construct the state (or, for the borrowed policy, do
  /// nothing).  Required for the `policy::state::borrowed` case where
  /// callers supply state per call.
  stateful_engine() = default;

  /// Wrap `engine` with default-constructed state (or no state for
  /// `borrowed`).
  explicit stateful_engine(Engine engine)
      : engine_{std::move(engine)} {}

  /// Wrap `engine` with a user-supplied initial state.  Not valid for
  /// the `borrowed` policy — borrowed pipelines do not own any state.
  stateful_engine(Engine engine, State init)
    requires (!std::same_as<StoragePolicy, policy::state::borrowed>)
      : engine_{std::move(engine)}, storage_{std::move(init)} {}

  /// Wrap `engine` with a caller-supplied `shared_ptr<State>`.  Only
  /// valid for the `shared` storage policy.
  stateful_engine(Engine engine, std::shared_ptr<State> ptr)
    requires std::same_as<StoragePolicy, policy::state::shared>
      : engine_{std::move(engine)}, storage_{std::move(ptr)} {}

  /// Run the pipeline.  Pushes a thread-local `state_context<State>`
  /// frame before delegating to the wrapped engine.  Stages observe
  /// the state via `pb::current_state<State>()`.
  ///
  /// Available only for storage policies that own state.  Borrowed
  /// pipelines must use `run_with_state(input, state)`.
  auto run(input_type input)
    requires (!std::same_as<StoragePolicy, policy::state::borrowed>)
  {
    if constexpr (std::same_as<StoragePolicy, policy::state::reset_per_run>) {
      storage_.reset_before_run();
    }
    state_context<State> frame{*storage_.active()};
    return engine_.run(std::move(input));
  }

  /// Result-returning variant of `run()`.  Same semantics, but always
  /// returns `result<Output>` regardless of stage style.
  auto try_run(input_type input) -> try_result_type
    requires (!std::same_as<StoragePolicy, policy::state::borrowed>)
  {
    if constexpr (std::same_as<StoragePolicy, policy::state::reset_per_run>) {
      storage_.reset_before_run();
    }
    state_context<State> frame{*storage_.active()};
    return engine_.try_run(std::move(input));
  }

  template <class Iterator, class Sentinel>
  [[nodiscard]] auto try_run_range(Iterator first, Sentinel last) -> std::vector<try_result_type>
    requires (!std::same_as<StoragePolicy, policy::state::borrowed>)
  {
    std::vector<try_result_type> results;
    if constexpr (std::sized_sentinel_for<Sentinel, Iterator>) {
      const auto count = last - first;
      if (count > 0) {
        results.reserve(static_cast<std::size_t>(count));
      }
    }
    for (; first != last; ++first) {
      results.push_back(try_run(input_type{*first}));
    }
    return results;
  }

  template <class Range>
  [[nodiscard]] auto try_run_each(Range&& inputs) -> std::vector<try_result_type>
    requires (!std::same_as<StoragePolicy, policy::state::borrowed>) &&
             requires(Range&& range) {
               std::begin(range);
               std::end(range);
             }
  {
    std::vector<try_result_type> results;
    if constexpr (requires { std::size(inputs); }) {
      results.reserve(static_cast<std::size_t>(std::size(inputs)));
    }
    auto first = std::begin(inputs);
    auto last = std::end(inputs);
    for (; first != last; ++first) {
      if constexpr (!std::is_lvalue_reference_v<Range&&> &&
                    !std::is_const_v<std::remove_reference_t<decltype(*first)>>) {
        results.push_back(try_run(input_type{std::move(*first)}));
      } else {
        results.push_back(try_run(input_type{*first}));
      }
    }
    return results;
  }

  /// Borrowed-state run — caller supplies the active state for this
  /// call.  Available for every storage policy: even owned pipelines
  /// can opt to use a caller-supplied state on a per-call basis,
  /// shadowing the engine-owned one.
  [[nodiscard]] auto run_with_state(input_type input, State& state) {
    state_context<State> frame{state};
    return engine_.run(std::move(input));
  }

  /// Result-returning variant of `run_with_state()`.
  [[nodiscard]] auto try_run_with_state(input_type input, State& state) -> try_result_type {
    state_context<State> frame{state};
    return engine_.try_run(std::move(input));
  }

  template <class Iterator, class Sentinel>
  [[nodiscard]] auto try_run_range_with_state(Iterator first, Sentinel last, State& state)
      -> std::vector<try_result_type> {
    std::vector<try_result_type> results;
    if constexpr (std::sized_sentinel_for<Sentinel, Iterator>) {
      const auto count = last - first;
      if (count > 0) {
        results.reserve(static_cast<std::size_t>(count));
      }
    }
    for (; first != last; ++first) {
      results.push_back(try_run_with_state(input_type{*first}, state));
    }
    return results;
  }

  template <class Range>
  [[nodiscard]] auto try_run_each_with_state(Range&& inputs, State& state)
      -> std::vector<try_result_type>
    requires requires(Range&& range) {
      std::begin(range);
      std::end(range);
    }
  {
    std::vector<try_result_type> results;
    if constexpr (requires { std::size(inputs); }) {
      results.reserve(static_cast<std::size_t>(std::size(inputs)));
    }
    auto first = std::begin(inputs);
    auto last = std::end(inputs);
    for (; first != last; ++first) {
      if constexpr (!std::is_lvalue_reference_v<Range&&> &&
                    !std::is_const_v<std::remove_reference_t<decltype(*first)>>) {
        results.push_back(try_run_with_state(input_type{std::move(*first)}, state));
      } else {
        results.push_back(try_run_with_state(input_type{*first}, state));
      }
    }
    return results;
  }

  /// Read-only access to the owned state.  Not available for borrowed
  /// pipelines.
  [[nodiscard]] auto state() const& noexcept -> const State&
    requires (!std::same_as<StoragePolicy, policy::state::borrowed> &&
              !std::same_as<StoragePolicy, policy::state::shared> &&
              !std::same_as<StoragePolicy, policy::state::thread_local_owned>)
  {
    return storage_.value_;
  }

  /// Mutable access to the owned state.  Not available for borrowed
  /// pipelines.
  [[nodiscard]] auto state_mut() & noexcept -> State&
    requires (!std::same_as<StoragePolicy, policy::state::borrowed> &&
              !std::same_as<StoragePolicy, policy::state::shared> &&
              !std::same_as<StoragePolicy, policy::state::thread_local_owned>)
  {
    return storage_.value_;
  }

  /// Replace the owned state with a new value.
  void set_state(State value)
    requires (!std::same_as<StoragePolicy, policy::state::borrowed> &&
              !std::same_as<StoragePolicy, policy::state::shared> &&
              !std::same_as<StoragePolicy, policy::state::thread_local_owned>)
  {
    storage_.value_ = std::move(value);
  }

  /// Reset the owned state to a default-constructed value.  For the
  /// `reset_per_run` policy this is also done automatically before
  /// every `run()`.
  void reset_state()
    requires (!std::same_as<StoragePolicy, policy::state::borrowed> &&
              !std::same_as<StoragePolicy, policy::state::shared> &&
              !std::same_as<StoragePolicy, policy::state::thread_local_owned>)
  {
    storage_.value_ = State{};
  }

  /// Access **this thread's** owned state instance for the
  /// `thread_local_owned` policy.  Each thread that has run at least
  /// once through this engine gets its own isolated `State`; this
  /// returns the calling thread's instance, lazily creating it if the
  /// calling thread has not run yet.  Only available for the
  /// `thread_local_owned` storage policy.
  [[nodiscard]] auto thread_local_state() & -> State&
    requires std::same_as<StoragePolicy, policy::state::thread_local_owned>
  {
    return *storage_.active();
  }

  /// Read access to the shared state pointer.  Only available for the
  /// `shared` storage policy.
  [[nodiscard]] auto shared_state() const noexcept -> std::shared_ptr<State>
    requires std::same_as<StoragePolicy, policy::state::shared>
  {
    return storage_.value_;
  }

  /// Replace the shared state pointer.  Only available for the
  /// `shared` storage policy.
  void set_shared_state(std::shared_ptr<State> ptr) noexcept
    requires std::same_as<StoragePolicy, policy::state::shared>
  {
    storage_.value_ = std::move(ptr);
  }

  /// Observer and descriptor forwarding.  Mirrors the error-policy
  /// wrappers so stateful engines compose with the rest of the
  /// wrapper stack (`pb::with_throw_on_error`, `pb::with_verbose`,
  /// etc.) without surface gaps.
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

  /// Mutable access to the wrapped engine.
  [[nodiscard]] auto underlying() & noexcept -> Engine& { return engine_; }
  [[nodiscard]] auto underlying() const& noexcept -> const Engine& { return engine_; }
  [[nodiscard]] auto underlying() && noexcept -> Engine&& { return std::move(engine_); }

private:
  Engine engine_{};
  [[no_unique_address]] runtime::detail::stateful_storage<State, StoragePolicy> storage_{};
};

/// Factory: wrap `engine` with owned default-constructed state of type
/// `State`.  Stages access the state via `pb::current_state<State>()`.
template <class State, class Engine>
[[nodiscard]] auto with_state(Engine engine)
    -> stateful_engine<Engine, State, policy::state::owned> {
  return stateful_engine<Engine, State, policy::state::owned>{std::move(engine)};
}

/// Factory: wrap `engine` with owned state initialized to `init`.
template <class State, class Engine>
[[nodiscard]] auto with_state(Engine engine, State init)
    -> stateful_engine<Engine, State, policy::state::owned> {
  return stateful_engine<Engine, State, policy::state::owned>{std::move(engine), std::move(init)};
}

/// Factory: wrap `engine` so it requires caller-supplied state on
/// every call.  Use `run_with_state(input, state)` to invoke.
template <class State, class Engine>
[[nodiscard]] auto with_borrowed_state(Engine engine)
    -> stateful_engine<Engine, State, policy::state::borrowed> {
  return stateful_engine<Engine, State, policy::state::borrowed>{std::move(engine)};
}

/// Factory: wrap `engine` with a shared `std::shared_ptr<State>` so
/// the same state can be shared across multiple engines or threads.
/// Synchronization inside the state type is the caller's responsibility.
template <class State, class Engine>
[[nodiscard]] auto with_shared_state(Engine engine,
                                     std::shared_ptr<State> state = std::make_shared<State>())
    -> stateful_engine<Engine, State, policy::state::shared> {
  return stateful_engine<Engine, State, policy::state::shared>{std::move(engine), std::move(state)};
}

/// Factory: wrap `engine` so the owned state is reset to a default-
/// constructed value before every `run()`.  Useful for per-run scratch
/// state that must not leak between calls.
template <class State, class Engine>
[[nodiscard]] auto with_reset_per_run_state(Engine engine)
    -> stateful_engine<Engine, State, policy::state::reset_per_run> {
  return stateful_engine<Engine, State, policy::state::reset_per_run>{std::move(engine)};
}

template <class State, class Engine>
[[nodiscard]] auto with_reset_per_run_state(Engine engine, State init)
    -> stateful_engine<Engine, State, policy::state::reset_per_run> {
  return stateful_engine<Engine, State, policy::state::reset_per_run>{std::move(engine), std::move(init)};
}

/// Factory: wrap `engine` with **per-thread** owned state of type
/// `State`.  Stages access the state via `pb::current_state<State>()`,
/// exactly like `pb::with_state<State>`, but the storage is a
/// `thread_local` instance: when the same wrapped engine is invoked
/// concurrently from multiple threads (e.g. driving independent runs in
/// parallel, or a future parallel backend), each thread observes its own
/// isolated `State` rather than a single shared one.  No synchronization
/// is required because no two threads ever touch the same `State`.
///
/// Lifetime semantics: each calling thread's `State` instance is created
/// lazily on that thread's first `run()` (default-constructed by this
/// overload) and then
/// **persists across subsequent `run()` calls on that same thread** —
/// i.e. it behaves like the owned policy, but partitioned per thread.
/// The instance lives until this engine is destroyed.  Distinct engine
/// instances keep distinct per-thread states (each engine owns its own
/// per-thread table), so two engines on the same thread never alias —
/// and a recycled engine address can never resurrect a stale state.
template <class State, class Engine>
[[nodiscard]] auto with_thread_local_state(Engine engine)
    -> stateful_engine<Engine, State, policy::state::thread_local_owned> {
  return stateful_engine<Engine, State, policy::state::thread_local_owned>{std::move(engine)};
}

/// Factory: wrap `engine` with per-thread owned state seeded from
/// `init`.  The seed is captured once; each thread's `State` instance is
/// **copy-constructed from the seed** the first time that thread runs,
/// then persists across that thread's subsequent runs.  Requires `State`
/// to be copy-constructible.
template <class State, class Engine>
[[nodiscard]] auto with_thread_local_state(Engine engine, State init)
    -> stateful_engine<Engine, State, policy::state::thread_local_owned> {
  return stateful_engine<Engine, State, policy::state::thread_local_owned>{std::move(engine),
                                                                           std::move(init)};
}

} // namespace pb
