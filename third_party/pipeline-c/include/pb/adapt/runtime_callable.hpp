#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include "pb/core/concepts.hpp"

/// @file
/// @brief Runtime-bound stateful-callable adapters.
///
/// `pb/adapt/fn.hpp` adapts free functions, member functions, and
/// *default-constructible* function objects into pipeline stages.  Every
/// adapter there carries its callable either as a non-type template
/// parameter (free / member function pointers) or as a type that the
/// runtime can default-construct.  That covers compile-time-known
/// callables, but it cannot wrap a callable whose behaviour is only
/// determined at **runtime** — a stateful lambda with captures, a
/// `std::function` assembled from configuration, or a member function
/// bound to a particular object instance.
///
/// This header closes that gap with two complementary adapters:
///
/// - `pb::runtime_callable<In, Out>` — a stage that *owns* an arbitrary
///   callable supplied at construction.  Because the callable carries
///   runtime state that must survive across invocations, the stage must be
///   used with the **stateful storage policy**
///   (`pb::runtime::stateful_sequential` /
///   `pb::runtime::store_stages_in_engine`), which stores stage instances
///   inside the engine — see @ref runtime-callable-stateful below.
/// - `pb::c_function_stage<In, Out, Fn>` — adapts a C-style free-function
///   pointer supplied as a *non-type template parameter*, covering the
///   "C API" adapter direction.  It carries no runtime state and therefore
///   works with the default `pb::runtime::sequential{}` policy too.
///
/// @section runtime-callable-stateful Stateful storage requirement
///
/// The default engine policy (`pb::runtime::sequential{}` /
/// `construct_stages_per_run`) *default-constructs a fresh stage object for
/// every invocation*, so a runtime-bound callable would be lost.  The
/// stateful policy (`pb::runtime::stateful_sequential` /
/// `store_stages_in_engine`) instead stores the stage inside the engine's
/// `std::tuple<Stages...>` and invokes the **same instance** on every run.
///
/// `runtime_callable` shares its bound callable through a
/// `std::shared_ptr`, so even though the engine value-initialises its stage
/// tuple, the bound callable is published through a scoped thread-local
/// *binding* established by `pb::with_runtime_callable_binding(...)` (or the
/// convenience `run_bound` / `try_run_bound` helpers).  A
/// `runtime_callable` that finds no active binding falls back to its own
/// owned callable (the one passed to its constructor), which is exactly the
/// instance used when the stage is invoked directly:
///
/// ```cpp
/// auto counter = std::make_shared<int>(0);
/// auto stage = pb::bind_callable<In, Out>(
///     [counter](In in) { return Out{in.value * ++*counter}; });
///
/// using Pipeline = pb::from<In>::then<decltype(stage)>::to<Out>;
/// auto engine = pb::compile<Pipeline>(pb::runtime::stateful_sequential{});
///
/// auto out = pb::run_bound(engine, In{10}, stage);   // binds `stage`, runs
/// ```
namespace pb {

namespace adapt_detail {

/// True when `Callable` invoked with `Input` yields exactly `Output`
/// (after removing reference/cv qualifiers from the result).
template <class Callable, class Input, class Output>
concept runtime_invocable_as_output =
    std::invocable<Callable&, Input> &&
    std::same_as<std::remove_cvref_t<std::invoke_result_t<Callable&, Input>>, Output>;

} // namespace adapt_detail

// ---------------------------------------------------------------------------
// runtime_callable<In, Out>
//
// A pipeline stage that OWNS an arbitrary callable bound at RUNTIME — a
// stateful lambda with captures, a std::function, or a bound member.  The
// callable is type-erased into std::function<Out(In)> and held by a shared
// pointer so the stage stays copyable and default-constructible (required
// by the engine's value-initialised stage tuple) while still carrying the
// caller's runtime state.
//
// Because the bound callable carries runtime state, this stage MUST be used
// with the stateful storage policy (pb::runtime::stateful_sequential /
// pb::runtime::store_stages_in_engine).  The engine value-initialises its
// stored copy, so the bound callable is published to that copy through a
// scoped thread-local binding (pb::with_runtime_callable_binding / the
// run_bound helpers).  See @ref runtime-callable-stateful above.
//
// Prefer the pb::bind_callable<In, Out>(f) factory, which deduces and
// type-erases the callable for you.
// ---------------------------------------------------------------------------
template <class In, class Out>
struct runtime_callable {
  using input_type    = In;
  using output_type   = Out;
  using error_type    = no_error;
  using callable_type = std::function<Out(In)>;

  /// Shared so a copy stored inside the engine and the caller's own copy
  /// can refer to the same bound callable.
  std::shared_ptr<callable_type> callable_{};

  /// Default-construct an *unbound* stage.  Required by the stateful
  /// engine's value-initialised `std::tuple<Stages...>`.  An unbound stage
  /// must be supplied a callable via a scoped binding (see
  /// `pb::with_runtime_callable_binding`) before it is invoked.
  runtime_callable() = default;

  /// Construct a stage that owns `callable`.
  explicit runtime_callable(callable_type callable)
      : callable_{std::make_shared<callable_type>(std::move(callable))} {}

  static constexpr std::string_view stage_name() noexcept { return "runtime_callable"; }

