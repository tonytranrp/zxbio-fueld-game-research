# Roadmap Gap Map

This page maps the major themes in `research/pipeline_builder_cpp_research_plan.md` to the repository evidence that exists **today**. Use it to separate shipped MVP support from roadmap-only gaps, and to identify the next safe documentation or implementation slices without overstating production readiness.

For a compact release-governance table that maps each gap to evidence, tests, release status, and the next required slice, see [Research Verification Matrix](research-verification-matrix.md).

## Current audited baseline

Historical local audit baseline: `aa3b8f6` on `main`. That MVP audit found the linear pipeline builder sound and locally green: developer configure/build/CTest completed with **78/78 tests passed**.

Current integrated worker-1/3/4 team head (wave-1/2/3 plus post-wave diagnostics/batch-run/export optimization, sender/receiver scaffold, export-helper misuse diagnostics, tooling registry/CLI option hardening, state DSL accessor hardening, fan-in observer/trace hardening, and reflect_stage fallback diagnostics commits): **250/250 ctest** on `clang-dev-ninja`. The sender/receiver module export update passes a direct `pipeline.mpp` syntax probe; the `modules-ninja` preset was not refreshed locally because `clang-scan-deps` is missing.

The previous cross-compiler-validated SHA is still `87299c14c813753d170911239e251064cbbfee6f`; cross-compiler validation has not yet been rerun on the new SHA.

## Reading this map

Each theme below records four things:

- **Current repository evidence** — what the tree already proves or documents.
- **Current support level** — shipped MVP support versus roadmap-only status.
- **Proof points** — the tests, examples, or docs that currently back the claim.
- **Safe next slice** — a narrow follow-on step that can strengthen the evidence without pretending the broader roadmap is complete.

## Theme map

### 1. Linear pipeline core

- **Current repository evidence:** The public docs and headers describe a linear typed pipeline builder with explicit stage metadata and compile-time validation.
- **Current support level:** **Shipped MVP support.**
- **Proof points:**
  - `docs/production-readiness.md`
  - `docs/examples.md`
  - `include/pb/core/describe.hpp`
  - compile-pass coverage under `tests/compile_pass/`
- **Safe next slice:** keep public docs synchronized with any future core API additions rather than broadening the supported topology claims.

### 2. Sequential runtime execution

- **Current repository evidence:** The repository documents and ships the sequential runtime path for validated linear pipelines. The sequential engine gained the 4th `Policies` type-list parameter enabling runtime-enforced `::with<>` DSL.
- **Current support level:** **Shipped MVP support.**
- **Proof points:**
  - `docs/production-readiness.md`
  - `include/pb/runtime/sequential.hpp`
  - runtime smoke coverage under `tests/runtime/`
  - `tests/runtime/with_policy_dsl_runtime_smoke.cpp`
  - `tests/compile_pass/with_policy_dsl_compile_pass.cpp`
- **Safe next slice:** keep runtime behavior, docs, and smoke coverage aligned; the runtime-enforced `::with<>` error-policy DSL is now active.

### 3. Diagnostics and compile-fail guidance

- **Current repository evidence:** Diagnostics are documented as partially supported for the current MVP, with compile-fail coverage, runtime error-formatting coverage, and a narrow machine-readable `pb.diagnostics.v1` record/collector contract.
- **Current support level:** **Partially shipped MVP support with broader roadmap work still open.**
- **Proof points:**
  - `docs/diagnostics-roadmap.md`
  - `docs/diagnostic-example-walkthrough.md`
  - `examples/error_diagnostic.cpp`
  - `tests/run_compile_fail.cmake`
  - `tests/runtime/error_diagnostic_smoke.cpp`
  - `tests/compile_pass/diagnostics_contract.cpp`
  - `tests/compile_pass/public_headers/core_diagnostics.cpp`
  - `include/pb/core/diagnostics.hpp`
