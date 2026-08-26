#pragma once

/// \file
/// \brief Optional stdexec (P2300 sender/receiver) execution-backend SCAFFOLD.
///
/// This header reserves the integration seam for an stdexec / `std::execution`
/// (sender/receiver) execution backend. It is **entirely dormant** on the
/// default build: every declaration other than the always-visible availability
/// constant lives inside `#if defined(PB_HAS_STDEXEC)`. When `PB_HAS_STDEXEC`
/// is undefined (the default) this file contributes nothing but an include
/// guard, a doxygen design note, and
/// `pb::runtime::stdexec_backend_available_v == false`.
///
/// The macro is defined by the build only when the `PB_ENABLE_STDEXEC` CMake
/// option is ON *and* `find_package(stdexec)` succeeds; see the root CMake
/// snippet shipped with this slice. The standard-library default build never
/// defines it, so the core/runtime surface stays dependency-free per
/// `docs/optional-backends-roadmap.md`. This backend stays the most
/// experimental of the three because it depends on compiler/library maturity.

namespace pb::runtime {

/// \brief Always-visible availability gate for the stdexec backend scaffold.
///
/// `true` only when the translation unit was compiled with `PB_HAS_STDEXEC`
/// defined (i.e. stdexec was found and wired in). Declared UNGATED so user code
/// and the scaffold smoke test can reference it on any toolchain, mirroring the
/// existing feature-gate style.
inline constexpr bool stdexec_backend_available_v =
#if defined(PB_HAS_STDEXEC)
    true;
#else
    false;
#endif

#if defined(PB_HAS_STDEXEC)

/// \brief Backend tag selecting the stdexec sender/receiver lowering.
///
/// Passed to `pb::compile<Pipeline>(stdexec_backend{})` once the adapter is
/// implemented. Today it is only a reserved tag type that exists when the
/// dependency is present; it carries no behaviour yet.
///
/// \par Planned lowering plan (sender/receiver mapping)
/// The validated linear pipeline `from<I>::then<S0>...::to<O>` lowers to a
/// single sender chain. Starting from a scheduler's `stdexec::schedule(sched)`
/// seeded with the input value, each stage `Sn` becomes a
/// `stdexec::then([](In in) -> Out { return Sn{}(in); })` continuation, so the
/// sender's value type at each link matches the compile-time-validated stage IO
/// edge. The terminal sender is consumed with `stdexec::sync_wait(...)` at the
/// `compile(...).run()` boundary (or returned as a sender for async callers).
///
/// \par Completion signatures
/// Each composed sender advertises three channels:
/// - value: `set_value_t(Output)` (or `set_value_t()` for a void-output
///   pipeline), carrying the successful result;
/// - error: `set_error_t(pb::error)` plus `set_error_t(std::exception_ptr)` for
///   thrown exceptions normalized into the existing diagnostic type;
/// - stopped: `set_stopped_t()` for cancellation.
/// The adapter exposes these via a `completion_signatures` specialization so the
/// chain composes with arbitrary downstream receivers.
///
/// \par Error channel
/// A stage returning a failed `pb::result<>` completes the chain on the error
/// channel via `set_error(pb::error)`, short-circuiting later `then` nodes
/// (sender composition naturally skips continuations on error). Thrown
/// exceptions are caught at the stage boundary and forwarded as
/// `set_error(std::exception_ptr)`, matching the sequential/thread-pool error
/// normalization.
///
/// \par Cancellation via stop_token
/// Cooperative cancellation maps to the receiver's environment
/// `stop_token` (`stdexec::get_stop_token(get_env(receiver))`). Stage
/// continuations poll `stop_requested()` at their boundaries and complete via
/// `set_stopped()` when a stop is requested, consistent with the thread-pool
/// backend's cooperative (non-preemptive) cancellation model. A
/// `pb::cancellation_token` supplied through a `stdexec_backend` field bridges to
/// that stop source.
struct stdexec_backend {};

#endif // PB_HAS_STDEXEC

} // namespace pb::runtime
