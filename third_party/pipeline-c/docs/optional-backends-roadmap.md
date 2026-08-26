# Optional Backends Roadmap / Status

Optional backend execution is now **partially supported**: the standard-library thread-pool backend can execute validated pipelines and parallelizes explicit fan-in case stages for supported `::fan_in` / `::join_all` shapes. Treat this page as the supported-boundary note for that first backend slice and as roadmap/status for the remaining optional backend adapters.

## Current status

Today the repository supports:

- a standard-library-only core/runtime surface for the shipped MVP
- linear compile-time pipeline validation through `pb::from<T>::then<S>::to<U>`
- sequential runtime execution for validated linear and supported branch pipelines
- a standard-library `pb::runtime::thread_pool` utility with smoke coverage
- a standard-library `pb::runtime::thread_pool_backend` pipeline backend for supported fan-in execution
- a public backend feature matrix that identifies supported versus roadmap/experimental backends
- build-time feature flags that reserve space for future backend experiments and fail fast if enabled before an adapter exists

Today the repository does **not** support:

- public oneTBB, Taskflow, or stdexec backend *adapters* (only dormant, compile-guarded scaffold headers + availability constants exist; see "Gated backend scaffolds")
- backend-specific examples or benchmarks beyond targeted runtime tests
- stable async/parallel execution semantics across multiple executors
- preemptive cancellation of already-running case stages

Keep release notes and examples aligned with that boundary. Present only the tested `thread_pool_backend` fan-in slice as shipped; keep dependency backends and broader parallel semantics as roadmap work.

The current matrix is exposed through `pb::runtime::backend_features()` / `pb::backend_features()`:

| Backend | Status | Default build | External dependency | Execution model |
| --- | --- | --- | --- | --- |
| `sequential` | supported | yes | no | deterministic linear sequential runtime |
| `thread_pool` | supported | no | no | standard-library backend; parallelizes passing fan-in case stages, preserves declaration-order aggregate results, drains running cases |
| `taskflow` | roadmap | no | yes | future task-graph adapter |
| `oneTBB` | roadmap | no | yes | future filter-pipeline adapter |
| `stdexec` | experimental | no | yes | future sender/receiver adapter |

`PB_BACKEND_TASKFLOW`, `PB_BACKEND_TBB`, and `PB_BACKEND_STDEXEC` intentionally stop configuration with a clear error today rather than silently producing a build without backend support.

## Gated backend scaffolds (integration seam)

To close the research-plan "optional pipeline execution backends" *design* seam without requiring the libraries to be installed, the repository now ships three compile-guarded scaffold headers. They reserve the public namespace, document the intended lowering, and stay **completely dormant** on the default build:

| Header | Tag type | Availability constant | Gate macro | Lowering target |
| --- | --- | --- | --- | --- |
| `pb/backends/tbb.hpp` | `pb::runtime::tbb_backend` | `pb::runtime::tbb_backend_available_v` | `PB_HAS_TBB` | oneTBB `parallel_pipeline` filter chain |
| `pb/backends/taskflow.hpp` | `pb::runtime::taskflow_backend` | `pb::runtime::taskflow_backend_available_v` | `PB_HAS_TASKFLOW` | Taskflow task graph |
| `pb/backends/stdexec.hpp` | `pb::runtime::stdexec_backend` | `pb::runtime::stdexec_backend_available_v` | `PB_HAS_STDEXEC` | stdexec sender/receiver chain |

Seam rules (mirroring the existing feature-gate style):

- The backend **tag struct** (`*_backend`) and all lowering code live entirely inside `#if defined(PB_HAS_*)`. When the macro is undefined — which is always the case on the default standard-library build — the header contributes nothing but an include guard, a doxygen design note, and the availability constant.
- Each **`*_available_v` constant is visible UNGATED** on every toolchain: it is `true` only when its `PB_HAS_*` macro is defined and `false` otherwise. User code and the scaffold smoke test can therefore branch on backend presence without first knowing whether the dependency was found.
- The scaffold headers are **not** pulled in by `pb/pipeline.hpp`; callers include `pb/backends/<name>.hpp` directly, so the default public include surface is unchanged and stays dependency-free.

Documented lowering plans captured in the headers:

- **oneTBB** (`tbb.hpp`): linear stages map to a `tbb::parallel_pipeline` filter chain (`make_filter` per stage); serial-in-order vs. parallel filter modes; `max_number_of_live_tokens` as the backpressure/token-limit knob.
- **Taskflow** (`taskflow.hpp`): stages map to `tf::Task` nodes with precedence edges; fan-in lowers to scatter/gather sibling tasks; the central correctness rule is **worker participation via `executor.corun(...)`** so a Taskflow worker never blocks on its own graph.
- **stdexec** (`stdexec.hpp`): linear stages map to a `schedule | then | ...` sender chain; documents the value/error/stopped **completion signatures**, the `set_error(pb::error)` / `set_error(std::exception_ptr)` **error channel**, and **cancellation via the receiver-environment `stop_token`**.

