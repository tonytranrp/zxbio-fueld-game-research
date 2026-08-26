#pragma once

/// \file
/// \brief Optional Taskflow execution-backend SCAFFOLD (compile-guarded seam).
///
/// This header reserves the integration seam for a Taskflow
/// (`tf::Taskflow` / `tf::Executor`) task-graph execution backend. It is
/// **entirely dormant** on the default build: every declaration other than the
/// always-visible availability constant lives inside
/// `#if defined(PB_HAS_TASKFLOW)`. When `PB_HAS_TASKFLOW` is undefined (the
/// default) this file contributes nothing but an include guard, a doxygen design
/// note, and `pb::runtime::taskflow_backend_available_v == false`.
///
/// The macro is defined by the build only when the `PB_ENABLE_TASKFLOW` CMake
/// option is ON *and* `find_package(Taskflow)` succeeds; see the root CMake
/// snippet shipped with this slice. The standard-library default build never
/// defines it, so the core/runtime surface stays dependency-free per
/// `docs/optional-backends-roadmap.md`.

namespace pb::runtime {

/// \brief Always-visible availability gate for the Taskflow backend scaffold.
///
/// `true` only when the translation unit was compiled with `PB_HAS_TASKFLOW`
/// defined (i.e. Taskflow was found and wired in). Declared UNGATED so user code
/// and the scaffold smoke test can reference it on any toolchain, mirroring the
/// existing feature-gate style.
inline constexpr bool taskflow_backend_available_v =
#if defined(PB_HAS_TASKFLOW)
    true;
#else
    false;
#endif

#if defined(PB_HAS_TASKFLOW)

/// \brief Backend tag selecting the Taskflow task-graph lowering.
///
/// Passed to `pb::compile<Pipeline>(taskflow_backend{})` once the adapter is
/// implemented. Today it is only a reserved tag type that exists when the
/// dependency is present; it carries no behaviour yet.
///
/// \par Planned lowering plan (task-graph mapping)
/// The validated pipeline lowers to a single `tf::Taskflow`. Each linear stage
/// `Sn` becomes a `tf::Task`; precedence edges (`task_a.precede(task_b)`) encode
/// the compile-time-validated stage IO edges, so the dependency DAG mirrors the
/// pipeline topology exactly. The whole graph is submitted once via
/// `tf::Executor::run(taskflow)` and awaited through the returned `tf::Future`.
///
/// \par Serial vs. parallel pipes
/// - A linear chain lowers to a strictly ordered path of tasks (one predecessor
///   edge each), giving the same observable order as the sequential backend.
/// - Explicit fan-in case stages lower to sibling tasks with a common
///   predecessor (scatter) and a common successor join task (gather). The
///   executor schedules the siblings concurrently while the join task collects
///   their `result<>` slots in declaration order, preserving the fan-in
///   ordered-aggregate contract.
/// - Stages flagged order-independent may additionally be expressed with
///   `tf::Subflow` for nested parallelism.
///
/// \par corun / worker-participation rule
/// Taskflow forbids a worker thread from blocking on `tf::Future::wait()` for a
/// graph owned by the same executor (it would deadlock the worker pool).
/// Therefore the adapter MUST use `executor.corun(taskflow)` (or
/// `corun_until`) whenever `compile(...).run()` is itself invoked from inside a
/// Taskflow worker, so the calling worker *participates* in executing the graph
/// instead of blocking. Top-level (non-worker) callers may use the plain
/// `run().wait()` path. This worker-participation rule is the central
/// correctness constraint of the Taskflow lowering and will be enforced/tested
/// before the backend is claimed as supported.
///
/// \par Concurrency, error & cancellation channel (planned)
/// Worker count is taken from the `tf::Executor` (configurable via a
/// `taskflow_backend` field). Stage failures normalize to `pb::result<>`;
/// cooperative cancellation maps to a `pb::cancellation_token` checked at task
/// boundaries, consistent with the thread-pool backend's non-preemptive model.
struct taskflow_backend {};

#endif // PB_HAS_TASKFLOW

} // namespace pb::runtime