- **Safe next slice:** add focused docs/test evidence only when it directly matches currently supported diagnostic behavior, and keep graph-aware emission, exported artifacts, cross-compiler wording parity, and schema-migration guarantees labeled as future work.

### 4. Package-consumer and release-readiness gates

- **Current repository evidence:** The docs now describe the package-consumer contract and the exact release-readiness command set that should be collected on a candidate SHA.
- **Current support level:** **Shipped release-scaffolding support, not a claim of production completeness.**
- **Proof points:**
  - `docs/production-readiness.md`
  - `docs/release-readiness-checklist.md`
  - `docs/build.md`
  - `continue.md` historical checkpoint guidance
- **Safe next slice:** refresh candidate-specific evidence on the chosen release SHA and record blockers instead of converting scaffolding into production claims.

### 5. Branch/join topology

- **Current repository evidence:** Full wave-1/2/3 fan-in surface: public branch/join DSL, sequential and thread-pool fan-in, fan-in multi-error envelope (`pb.fan_in_error_envelope` + `pb::collect_fan_in_errors`, schema `"pb.fan_in.errors.v1"`), fan-in observer lifecycle events (`on_fan_in_started` / `on_fan_in_case_scheduled` / `on_fan_in_case_completed` / `on_fan_in_completed` — additive v1-ABI-safe no-op virtuals), `pb::unique_clone<T, Clone>` owned per-case deep-copy policy, cooperative cancellation (`pb::cancellation_source` / `pb::cancellation_token`, schema `"pb.cancel.v1"`), and comprehensive tests and examples.
- **Current support level:** **Supported for homogeneous outputs, heterogeneous outputs through `std::variant`, selected-output type-list joins, explicit sequential fan-in, thread-pool fan-in scheduling with cooperative cancellation, borrowed move-only fan-in, move-only selected-branch input consumption, `pb::shared_view<T>` non-copyable fan-in inputs, `pb::unique_clone<T, Clone>` owned per-case cloning, `pb::projected` projection adapters, multi-error envelope, and fan-in lifecycle observer events.**
- **Proof points:**
  - `docs/branch-join-roadmap.md`
  - `include/pb/core/pipeline_state.hpp` (public `::branch<...>::join<...>` DSL)
  - `include/pb/runtime/sequential.hpp`
  - `include/pb/runtime/clone.hpp` (`pb::shared_view<T>`, `pb::unique_clone<T,Clone>`, `pb::projected`, introspection variable templates)
  - `include/pb/runtime/fan_in_error.hpp` (`pb::fan_in_error_envelope`, `pb::collect_fan_in_errors`, schema `"pb.fan_in.errors.v1"`)
  - `include/pb/runtime/cancellation.hpp` (`pb::cancellation_source`, `pb::cancellation_token`, schema `"pb.cancel.v1"`)
  - `include/pb/runtime/observer.hpp` (four `on_fan_in_*` virtuals)
  - `tests/runtime/fan_in_observer_events_smoke.cpp`
  - `tests/runtime/fan_in_unique_clone_smoke.cpp`
  - `tests/runtime/fan_in_error_envelope_smoke.cpp`
  - `tests/runtime/thread_pool_cancellation_smoke.cpp`
  - `tests/runtime/fan_in_shared_view_smoke.cpp`
  - `tests/runtime/projected_stage_smoke.cpp`
  - `examples/thread_pool_fan_in_demo.cpp`
- **Safe next slice:** keep preemptive (non-cooperative) cancellation, dependency backend fan-in, and broad backend branch execution as separate design/implementation phases.

### 6. Graph export

- **Current repository evidence:** DOT/JSON/text helper slices exist for linear and supported branch pipelines, including branch topology detection in JSON. `pb::export_text` ships alongside the JSON/DOT helpers. The export schema is backed by a typed contract (`docs/export-schema-v1.md`) with byte-equal regression tests.
- **Current support level:** **Partial descriptor-record-backed export helper support including `pb::export_text`; the `pb.core.graph.v1` typed contract governs the current helper output. The contract is typed, not yet a promised frozen external interchange schema.**
- **Proof points:**
  - `docs/graph-export-roadmap.md`
  - `docs/export-schema-v1.md`
  - `include/pb/core/export_text.hpp`
  - `tests/compile_pass/schema_v1_contract.cpp`
  - `tests/compile_pass/schema_v1_extended_golden.cpp`
  - `tests/compile_pass/export_text.cpp`, `tests/compile_pass/export_text_golden.cpp`
