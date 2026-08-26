#pragma once

/// \file
/// \brief Optional oneTBB execution-backend SCAFFOLD (compile-guarded seam).
///
/// This header reserves the integration seam for an oneTBB
/// (`tbb::parallel_pipeline`) execution backend. It is **entirely dormant** on
/// the default build: every declaration other than the always-visible
/// availability constant lives inside `#if defined(PB_HAS_TBB)`. When
/// `PB_HAS_TBB` is undefined (the default) this file contributes nothing but an
/// include guard, a doxygen design note, and
/// `pb::runtime::tbb_backend_available_v == false`.
///
/// The macro is defined by the build only when the `PB_ENABLE_TBB` CMake option
/// is ON *and* `find_package(TBB)` succeeds; see the root CMake snippet shipped
/// with this slice. The standard-library default build never defines it, so the
/// core/runtime surface stays dependency-free per
/// `docs/optional-backends-roadmap.md`.

namespace pb::runtime {

/// \brief Always-visible availability gate for the oneTBB backend scaffold.
///
/// `true` only when the translation unit was compiled with `PB_HAS_TBB`
/// defined (i.e. oneTBB was found and wired in). The constant is declared
/// UNGATED so that user code and the scaffold smoke test can reference it on any
/// toolchain to decide whether the backend is present, mirroring the existing
/// feature-gate style (`pb::runtime::backend_supported`).
inline constexpr bool tbb_backend_available_v =
#if defined(PB_HAS_TBB)
    true;
#else
    false;
#endif

#if defined(PB_HAS_TBB)

/// \brief Backend tag selecting the oneTBB `parallel_pipeline` lowering.
///
/// Passed to `pb::compile<Pipeline>(tbb_backend{})` once the adapter is
/// implemented. Today it is only a reserved tag type that exists when the
/// dependency is present; it carries no behaviour yet.
///
/// \par Planned lowering plan (filter-chain mapping)
/// The validated linear pipeline `from<I>::then<S0>::then<S1>...::to<O>` lowers
/// to a single `tbb::parallel_pipeline` whose ordered token stream is the input
/// values. Each pipeline stage `Sn` becomes one `tbb::make_filter<In, Out>`
/// node, chained left-to-right with `operator&`, so the filter chain's
/// input/output types match the compile-time-validated stage IO edges exactly.
///
/// \par Serial vs. parallel filter modes
/// - Stages with externally observable order or shared mutable state lower to
///   `tbb::filter_mode::serial_in_order` (deterministic, declaration order
///   preserved). This is the default and mirrors the sequential backend's
///   observable semantics.
/// - Stages declared stateless / order-independent may lower to
///   `tbb::filter_mode::parallel`, letting oneTBB run multiple tokens through
///   that stage concurrently.
/// - The input-producing filter and the output-collecting filter are
///   `serial_in_order` so results aggregate in declaration order, matching the
///   thread-pool backend's ordered-aggregate guarantee.
/// - Explicit fan-in case stages map to a parallel sub-region whose ordered
///   join reuses the existing `result<>` aggregate, preserving the fan-in
///   ordering contract.
///
/// \par max_number_of_live_tokens
/// `tbb::parallel_pipeline(max_number_of_live_tokens, filters)` bounds in-flight
/// tokens (backpressure / memory ceiling). The adapter will surface this through
/// a `tbb_backend` field (default derived from worker count, e.g.
/// `2 * worker_count`) so callers can tune throughput vs. peak memory the same
/// way `thread_pool_backend::worker_count` tunes the standard-library backend.
///
/// \par Error & cancellation channel (planned)
/// Stage failures normalize to the existing `pb::result<>` error type; the first
/// failing token short-circuits the pipeline. Cooperative cancellation maps to a
/// `pb::cancellation_token` checked at filter boundaries, consistent with the
/// thread-pool backend's cooperative (non-preemptive) model.
struct tbb_backend {};

#endif // PB_HAS_TBB

} // namespace pb::runtime