  /// Invoke the bound callable.  Resolution order:
  ///   1. the thread-local active binding for this stage type, if any
  ///      (established by `pb::with_runtime_callable_binding`); else
  ///   2. this stage's own owned callable (constructor-supplied).
  /// Throws `std::bad_function_call` if neither is bound.
  [[nodiscard]] output_type operator()(input_type input) const {
    if (auto* bound = active_binding(); bound != nullptr) {
      return (*bound)(std::move(input));
    }
    if (!callable_ || !*callable_) {
      throw std::bad_function_call{};
    }
    return (*callable_)(std::move(input));
  }

  /// Pointer to the thread-local active binding for this stage type, or
  /// `nullptr` when no `with_runtime_callable_binding` scope is active.
  /// Exposed so the scope guard can save/restore it.
  [[nodiscard]] static auto active_binding() noexcept -> callable_type*& {
    thread_local callable_type* binding = nullptr;
    return binding;
  }
};

/// RAII scope that makes `stage`'s owned callable the active binding for
/// `runtime_callable<In, Out>` on the current thread.  While the guard is
/// alive, every `runtime_callable<In, Out>` invoked on this thread — most
/// importantly the engine's value-initialised stored copy — routes through
/// `stage`'s callable.  Nested guards stack (LIFO).
template <class In, class Out>
class runtime_callable_binding {
public:
  explicit runtime_callable_binding(runtime_callable<In, Out>& stage)
      : previous_{runtime_callable<In, Out>::active_binding()} {
    if (!stage.callable_ || !*stage.callable_) {
      throw std::bad_function_call{};
    }
    runtime_callable<In, Out>::active_binding() = stage.callable_.get();
  }

  runtime_callable_binding(const runtime_callable_binding&) = delete;
  runtime_callable_binding& operator=(const runtime_callable_binding&) = delete;
  runtime_callable_binding(runtime_callable_binding&&) = delete;
  runtime_callable_binding& operator=(runtime_callable_binding&&) = delete;

  ~runtime_callable_binding() { runtime_callable<In, Out>::active_binding() = previous_; }

private:
  typename runtime_callable<In, Out>::callable_type* previous_{nullptr};
};

/// Establish a scoped binding making `stage`'s callable active for the
/// matching `runtime_callable<In, Out>` stage type on this thread.  Return
/// value must be kept alive (RAII) for the duration of the run.
template <class In, class Out>
[[nodiscard]] auto with_runtime_callable_binding(runtime_callable<In, Out>& stage)
    -> runtime_callable_binding<In, Out> {
  return runtime_callable_binding<In, Out>{stage};
}

/// Convenience: bind `stage` and `engine.run(input)` in one call.  The
/// binding is active only for the duration of this run.
template <class Engine, class In, class Out>
[[nodiscard]] auto run_bound(Engine& engine, In input, runtime_callable<In, Out>& stage)
    -> decltype(engine.run(std::move(input))) {
  auto guard = with_runtime_callable_binding(stage);
  return engine.run(std::move(input));
}

/// Convenience: bind `stage` and `engine.try_run(input)` in one call.
template <class Engine, class In, class Out>
[[nodiscard]] auto try_run_bound(Engine& engine, In input, runtime_callable<In, Out>& stage)
    -> decltype(engine.try_run(std::move(input))) {
  auto guard = with_runtime_callable_binding(stage);
  return engine.try_run(std::move(input));
}

/// Factory: build a `runtime_callable<In, Out>` from any callable `f`
/// (a stateful/capturing lambda, a `std::function`, or a bound member).
/// `In` and `Out` are supplied explicitly; `f` is type-erased into the
/// stage's `std::function<Out(In)>`.
///
/// The returned stage owns `f` and must be used with the stateful storage
/// policy via a `with_runtime_callable_binding` scope — see
/// @ref runtime-callable-stateful.
template <class In, class Out, class F>
[[nodiscard]] auto bind_callable(F&& f) -> runtime_callable<In, Out> {
  static_assert(adapt_detail::runtime_invocable_as_output<std::remove_cvref_t<F>, In, Out>,
                "pb::bind_callable: f must be invocable with In and return Out");
  return runtime_callable<In, Out>{std::function<Out(In)>{std::forward<F>(f)}};
}

// ---------------------------------------------------------------------------
// c_function_stage<In, Out, Fn>
//
// Adapts a C-style free-function pointer `Out (*Fn)(In)` supplied as a
// NON-TYPE template parameter into a pipeline stage.  The function address
// is embedded in the type, so the stage is default-constructible and carries
// no runtime state — it therefore composes under the default
// pb::runtime::sequential{} policy as well as the stateful one.  This covers
// the "C API" adapter direction: wrapping a plain extern-"C" function.
// ---------------------------------------------------------------------------
template <class In, class Out, Out (*Fn)(In)>
struct c_function_stage {
  using input_type  = In;
  using output_type = Out;
  using error_type  = no_error;

  static constexpr auto function = Fn;

  static constexpr std::string_view stage_name() noexcept { return "c_function_stage"; }

  [[nodiscard]] constexpr output_type operator()(input_type input) const
      noexcept(noexcept(Fn(std::move(input)))) {
    return Fn(std::move(input));
  }
};

} // namespace pb