- **Safe next slice:** keep helper output aligned with the v1 contract; claim a frozen external schema only after stability promises, CLI/file export, and schema migration policy are established.

### 7. Observer hooks

- **Current repository evidence:** Runtime observer callbacks are shipped for the sequential executor plus the four additive fan-in lifecycle events (`on_fan_in_started`, `on_fan_in_case_scheduled`, `on_fan_in_case_completed`, `on_fan_in_completed`) on both sequential and thread-pool fan-in paths.
- **Current support level:** **Shipped for sequential runtime and fan-in lifecycle. v1-ABI-safe (no-op virtual defaults).**
- **Proof points:**
  - `docs/observer-hooks-roadmap.md`
  - `include/pb/runtime/observer.hpp`
  - `tests/runtime/sequential_observer_smoke.cpp`
  - `tests/runtime/fan_in_observer_events_smoke.cpp`
  - `tests/runtime/thread_pool_fan_in_stress_smoke.cpp`
- **Safe next slice:** add observer examples/benchmarks and harden public contract details (ABI/stability, lifecycle, ordering) before claiming full support.

### 8. Optional backends

- **Current repository evidence:** The repository supports the standard-library sequential path and the `thread_pool_backend` for fan-in. Optional backend integration seams for oneTBB, Taskflow, and stdexec exist as compile-guarded scaffolds under `include/pb/backends/{tbb,taskflow,stdexec}.hpp` — dormant by default; each exposes `*_backend_available_v == false` on the standard build. `PB_ENABLE_{TBB,TASKFLOW,STDEXEC}` CMake options are wired but off.
- **Current support level:** **Sequential + thread-pool fan-in slice supported. Optional backend seams are dormant scaffolds — working external backends are NOT implemented.**
- **Proof points:**
  - `docs/optional-backends-roadmap.md`
  - `include/pb/backends/tbb.hpp`, `include/pb/backends/taskflow.hpp`, `include/pb/backends/stdexec.hpp`
  - `tests/compile_pass/backend_scaffolds_smoke.cpp` (static-asserts all three `*_backend_available_v == false`)
  - `include/pb/runtime/thread_pool_backend.hpp`
- **Safe next slice:** activate a working backend behind `PB_ENABLE_TBB` only after `find_package(TBB)` integration, isolation tests, and matrix evidence are in place.

### 9. Stable runtime descriptor

- **Current repository evidence:** A narrow linear runtime-descriptor helper exists, alongside compile-time introspection and runtime diagnostic stage-identity checks. The `pb.core.graph.v1` schema-version string is backed by a typed contract.
- **Current support level:** **Partially shipped linear metadata support with a typed `pb.core.graph.v1` version contract; stable external export/compatibility guarantees remain roadmap-only.**
- **Proof points:**
  - `docs/runtime-descriptor-roadmap.md`
  - `docs/export-schema-v1.md`
  - `include/pb/core/describe.hpp`
  - `include/pb/runtime/descriptor.hpp`
  - `tests/compile_pass/schema_v1_contract.cpp`
  - `tests/compile_pass/schema_v1_extended_golden.cpp`
- **Safe next slice:** keep the current linear helper documented and tested at its real boundary; treat the v1 typed contract as a regression net rather than a frozen external promise until schema migration policy is documented.

### 10. Policy DSL (three axes)

