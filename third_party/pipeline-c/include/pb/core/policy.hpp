#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>

/// @file
/// @brief Baseline policy DSL — compile-time tag types for pipeline behaviour configuration.
///
/// Every policy type is an empty tag struct.  Users compose them as template arguments
/// (e.g. pb::with<policy::storage::persistent, policy::contracts::strict>) to signal
/// compile-time behavioural choices without runtime overhead.  The compiler eliminates
/// every tag — they are zero-size, zero-cost, and never appear in generated code.

namespace pb::policy {

// ---------------------------------------------------------------------------
// Error-handling policies
// ---------------------------------------------------------------------------
namespace errors {

/// Use std::expected / expected-like error handling.
/// Stages that return result<T, E> propagate errors through the pipeline.
struct expected {};

/// Terminate on stage failure (std::terminate / std::abort).
/// Suitable for pipelines where any failure is unrecoverable.
struct terminate {};

/// Ignore errors; continue pipeline execution regardless.
/// Use with caution — only for best-effort / fire-and-forget workloads.
struct ignore {};

// ---------------------------------------------------------------------------
// Runtime-enforced error-policy markers (carried by ::with<...> into the
// finalized pipeline type and acted upon by pb::compile<P>(sequential{})).
//
// These five named markers select a concrete engine wrapper from
// pb/runtime/error_policy.hpp at compile time.  The error axis is single-valued:
// adding a second runtime error marker is rejected by the pipeline-state builder.
//
//   throwing     -> pb::with_throw_on_error      (throws pipeline_exception)
//   terminating  -> pb::with_terminate_on_error  (std::terminate on failure)
//   ignoring     -> pb::with_ignore_errors       (fallback value on failure)
//   propagating  -> no wrapper (baseline result-returning engine)
//   result       -> no wrapper (baseline result-returning engine)
//
// They are intentionally distinct from the legacy `expected`/`terminate`/
// `ignore` tags above so existing introspection-only DSL usage keeps its
// zero-runtime-behaviour contract.
// ---------------------------------------------------------------------------

/// Throw `pb::pipeline_exception` on stage failure.
struct throwing {};

/// Call `std::terminate()` on stage failure.
struct terminating {};

/// Substitute a fallback value on stage failure (best-effort execution).
struct ignoring {};

/// Surface stage exceptions while preserving result-based failures.
/// Selects no compile<> wrapper (baseline engine) by default.
struct propagating {};

/// Explicit "return result<T>" marker — the library default.
/// Selects no compile<> wrapper (baseline engine).
struct result {};

} // namespace errors

// ---------------------------------------------------------------------------
// Exception policies
// ---------------------------------------------------------------------------
namespace exceptions {

/// Let exceptions propagate naturally through the call stack.
/// The caller is responsible for catching and handling them.
struct propagate {};

/// Catch all exceptions at stage boundaries, convert them into
/// the pipeline's error type (e.g. result<T, E>).
struct catch_all {};

} // namespace exceptions

// ---------------------------------------------------------------------------
// Copy / move / borrow policies (value-category semantics)
// ---------------------------------------------------------------------------
namespace memory {

/// Copy inputs between stages.
/// Safe, but may incur heap allocations for large objects.
struct copy {};

/// Move inputs between stages (std::move semantics).
/// Zero-copy for movable types; leaves source in valid-but-unspecified state.
struct move {};

/// Pass by const reference (borrow semantics).
/// Zero-copy, zero-overhead; the caller retains ownership.
struct borrow {};

} // namespace memory

// ---------------------------------------------------------------------------
// State-storage policies (pipeline engine lifecycle)
// ---------------------------------------------------------------------------
namespace storage {

/// Default-construct stages on every pipeline run.
/// Stateless stages have zero runtime cost with this policy.
struct per_run {};

/// Store stage instances inside the engine, reusing them across runs.
/// Enables stages to carry mutable state between invocations.
struct persistent {};

/// Thread-local storage for stage instances.
/// Each thread gets its own copy; useful with parallel backends.
struct thread_local_storage {};

} // namespace storage

// ---------------------------------------------------------------------------
// Assertion / contract policies
// ---------------------------------------------------------------------------
namespace contracts {

/// Static assertions and compile-time enforcement.
/// Every type constraint is verified at compile-time with static_assert.
struct strict {};

/// Runtime checks only (assert / custom verification).
/// Compile-time checks are relaxed; validation deferred to runtime.
struct relaxed {};

} // namespace contracts

// ---------------------------------------------------------------------------
// Diagnostic verbosity policies
// ---------------------------------------------------------------------------
namespace diagnostics {

/// No diagnostics emitted — silence is golden.
struct silent {};

/// Standard diagnostics — enough to understand what's happening.
struct normal {};

/// Detailed diagnostics — every stage transition, timing, and value dump.
///
/// Runtime-enforced marker: when carried by `::with<diagnostics::verbose>`,
/// `pb::compile<P>(sequential{})` wraps the (possibly error-policy-wrapped)
/// engine in `pb::with_verbose_diagnostics(...)`, which auto-attaches a
/// `pb::verbose_observer` that logs every stage transition.
struct verbose {};

/// Explicit "no verbose wrapper" marker — the diagnostics-axis counterpart of
/// `errors::result`.  Carrying `::with<diagnostics::quiet>` records intent in
/// the policy list without changing `compile<>` behaviour: the engine is the
/// bare (or error-policy-wrapped) engine, byte-for-byte identical to carrying
/// no diagnostics marker at all.
struct quiet {};

} // namespace diagnostics

// ---------------------------------------------------------------------------
// Copy / move / borrow value-category policies (carried marker axis)
// ---------------------------------------------------------------------------
///
/// The `copying` markers pin the *intended* value-category contract for values
/// flowing between stages and through fan-out/fan-in branches.  Unlike the
/// `errors` and `diagnostics` axes, the `copying` axis is **carried + queryable
/// but only partially enforced**: `pb::compile<P>(sequential{})` does NOT change
/// runtime copy/move behaviour based on these markers.  Enforcement of value
/// category beyond the existing `pb::shared_view` / `pb::unique_clone` fan-in
/// strategies (see `pb/runtime/clone.hpp`) is forward-looking.
///
/// What the markers buy you today:
///   * They are threaded into the finalized pipeline's `policies` type-list by
///     `::with<copying::...>` and surface through `pb::has_copying_policy_v<P>`
///     / `pb::pipeline_copying_policy_t<P>`.
///   * The axis is single-valued: adding a second copying marker (duplicate or
///     conflicting) is rejected at compile time by the pipeline-state builder.
///   * Tooling and user `static_assert`s can READ the pinned intent to document
///     a pipeline's ownership model, or to gate higher-level adapters.
///
/// They are intentionally distinct from the introspection-only `memory` tags
/// (`copy`/`move`/`borrow`) above so existing zero-runtime-behaviour DSL usage
/// is unaffected.
namespace copying {

/// Values are passed by value (copied) between stages.  The default-intent
/// marker; semantically equivalent to carrying no copying marker for the bare
/// sequential engine.
struct value {};

/// Values are move-only — each value has a single owner and is moved, never
/// copied, between stages.  Pins intent for move-only payloads.
struct move_only {};

/// Values are shared through a copyable view (cf. `pb::shared_view`): one
/// underlying object behind shared ownership, cheap to copy at the boundary.
struct shared {};

/// Values are deep-cloned per consumer (cf. `pb::unique_clone`): every fan-out
/// consumer receives an independently-owned copy.
struct clone {};

} // namespace copying

} // namespace pb::policy