### Enabling the scaffolds (kept OFF)

Three root CMake options gate the macros — `PB_ENABLE_TBB`, `PB_ENABLE_TASKFLOW`, `PB_ENABLE_STDEXEC`, all **default OFF**. When turned ON, each option `find_package`s the corresponding dependency and, on success, adds the matching `PB_HAS_*` compile definition to `pb::pipeline`. Because the dependencies are not installed and no execution adapter exists yet, these options are intentionally **left OFF and unwired in the default build**; they exist only to mark the seam. These are distinct from the older fail-fast `PB_BACKEND_*` flags, which still hard-error to prevent claiming unimplemented execution support.

The `pb_backend_scaffolds_smoke_compile_pass` test (labels `compile-pass;backend;scaffold`) includes all three headers and `static_assert`s that every `*_available_v` is `false` on the default build, proving the seam is dormant and the default build is unaffected.

## Why optional backends matter

Optional backends are the path from the current deterministic sequential executor to richer execution models. The research plan treats them as important because they can eventually provide:

- parallel execution for suitable pipeline shapes
- integration with established task-graph and filter-pipeline runtimes
- experimental sender/receiver execution models once compiler support is practical
- backend-specific tuning for token limits, scheduling, and concurrency policy

At the same time, the research plan is explicit that these backends should remain isolated from the core DSL and only arrive after the basic validation/runtime story is stable.

## Intended scope relative to the research plan

The research plan treats optional backends as planned work, not current behavior:

- sequential execution is the first supported backend
- the first standard-library thread-pool backend slice is implemented for supported fan-in pipelines
- oneTBB and Taskflow remain optional adapter targets
- stdexec/sender-receiver support stays experimental until compiler/library support stabilizes
- backend-specific headers and dependencies must stay out of public core headers

The current repository now ships the first runtime adapter needed to claim limited backend support beyond sequential execution: `pb::runtime::thread_pool_backend`. The support claim is intentionally narrow. It covers validated pipelines and uses parallel worker tasks only for explicit fan-in case stages; selected-output branch routing and ordinary linear stages retain deterministic pipeline order. oneTBB, Taskflow, and stdexec remain roadmap adapters.

## Non-goals for the current MVP

The current MVP should **not** claim:

- production-ready parallel execution semantics beyond supported fan-in case scheduling
- stable external dependency integration for oneTBB, Taskflow, or stdexec
- backend-agnostic cancellation, scheduling, or token-limit policy beyond the documented thread-pool defaults
- dependency backend headers in the default user include surface

Those decisions belong to later slices with explicit adapter targets, tests, and performance evidence.

## What would be required before claiming support

Before each additional optional backend or broader thread-pool behavior can move from roadmap to supported behavior, the repo needs:

1. **A defined backend adapter surface**  
   Stable target names, feature-gate behavior, dependency rules, and error/ownership expectations. The current feature matrix and fail-fast CMake flags define the status-reporting and feature-gate boundary, but not execution adapters.
2. **Implementation coverage for each backend**  
   A real adapter path for the chosen backend, not just a CMake option placeholder.
3. **Targeted tests and examples**  
   Positive runtime tests, negative/configuration-failure checks, and at least one backend-specific example where support is claimed.
4. **Isolation proof**  
   Evidence that backend dependencies stay out of the core public headers and default build surface.
5. **Performance and behavior evidence**  
   Validation for concurrency/tokens/scheduling behavior plus compile-time/runtime cost before treating the backend as production-ready.

## Verification status today

The current verification evidence covers the sequential pipeline core and the first thread-pool fan-in backend slice:

- compile-pass coverage
- compile-fail diagnostic coverage
- runtime smoke coverage
- package-consumer smoke coverage
- benchmark smoke scaffolding

The `pb_runtime_thread_pool_smoke` test covers the standalone `pb::runtime::thread_pool` utility, and `pb_runtime_thread_pool_fan_in_smoke` compiles and runs pipelines through `pb::runtime::thread_pool_backend`. The backend smoke proves worker-count configuration, parallel overlap for passing fan-in cases, deterministic ordered aggregation despite concurrent execution, failed-case aggregation, void-output case aggregation, borrowed move-only fan-in, and synchronized observer callbacks.

## Release guidance

Release notes and docs may describe optional backends as:

> `thread_pool_backend` is supported for the current standard-library fan-in backend slice; oneTBB, Taskflow, stdexec, preemptive cancellation, backend examples, and backend benchmarks remain roadmap work.

If a future slice adds optional backends, update this page together with:

- `docs/production-readiness.md`
- any backend-specific examples or API docs
- build guidance for enabling and verifying those backends
- the relevant tests/bench entries that validate isolation, runtime behavior, and cost