- **Current repository evidence:** The `::with<>` DSL now carries and runtime-enforces three independent, order-insensitive policy axes: `errors` (five markers, selects wrapper via `compile<>`), `diagnostics` (two markers, `verbose` selects verbose wrapper), and `copying` (four markers, carried + queryable, runtime enforcement forward-looking). `pb::has_error_policy_v<P>`, `pb::has_diagnostics_policy_v<P>`, `pb::has_copying_policy_v<P>` expose compile-time queries.
- **Current support level:** **Shipped for errors and diagnostics axes (runtime-enforced via `compile<>`). Copying axis carried + queryable; runtime enforcement is forward-looking.**
- **Proof points:**
  - `include/pb/core/policy.hpp`
  - `include/pb/runtime/sequential.hpp` (4th Policies param + wrapper selection)
  - `tests/runtime/with_policy_dsl_runtime_smoke.cpp`
  - `tests/compile_pass/with_policy_dsl_compile_pass.cpp`
  - `tests/runtime/policy_axes_runtime_smoke.cpp`
  - `tests/compile_pass/policy_axes_compile_pass.cpp`
- **Safe next slice:** runtime enforcement of the copying axis and additional policy axes (executor capability, contracts) are future work.

### 11. Adapters (extended surface)

- **Current repository evidence:** Free-function, member-function, function-object/functor, `pb::runtime_callable<In,Out>` (type-erased runtime-bound stateful callable), `pb::bind_callable` (factory), `pb::c_function_stage<In,Out,Fn>` (C-style function-pointer non-type-template-param adapter), and the synchronous coroutine adapter (`pb::coro::sync_task<T>` + `pb::coroutine_stage<Stage>` / `pb::adapt_coroutine`).
- **Current support level:** **Shipped for all the above adapter shapes.**
- **Proof points:**
  - `include/pb/adapt/fn.hpp`
  - `include/pb/adapt/runtime_callable.hpp`
  - `include/pb/adapt/coroutine.hpp`
  - `tests/runtime/runtime_callable_adapter_smoke.cpp`
  - `tests/runtime/coroutine_adapter_smoke.cpp`
  - `tests/compile_pass/public_headers/adapt_runtime_callable.cpp`
  - `tests/compile_pass/public_headers/adapt_coroutine.cpp`
- **Safe next slice:** async/sender coroutine backends and working C++26 reflection-driven adapter remain future work.

### 12. C++20 named module

- **Current repository evidence:** `PB_BUILD_MODULE` CMake option builds `include/pb/pipeline.mpp` via `FILE_SET CXX_MODULES`; `modules-ninja` preset; `tests/module/use_module.cpp` does `import pb.pipeline;` and passes. Verified on clang 22 (llvm-mingw), CMake 4.3, Ninja. Default header build unaffected.
- **Current support level:** **Shipped for the documented preset on clang 22.**
- **Proof points:**
  - `docs/modules.md`
  - `CMakePresets.json` (`modules-ninja` preset)
  - `include/pb/pipeline.mpp`
  - `tests/module/use_module.cpp`
- **Safe next slice:** broader compiler/standard library compatibility evidence and install/export story for module consumers.

### 13. Benchmark evidence and performance budgets

- **Current repository evidence:** Benchmark lanes are documented as smoke/profiling scaffolding, not release thresholds. Branch/fan-in compile-time benchmark (`bench/compile_time/branch_fan_in_10.cpp`) and thread-pool fan-in runtime benchmark (`bench/runtime/thread_pool_fan_in.cpp`) exist.
- **Current support level:** **Shipped smoke scaffolding with roadmap-only performance governance.**
- **Proof points:**
  - `docs/build.md`
  - `docs/benchmark-workflow.md`
  - `docs/release-readiness-checklist.md`
  - `bench/compile_time/branch_fan_in_10.cpp`
  - `bench/runtime/thread_pool_fan_in.cpp`
- **Safe next slice:** collect reproducible benchmark evidence with context, but keep thresholds/regression budgets labeled as future work until explicitly established.

## Active missing-feature priority queue

This queue is the durable docs-lane view of the current missing-feature push. It is not a release claim: move an item to "supported" only after the implementation commit is integrated, tests pass on the candidate branch, and release-facing docs/examples are updated.