namespace pb::policy {

// ---------------------------------------------------------------------------
// Lifetime / ownership policy metadata
// ---------------------------------------------------------------------------
namespace lifetime {

/// The pipeline borrows input/state references; caller-owned objects must outlive execution.
struct borrowed {};

/// The pipeline owns state/resources for the engine lifetime.
struct owned {};

/// The pipeline shares immutable views across case execution.
struct shared_view {};

} // namespace lifetime

// ---------------------------------------------------------------------------
// Clone / fan-out policy metadata
// ---------------------------------------------------------------------------
namespace clone {

/// Inputs may be copied for fan-out/fan-in execution.
struct copyable_input {};

/// Fan-out uses borrowed const views instead of cloning input objects.
struct borrowed_input {};

/// Future extension point for user-provided clone/projection policy.
struct custom {};

} // namespace clone

// ---------------------------------------------------------------------------
// Scheduling / cancellation policy metadata
// ---------------------------------------------------------------------------
namespace scheduling {

/// Results are reported in declaration/case-index order even if work runs in parallel.
struct deterministic_case_order {};

/// Passing branch cases may execute concurrently when the backend supports it.
struct parallel_cases {};

} // namespace scheduling

namespace cancellation {

/// Do not preempt already-running branch cases; collect all case results before the join.
struct drain_running_cases {};

/// Future extension point for fail-fast cancellation before work starts.
struct fail_fast_not_started {};

} // namespace cancellation

// ---------------------------------------------------------------------------
// Policy bundle and constexpr introspection helpers
// ---------------------------------------------------------------------------

template <class... Policies>
struct bundle {
  static constexpr std::size_t size = sizeof...(Policies);
};

template <class Policy, class Bundle>
struct contains : std::false_type {};

template <class Policy, class... Policies>
struct contains<Policy, bundle<Policies...>> : std::bool_constant<(std::same_as<Policy, Policies> || ...)> {};

template <class Policy, class Bundle>
inline constexpr bool contains_v = contains<Policy, Bundle>::value;

template <class... Policies>
[[nodiscard]] consteval auto make_bundle() noexcept -> bundle<Policies...> {
  return {};
}

// ---------------------------------------------------------------------------
// Error-policy marker trait
// ---------------------------------------------------------------------------

/// True for exactly the five runtime-enforced error-policy markers in
/// `pb::policy::errors`: throwing, terminating, ignoring, propagating, result.
/// The legacy `expected`/`terminate`/`ignore` introspection tags are NOT
/// error policies for this trait — they never drive compile<> wrapping.
template <class T>
struct is_error_policy : std::false_type {};

template <>
struct is_error_policy<errors::throwing> : std::true_type {};
template <>
struct is_error_policy<errors::terminating> : std::true_type {};
template <>
struct is_error_policy<errors::ignoring> : std::true_type {};
template <>
struct is_error_policy<errors::propagating> : std::true_type {};
template <>
struct is_error_policy<errors::result> : std::true_type {};

template <class T>
inline constexpr bool is_error_policy_v = is_error_policy<T>::value;

// ---------------------------------------------------------------------------
// Diagnostics-policy marker trait
// ---------------------------------------------------------------------------

/// True for exactly the two runtime-enforced diagnostics-policy markers in
/// `pb::policy::diagnostics`: `verbose` (selects the verbose engine wrapper)
/// and `quiet` (selects no wrapper).  The diagnostics axis is single-valued;
/// adding both markers is rejected by the pipeline-state builder.  The legacy `silent`/`normal`
/// introspection tags are NOT diagnostics policies for this trait — they never
/// drive `compile<>` wrapping.
template <class T>
struct is_diagnostics_policy : std::false_type {};

template <>
struct is_diagnostics_policy<diagnostics::verbose> : std::true_type {};
template <>
struct is_diagnostics_policy<diagnostics::quiet> : std::true_type {};

template <class T>
inline constexpr bool is_diagnostics_policy_v = is_diagnostics_policy<T>::value;

// ---------------------------------------------------------------------------
// Copying-policy marker trait
// ---------------------------------------------------------------------------

/// True for the four `pb::policy::copying` markers: `value`, `move_only`,
/// `shared`, `clone`.  These are carried + queryable but only partially
/// enforced — see the `copying` namespace docs.
template <class T>
struct is_copying_policy : std::false_type {};

template <>
struct is_copying_policy<copying::value> : std::true_type {};
template <>
struct is_copying_policy<copying::move_only> : std::true_type {};
template <>
struct is_copying_policy<copying::shared> : std::true_type {};
template <>
struct is_copying_policy<copying::clone> : std::true_type {};

template <class T>
inline constexpr bool is_copying_policy_v = is_copying_policy<T>::value;

using default_thread_pool_fan_in = bundle<scheduling::deterministic_case_order,
                                          scheduling::parallel_cases,
                                          cancellation::drain_running_cases,
                                          clone::copyable_input>;

} // namespace pb::policy