| Priority | Missing feature / evidence gap | Current docs-lane status | Promotion gate |
| --- | --- | --- | --- |
| 1 | Cross-compiler validation rerun on the integrated team head | **Not yet run.** 250/250 ctest on `clang-dev-ninja`; sender/receiver module export syntax probe passes, but `modules-ninja` was not refreshed locally because `clang-scan-deps` is missing. Last cross-compiler-validated SHA is `87299c14c813753d170911239e251064cbbfee6f`. | Rerun the GitHub Cross Compiler Validation workflow on the integrated team head; record GCC/Clang/MSVC/package results. |
| 2 | Runtime-enforced `::with<pb::policy::errors::...>` DSL | **Shipped.** `compile<P>(sequential{})` inspects the `Policies` type-list and wraps the engine in the matching error-policy wrapper; `pb::has_error_policy_v<P>` is queryable. Tests: `tests/runtime/with_policy_dsl_runtime_smoke.cpp`, `tests/compile_pass/with_policy_dsl_compile_pass.cpp`. | Promote when reflected in release-notes wording. |
| 3 | Diagnostics policy axis (`::with<diagnostics::verbose/quiet>`) | **Shipped.** `::with<diagnostics::verbose>` wraps the (possibly error-policy-wrapped) engine in `pb::with_verbose_diagnostics`. Tests: `tests/runtime/policy_axes_runtime_smoke.cpp`, `tests/compile_pass/policy_axes_compile_pass.cpp`. | Promote when reflected in release-notes wording. |
| 4 | Copying policy axis (`::with<copying::...>`) | **Shipped as carried + queryable.** Runtime enforcement of the copying axis beyond `pb::shared_view` / `pb::unique_clone` is forward-looking. | Add runtime enforcement; update docs. |
| 5 | Fan-in multi-error envelope | **Shipped.** `pb::fan_in_error_envelope` + `pb::collect_fan_in_errors`, schema `"pb.fan_in.errors.v1"`. | Promote when reflected in release-notes wording. |
| 6 | Fan-in observer lifecycle events | **Shipped.** Four additive v1-ABI-safe virtuals: `on_fan_in_started`, `on_fan_in_case_scheduled`, `on_fan_in_case_completed`, `on_fan_in_completed`. Emitted on sequential + thread-pool fan-in. | Promote when reflected in release-notes wording. |
| 7 | Cooperative cancellation | **Shipped.** `pb::cancellation_source` / `pb::cancellation_token`, schema `"pb.cancel.v1"`. Thread-pool fan-in checks the token before enqueueing each case. | Promote when reflected in release-notes wording. |
| 8 | `pb::unique_clone<T, Clone>` | **Shipped.** Owned per-case deep-copy fan-in policy. Header: `include/pb/runtime/clone.hpp`. Test: `tests/runtime/fan_in_unique_clone_smoke.cpp`. | Promote when reflected in release-notes wording. |
| 9 | Runtime-bound callable adapters | **Shipped.** `pb::runtime_callable<In,Out>` + `pb::bind_callable` + `pb::c_function_stage<In,Out,Fn>`. Header: `include/pb/adapt/runtime_callable.hpp`. | Promote when reflected in release-notes wording. |
| 10 | Synchronous coroutine + sender/receiver scaffolds | **Shipped.** `pb::coro::sync_task<T>` + `pb::coroutine_stage<Stage>` / `pb::adapt_coroutine`, plus dependency-free synchronous `pb::sync_just` / `pb::sync_sender_stage`. Headers: `include/pb/adapt/coroutine.hpp`, `include/pb/adapt/sender_receiver.hpp`. Full async/sender backends remain future work. | Promote when reflected in release-notes wording. |
| 11 | Optional backend integration seams | **Shipped as dormant compile-guarded scaffolds.** `include/pb/backends/{tbb,taskflow,stdexec}.hpp`; `*_backend_available_v == false` on default build. | Working backend promotion requires `find_package` integration, isolation tests, and matrix evidence. |
| 12 | C++20 named module build | **Shipped.** `PB_BUILD_MODULE` + `modules-ninja` preset + `tests/module/use_module.cpp` passing on clang 22. | Broader compiler compatibility evidence and install/export story. |
| 13 | `pb::tooling::pipeline_registry` + pb_cli refactor | **Shipped.** Five built-in pipelines; `pb::tooling::pipeline_registry` is the documented extension point. | Stable CLI contract for arbitrary user-defined pipelines. |
| 14 | Release/package evidence on candidate SHA | Current working tree at 250/250 ctest. Last cross-compiler-validated SHA has local full/package validation plus GitHub CI evidence. | Rerun package-release on the integrated team head and record archive path plus workflow URL. |
| 15 | Backend feature matrix | `sequential` and `thread_pool_backend` supported; oneTBB/Taskflow/stdexec dormant scaffolds only. | Working backend promotion requires activation, isolation tests, matrix evidence. |
| 16 | Full release compiler matrix | GitHub evidence exists for SHA `87299c14c813753d170911239e251064cbbfee6f`: GCC C++20/C++23, Clang C++20/C++23, MSVC C++20, and clean Ubuntu package-release passed 163/163. MSVC C++23, C++26 gates, and Windows package-release remain unclaimed. | Rerun on the integrated team head; record compiler IDs, preset, configure/build/test/package logs. |

## Current support summary

The current repository can safely claim:

- linear typed pipeline validation
- explicit stage metadata and compile-time introspection helpers
- sequential runtime execution for validated linear pipelines
- sequential branch execution with optional join stages, heterogeneous outputs through `std::variant`, explicit sequential fan-in with skipped/completed/failed slots, borrowed move-only fan-in, move-only selected-branch input consumption, and the standard-library `thread_pool_backend` fan-in scheduling slice with cooperative cancellation (`pb.cancel.v1`)
- `pb::shared_view<T>` copy-cheap shared-ptr-backed wrapper for non-copyable fan-in inputs, with `pb::fan_in_uses_copy_v` / `pb::fan_in_uses_borrow_v` introspection
- `pb::unique_clone<T, Clone>` owned per-case deep-copy fan-in policy
- `pb::projected<From, Projection, Stage>` projection adapter wrapping an existing stage with a user-supplied projection callable
- `pb::fan_in_error_envelope` + `pb::collect_fan_in_errors` — structured multi-error aggregation with stable `"pb.fan_in.errors.v1"` text schema
- four additive fan-in observer lifecycle events (`on_fan_in_started`, `on_fan_in_case_scheduled`, `on_fan_in_case_completed`, `on_fan_in_completed`) — v1-ABI-safe no-op virtual defaults
- cooperative cancellation (`pb::cancellation_source` / `pb::cancellation_token`, schema `"pb.cancel.v1"`); preemptive interruption is documented as out of scope
- error-policy DSL: five factory-built engine wrappers — `pb::with_throw_on_error`, `pb::with_terminate_on_error`, `pb::with_ignore_errors`, `pb::with_propagate_exceptions`, `pb::with_verbose_diagnostics`; runtime-enforced via `::with<pb::policy::errors::...>` on the sequential engine; `pb::has_error_policy_v<P>` is queryable
- `::with<pb::policy::diagnostics::verbose/quiet>` — diagnostics axis runtime-enforced; `pb::has_diagnostics_policy_v<P>` is queryable
- `::with<pb::policy::copying::value/move_only/shared/clone>` — copying axis carried + queryable; `pb::has_copying_policy_v<P>` is queryable; runtime enforcement is forward-looking
- runtime-bound stateful callable adapter: `pb::runtime_callable<In,Out>` / `pb::bind_callable` / `pb::c_function_stage<In,Out,Fn>` (header: `include/pb/adapt/runtime_callable.hpp`)
- synchronous coroutine stage adapter: `pb::coro::sync_task<T>` / `pb::coroutine_stage<Stage>` / `pb::adapt_coroutine` (header: `include/pb/adapt/coroutine.hpp`)
- `pb::tooling::pipeline_registry` name-keyed insertion-ordered registry + pb_cli refactored to use it; 5 built-in pipelines: `order-linear`, `order-branch`, `order-fan-in`, `order-enrich`, `order-variant`
- optional backend integration seams (dormant compile-guarded scaffolds): `include/pb/backends/{tbb,taskflow,stdexec}.hpp`; `*_backend_available_v == false` on default build
- C++20 named module build: `PB_BUILD_MODULE` option + `modules-ninja` preset + `tests/module/use_module.cpp` (`import pb.pipeline;`) passing on clang 22
- `pb.core.graph.v1` typed schema-version contract enforced by compile-time regression tests; `pb::export_text` text-format export helper
- typed C++26 / C++23 feature gate constants (`pb::features::has_cpp26`, `has_reflection`, `has_contracts`, `has_pack_indexing`, `has_std_expected`, `has_deducing_this`)
- reflection adapter scaffold in `include/pb/adapt/reflect.hpp` gated on `PB_HAS_REFLECTION` with a graceful C++20 fallback
- stateful storage DSL (`pb.state.v1`) including thread-local storage policy (`pb::with_thread_local_state<State>`)
- current working tree at **250/250 local ctest** on `clang-dev-ninja`; sender/receiver module export syntax probe passes; `modules-ninja` was not refreshed locally because `clang-scan-deps` is missing; cross-compiler validation on the new SHA pending
- release-readiness documentation plus GitHub GCC/Clang/MSVC/package validation evidence and local package evidence for code SHA `87299c14c813753d170911239e251064cbbfee6f`

The current repository should **not** claim:

- production-complete topology or execution coverage
- working oneTBB, Taskflow, or stdexec backends — the scaffold seams are dormant
- preemptive (non-cooperative) cancellation
- runtime enforcement of the copying policy axis beyond `pb::shared_view` / `pb::unique_clone`
- full async/sender coroutine backends (synchronous `pb::coro::sync_task<T>` and the dependency-free `pb::sync_sender_stage` scaffold are shipped; async backends are future work)
- a frozen external descriptor/export schema or CLI/file export for arbitrary user pipelines
- benchmark thresholds or CI-enforced performance budgets
- MSVC C++23, C++26 feature implementation, Windows package-release, or package-manager ecosystem validation
- C++26 reflection as supported — the adapter scaffold is gated and falls back on C++20
- a published release or tagged version

## How to use this map in the docs lane

When adding a new status page or release-facing note:

1. Start by finding the theme above.
2. Reuse only the proof points that already exist in the tree.
3. Mark any missing capability as roadmap-only unless code, tests, and examples all support the claim.
4. Prefer one narrow page or one narrow link update over broad cross-doc churn.

That keeps the documentation truthful while the repo continues moving from MVP scaffolding toward wider roadmap coverage.

## Next long-horizon queue

Use these as the first finite batches when the next 3-agent team resumes:

1. **Core API / diagnostics:** improve one small compile-time diagnostics or metadata ergonomics gap, then add compile-pass/compile-fail evidence.
2. **Runtime / adapters:** activate and test a working optional backend (TBB recommended first — the `PB_ENABLE_TBB` CMake hook is already wired) with isolation tests and a matrix evidence run.
3. **Updater / docs:** rerun cross-compiler validation on the integrated team head and update `docs/cross-compiler-validation.md` and the evidence in `research/not done.md`.

The next safe work should continue from shipped MVP surfaces. Preemptive cancellation, working dependency-backend execution beyond the thread-pool slice, runtime enforcement of the copying policy axis, full async/sender coroutine backends beyond the synchronous scaffold, and stable descriptor/export compatibility remain separate design/implementation phases, not opportunistic follow-ups inside a routine hardening batch.
