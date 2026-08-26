# Current implementation map against `pipeline_builder_cpp_research_plan.md`

This file tracks what is done, partially done, still missing, and what must be finished before Pipeline-c++ can claim the full original research-plan vision.

Updated after the wave-1/2/3 batch and team hardening that add `pb::unique_clone`, thread-local state policy, fan-in observer lifecycle events, fan-in multi-error envelope (`pb.fan_in.errors.v1`), synchronous coroutine and sender/receiver scaffolds, `pb::tooling::pipeline_registry` + pb_cli refactor, runtime-enforced `::with<pb::policy::errors::...>` DSL on the sequential engine, cooperative cancellation (`pb.cancel.v1`), diagnostics and copying policy axes, `pb::runtime_callable` / `pb::bind_callable` / `pb::c_function_stage` adapters, optional backend integration seams (TBB/Taskflow/stdexec — compile-guarded scaffolds, dormant by default), and the C++20 named module build (`PB_BUILD_MODULE` + `modules-ninja` preset + `import pb.pipeline;`).

## Latest evidence snapshot

Current local evidence for the integrated worker-1/3/4 team head (wave-1/2/3 plus post-wave diagnostics/batch-run/export optimization, sender/receiver scaffold, and fan-in observer lifecycle hardening commits integrated):

```text
- cmake --preset clang-dev-ninja: passed
- cmake --build --preset clang-dev-ninja: passed
- ctest --preset clang-dev-ninja --output-on-failure: passed, 250/250
- include/pb/pipeline.mpp syntax probe after sender/receiver module export update: passed (`clang++ -std=c++20 -I include -fsyntax-only -x c++ include/pb/pipeline.mpp`)
- modules-ninja / pb_use_module refresh: not run in this environment because `clang-scan-deps` is missing
```

The previous validated code SHA `87299c14c813753d170911239e251064cbbfee6f` still has fresh local + GitHub evidence:

```text
- ctest --preset clang-dev-ninja --output-on-failure: passed, 163/163
- ctest --preset package-release-clang-ninja --output-on-failure: passed, 163/163
- cmake --build --preset package-release-clang-ninja --target package: passed
- package artifact: build/package-release-clang-ninja/pipebuilder-0.1.0-Linux.tar.gz
```

Latest GitHub validation for that SHA (still current as the canonical cross-compiler reference until the new SHA is revalidated):

```text
- workflow: Cross Compiler Validation
- run: https://github.com/tonytranrp/Pipeline-c-/actions/runs/26145779030
- validated SHA: 87299c14c813753d170911239e251064cbbfee6f
- GCC C++20/C++23: passed, 163/163
- Clang C++20/C++23: passed, 163/163
- MSVC C++20: passed, 163/163
- clean Ubuntu package-release-clang-ninja: passed, 163/163 plus TGZ package generation
```

Normal CI also passed on the same SHA: <https://github.com/tonytranrp/Pipeline-c-/actions/runs/26145765932>.

## Done / production-grade for the current supported scope

These areas are implemented, tested, documented, and safe to present as supported when the release evidence is fresh.

| Area | Current status | Evidence / boundary |
| --- | --- | --- |
| CMake/package skeleton | Done for the current package shape. | `pb::core`, `pb::runtime`, and `pb::pipeline` install/export targets exist; package smoke validates downstream consumers and TGZ contents. |
| C++20 baseline | Done. | Target-based CMake requires C++20 and disables extensions. C++23 is validated in GitHub lanes, but C++20 remains the baseline. |
| Linear typed DSL | Done. | `pb::from<T>::then<S>::to<U>` and `::to<U>` are implemented with compile-pass and compile-fail coverage. |
| Stage traits and metadata | Done for current traits. | `input_type`, `output_type`, optional `error_type`, names, keys, public aliases, and descriptor helpers are covered. |
| Indexed pipeline I/O aliases | Done. | `pb::pipeline_stage_input_t<P, N>` and `pb::pipeline_stage_output_t<P, N>` ship with named out-of-range `static_assert` diagnostics; compile-pass + two compile-fail tests cover the alias and OOB boundary. |
| Linear compile-time validation | Done for current linear graph. | Invalid stages, adjacent edge mismatch, sink mismatch, and public-header coverage are tested. |
| Sequential runtime | Done for validated linear pipelines. | `pb::compile<Pipeline>(pb::runtime::sequential{})`, `run()`, `try_run()`, move-only linear paths, and zero-stage identity behavior are tested. |
| Result / expected-like plumbing | Done for current supported behavior. | `pb::runtime::result`, expected-like normalization, stage failure propagation, custom error conversion, and exception capture boundaries are tested. |
| Error-policy DSL (wrapper layer) | Done — parity-proven, composable, and runtime-enforced via `::with<>`. | Five factory-built engine wrappers under `include/pb/runtime/error_policy.hpp`: `pb::with_throw_on_error` → `throwing_engine`, `pb::with_terminate_on_error` → `terminating_engine`, `pb::with_ignore_errors` → `ignoring_engine`, `pb::with_propagate_exceptions` → `propagating_engine`, `pb::with_verbose_diagnostics` → `verbose_engine`. Runtime-enforced: `pipeline` gained a 4th `Policies` type-list param; `pb::compile<P>(sequential{})` inspects it and wraps the engine in the matching error policy when `::with<errors::throwing>` (etc.) is present. `pb::has_error_policy_v<P>` queries the finalized pipeline. Tests: `tests/runtime/with_policy_dsl_runtime_smoke.cpp`, `tests/compile_pass/with_policy_dsl_compile_pass.cpp`, plus existing parity-matrix and composability lanes. |
| `::with<>` diagnostics policy axis | Done. | `pb::policy::diagnostics::{verbose,quiet}` markers carried + enforced: `::with<diagnostics::verbose>` makes `compile<>` wrap the engine in `pb::with_verbose_diagnostics`. `pb::has_diagnostics_policy_v<P>`. Tests: `tests/runtime/policy_axes_runtime_smoke.cpp`, `tests/compile_pass/policy_axes_compile_pass.cpp`. |
| `::with<>` copying policy axis | Done (carried + queryable; runtime enforcement is forward-looking). | `pb::policy::copying::{value,move_only,shared,clone}` markers threaded into the finalized pipeline's `policies` type-list; `pb::has_copying_policy_v<P>` / `pb::pipeline_copying_policy_t<P>`. Tests: same policy-axes lanes. |
| Observer hooks for sequential runtime | Done for current sequential hooks plus fan-in lifecycle events. | Stage start/success/failure/exception, branch case events, and the four additive fan-in lifecycle virtuals (`on_fan_in_started`, `on_fan_in_case_scheduled`, `on_fan_in_case_completed`, `on_fan_in_completed`) are v1-ABI-safe no-op defaults in `include/pb/runtime/observer.hpp`. Emitted on both sequential and thread-pool fan-in paths. |
| User-code adapters | Done for current adapter shapes plus runtime-bound and C-style adapters. | Free-function, member-function, function-object/functor adapters + `pb::runtime_callable<In,Out>` (owns a runtime callable / stateful lambda) + `pb::bind_callable` factory + `pb::c_function_stage<In,Out,Fn>` (C-style function-pointer non-type-template-param adapter). Headers: `include/pb/adapt/fn.hpp`, `include/pb/adapt/runtime_callable.hpp`. Tests: `tests/runtime/runtime_callable_adapter_smoke.cpp`, `tests/compile_pass/public_headers/adapt_runtime_callable.cpp`. |
| Coroutine/sender scaffolds | Done for synchronous dependency-free paths. | `pb::coro::sync_task<T>` eager synchronous task + `pb::coroutine_stage<Stage>` / `pb::adapt_coroutine(stage)` adapter, plus `pb::sync_just` / `pb::sync_sender_stage` for one-value synchronous sender-like factories. Full async/sender backends remain future work. Headers: `include/pb/adapt/coroutine.hpp`, `include/pb/adapt/sender_receiver.hpp`. Tests: `tests/runtime/coroutine_adapter_smoke.cpp`, `tests/compile_pass/public_headers/adapt_coroutine.cpp`, `tests/runtime/sender_receiver_adapter_smoke.cpp`, `tests/compile_pass/public_headers/adapt_sender_receiver.cpp`. |
| Branch / selected-output join surface | Done for the supported sequential slice. | Public `::branch<...>` and `::join<...>`, homogeneous outputs, `std::variant` heterogeneous outputs, duplicate variant alternatives by index, selected-output type-list joins, observer case events, stateful storage, move-only selected-branch input consumption, examples, and tests are present. |
| Fan-in join surface | Done for sequential plus thread-pool backend slice. | `::fan_in<JoinStage>` / `::join_all<JoinStage>`, all-predicate evaluation, declaration-order aggregation, zero/one/many passing cases, duplicate same-type outputs by index, stateful storage, void-output cases, borrowed move-only fan-in, thread-pool parallel case execution, deterministic aggregate ordering, synchronized observer forwarding, fan-in lifecycle events. |
| Fan-in clone / projection policies | Done. | `pb::shared_view<T>` (shared-ptr-backed copyable wrapper) + `pb::make_shared_view`; `pb::projected<From, Projection, Stage>` projection adapter; `pb::unique_clone<T, Clone>` owned-per-case deep-copy wrapper (closing the "owned per-case unique-clone beyond shared-view" gap); `pb::fan_in_uses_copy_v` / `pb::fan_in_uses_borrow_v`. Header: `include/pb/runtime/clone.hpp`. Tests: `tests/runtime/fan_in_shared_view_smoke.cpp`, `tests/runtime/projected_stage_smoke.cpp`, `tests/runtime/fan_in_unique_clone_smoke.cpp`, `tests/compile_pass/public_headers/runtime_clone_unique.cpp`. |
| Fan-in multi-error envelope | Done. | `pb::fan_in_error_envelope` + `pb::collect_fan_in_errors` — structured aggregation of every failed fan-in case with stable schema `"pb.fan_in.errors.v1"`. Header: `include/pb/runtime/fan_in_error.hpp`. Tests: `tests/runtime/fan_in_error_envelope_smoke.cpp`, `tests/compile_pass/public_headers/runtime_fan_in_error.cpp`. |
| Thread-local stateful state policy | Done. | `pb::with_thread_local_state<State>` — thread-local storage policy for stateful stages under concurrent invocation. Header: `include/pb/runtime/state.hpp`. Test: `tests/runtime/state_thread_local_smoke.cpp`. |
| Cooperative cancellation | Done (cooperative, not preemptive). | `pb::cancellation_source` / `pb::cancellation_token` over `std::atomic<bool>` (lock-free, acquire/release). Schema: `"pb.cancel.v1"`. Thread-pool fan-in backend checks the token before enqueueing each case; not-yet-started cases are skipped and reported via `on_case_skipped`. Preemptive interruption of running stages is documented as out of scope. Header: `include/pb/runtime/cancellation.hpp`. Tests: `tests/runtime/thread_pool_cancellation_smoke.cpp`, `tests/compile_pass/public_headers/runtime_cancellation.cpp`. |
| `pb::tooling::pipeline_registry` + pb_cli refactor | Done. | Name-keyed, insertion-ordered registry; 3 original built-ins re-registered byte-identically (goldens stay green) + 2 new pipelines (`order-enrich`, `order-variant`) added. Header: `include/pb/tooling/cli_registry.hpp`. Tests: `tests/run_pb_cli_list.cmake`, new list/describe tests. |
| Optional backend integration seams | Done as dormant compile-guarded scaffolds. | `include/pb/backends/{tbb,taskflow,stdexec}.hpp` — fully `#if PB_HAS_*`-guarded, dormant on the default build; each exposes an ungated `*_backend_available_v` (false by default). `PB_ENABLE_{TBB,TASKFLOW,STDEXEC}` CMake options (default OFF; `find_package` + `PB_HAS_*` when ON). Test: `tests/compile_pass/backend_scaffolds_smoke.cpp` static-asserts the seam is dormant. Working oneTBB/Taskflow/stdexec backends remain roadmap-only. |
| C++20 named module build | Done (verified on clang 22 llvm-mingw, CMake 4.3, Ninja). | `PB_BUILD_MODULE` option builds `include/pb/pipeline.mpp` via `FILE_SET CXX_MODULES`; `modules-ninja` preset; `tests/module/use_module.cpp` does `import pb.pipeline;` and passes as `pb_use_module`. Default header build unaffected. Docs: `docs/modules.md`. |
| Branch case identity metadata | Done for helper/export preparation. | Descriptor branch case records carry deterministic case and node identities plus labels. Runtime fallback identities exist for unnamed branch children. |
| `pb.core.graph.v1` typed schema contract | Done as a regression-locked typed contract. | The literal `"pb.core.graph.v1"` identifier is enforced at compile time by `tests/compile_pass/schema_v1_contract.cpp`; byte-equal helper output for linear, branch, and fan-in pipelines is locked in `tests/compile_pass/schema_v1_extended_golden.cpp`. Formal spec: `docs/export-schema-v1.md`. |
| DOT / JSON / text helper export | Done as a helper surface for supported shapes. | Linear, selected-output branch, and explicit fan-in helper output uses `schema_version = "pb.core.graph.v1"`. `pb::export_text` helper ships alongside JSON/DOT helpers. Golden tests present. |
| `pb_cli` introspection CLI | Done as a built-in introspection surface; broader CLI contract still future work. | `pb_cli list`, `pb_cli describe <name> --format=<dot|json|text> [--out=<path>]`, and `pb_cli features` are built into every preset. Five built-in pipelines ship: `order-linear`, `order-branch`, `order-fan-in`, `order-enrich`, `order-variant`. |
| Typed C++26 / C++23 feature gates | Done as forward-compat gate constants with a regression-tested fallback. | `pb::features::has_cpp26`, `has_reflection`, `has_contracts`, `has_pack_indexing`, `has_std_expected`, `has_deducing_this` are `inline constexpr bool` constants. Header: `include/pb/core/cpp26_features.hpp`. Docs: `docs/cpp26-feature-gates.md`. |
| Reflection adapter scaffold | Done as a gated forward-compat seam — not a working reflection feature yet. | `pb::reflect_stage<T>` is declared on every compiler; when `PB_HAS_REFLECTION == 1` it emits a working adapter, on C++20 baselines instantiating it produces a named `static_assert`. `pb::reflect_stage_available_v<T>` mirrors the macro. Header: `include/pb/adapt/reflect.hpp`. |
| Compile-time benchmark smoke scaffolding | Done as smoke/build evidence. | Header inclusion, 5-stage chain, 50-stage chain, branch/fan-in compile-time benchmark (`bench/compile_time/branch_fan_in_10.cpp`), and thread-pool fan-in runtime benchmark (`bench/runtime/thread_pool_fan_in.cpp`) exist. Timing budgets are not established. |
| Release/package verification scaffolding | Done for the previously validated candidate. | Package release preset, package smoke, GitHub cross-compiler workflow, release docs, evidence templates, and exact code-SHA GitHub matrix evidence exist for SHA `87299c14c813753d170911239e251064cbbfee6f`. Cross-compiler validation has **not** been rerun on the new working-tree SHA. Publication still pending. |
| Stateful storage DSL (`pb.state.v1`) | Done. | `pb::with_state<S>` / `pb::with_borrowed_state<S>` / `pb::with_shared_state<S>` / `pb::with_reset_per_run_state<S>` plus `pb::with_thread_local_state<State>`. Thread-local `pb::current_state<S>()` / `pb::try_current_state<S>()`. Composes with every error-policy wrapper; exception-safe via `pb::state_context<S>` RAII frame. Header: `include/pb/runtime/state.hpp`. Docs: `docs/state-dsl.md`. Tests: `tests/runtime/state_dsl_smoke.cpp`, `tests/runtime/state_thread_local_smoke.cpp`. Schema-pinned in `tests/compile_pass/schema_v1_cross_version_regression.cpp`. |

### Documentation status by product surface

These are the docs that should be treated as current after the wave-1/2/3 batch.

| Documentation area | Current truth to preserve | Pages to keep synchronized |
| --- | --- | --- |
| Branch/join and fan-in | Selected-output branch/join, explicit fan-in, clone/projection policies (`pb::shared_view`, `pb::projected`, `pb::unique_clone`), fan-in multi-error envelope, and fan-in observer lifecycle events are supported for the documented sequential and thread-pool fan-in slices; dependency backends, preemptive cancellation, and richer error/cancellation policies are still roadmap-only. | `docs/branch-join-roadmap.md`, `docs/sequential-branch-execution-limitations.md`, `docs/fan-in-join-design.md`, this file |
| Backend support | `sequential` is the baseline backend and `thread_pool_backend` is supported only for the standard-library fan-in slice; optional backend seams for oneTBB, Taskflow, and stdexec exist as compile-guarded scaffolds (`include/pb/backends/*.hpp`) but are dormant by default — working external backends are not implemented. | `docs/optional-backends-roadmap.md`, `docs/research-verification-matrix.md`, `docs/roadmap-gap-map.md` |
| Error-policy DSL | Five-wrapper engine-side DSL is shipped, parity-hardened, composable, and runtime-enforced via `::with<pb::policy::errors::...>` on the sequential engine (`pb::has_error_policy_v`). Diagnostics axis (`::with<diagnostics::verbose/quiet>`) and copying axis (`::with<copying::value/move_only/shared/clone>`) also shipped. | `README.md` "Production-grade extras" section, `include/pb/runtime/error_policy.hpp`, `include/pb/core/policy.hpp`, `docs/production-readiness.md`, `docs/roadmap-gap-map.md`, this file |
| Adapters | Free-function, member-function, function-object/functor, `pb::runtime_callable` (runtime-bound stateful callable), `pb::c_function_stage` (C-style function-pointer), and `pb::coroutine_stage` / `pb::adapt_coroutine` (synchronous coroutine adapter) are shipped. | `README.md`, `include/pb/adapt/`, `docs/production-readiness.md`, this file |
| Descriptor/export helpers | DOT / JSON / `pb::export_text` helpers cover linear, selected-output branch, and explicit fan-in pipelines; the `pb.core.graph.v1` typed contract anchors the schema-version string and byte-equal field layout, but a frozen external interchange schema remains roadmap-only. | `docs/export-schema-v1.md`, `docs/graph-export-roadmap.md`, `docs/export-helper-schema.md`, `docs/runtime-descriptor-roadmap.md` |
| C++26 feature gates | Typed `pb::features::*` `constexpr bool` constants ship and resolve to `false` on C++20; reflection adapter scaffold is gated and falls back gracefully. C++26 reflection is **not** supported on current toolchains. | `docs/cpp26-feature-gates.md`, `include/pb/core/cpp26_features.hpp`, `include/pb/adapt/reflect.hpp`, this file |
| `pb_cli` introspection | `list` / `describe` / `features` ship with 5 built-in pipelines (`order-linear`, `order-branch`, `order-fan-in`, `order-enrich`, `order-variant`); `pb::tooling::pipeline_registry` is the extension point for user forks; a broader stable CLI contract for arbitrary user pipelines remains future work. | `README.md` "CLI: `pb_cli`" section, `tools/pb_cli.cpp`, `include/pb/tooling/cli_registry.hpp`, `tests/run_pb_cli_describe.cmake` |
| C++20 named module | `PB_BUILD_MODULE` + `modules-ninja` preset + `import pb.pipeline;` verified on clang 22; default header build unaffected. | `docs/modules.md`, `CMakePresets.json`, `include/pb/pipeline.mpp` |
| Release evidence | Exact-SHA local, CI, cross-compiler, and package evidence exists for code SHA `87299c14c813753d170911239e251064cbbfee6f`. Working tree is **250/250 local ctest**; cross-compiler validation on the new SHA is pending. | `docs/cross-compiler-validation.md`, `docs/current-release-summary.md`, `docs/production-readiness.md`, `docs/research-verification-matrix.md` |
| Remaining production gaps | Full async/sender coroutine backends beyond the synchronous scaffold, working external TBB/Taskflow/stdexec backends, frozen external descriptor/export schema, benchmark budgets, C++26 reflection-and-contracts integration, and release publication remain incomplete. | `docs/roadmap-gap-map.md`, this file, release notes |

## Implemented but still partial / almost production-grade

These areas work in meaningful slices, but should not be described as fully production-grade until the missing items in the right column are finished.

| Area | Implemented now | Missing before maximum / production grade |
| --- | --- | --- |
| Branch/join execution | All wave-1/2/3 fan-in features shipped: selected-output branch, sequential + thread-pool fan-in, `pb::shared_view` / `pb::projected` / `pb::unique_clone` clone policies, multi-error envelope (`pb.fan_in.errors.v1`), fan-in observer lifecycle events, cooperative cancellation (`pb.cancel.v1`). | oneTBB/Taskflow/stdexec backend branch execution; preemptive (non-cooperative) cancellation; backend examples/benchmarks; richer backend fan-in policy knobs; full backend branch matrix tests. |
| Heterogeneous branch outputs | `std::variant` preserves duplicate alternatives by index; type-list selected-output joins dispatch by C++ type. Sequential fan-in distinguishes duplicate same-type case outputs by case index in `fan_in_case_result<I, T>`. | Tagged case-result model for selected-output joins if needed; backend/fan-in error-envelope policies; docs/examples for all future policies. |
| Move-only branch input | Sequential and thread-pool fan-in support move-only inputs via borrow or `pb::shared_view<T>`. `pb::unique_clone<T, Clone>` provides owned per-case deep-copy for types that do not support shared-ptr semantics. | Deeper resource lifetime examples; richer observer/error edge cases. |
| Runtime descriptor/export | Runtime descriptor records and helper export fields cover current linear, selected-output branch, and explicit fan-in helper output with the `pb.core.graph.v1` typed contract and byte-equal goldens. | Stable versioned external schema, compatibility rules, ownership/lifetime guarantees, schema-migration policy (v1 → v2), and a documented release-grade descriptor/export API. |
| DOT / JSON / text export | Helper output is golden-tested and documented. `pb::export_text` ships. | Long-term schema compatibility promise, broader CLI/file export contract for arbitrary user pipelines, backend scheduling/trace export, schema migration rules, and published compatibility fixtures. |
| Runtime error model | `try_run()` captures supported failures; runtime-enforced `::with<pb::policy::errors::...>` DSL makes `compile<>` select the matching wrapper; all three policy axes (errors, diagnostics, copying) are carried + queryable. | Serialized error records, unified `run()` / `try_run()` policy narrative docs that distinguish wrapper behaviour from baseline-engine semantics, no-exception policy. |
| Diagnostics | Compile-fail tests cover important current structural errors; runtime errors carry stage identity. `pb.diagnostics.v1` now ships a narrow machine-readable `diagnostic_record` / `diagnostic_collector` contract plus initial suggested-fix constants. Compile-fail diagnostic goldens expanded in the release-readiness pack. | Frozen diagnostic wording/schema migration policy, exported diagnostic artifacts, cross-compiler diagnostic parity, and full branch/fan-in/backend diagnostic matrix. |
| Observer/tracing | Sequential observer hooks, fan-in lifecycle events, trace-related smoke coverage exist; the policy-DSL `verbose_engine` auto-attaches a `verbose_observer` with stable `[pb.verbose] <event> stage=<key>` line schema. | Stable observer ABI/event schema, observer ownership/lifetime contract, tracing sink API, trace file format, cross-executor behavior, and observer/trace overhead benchmarks. |
| Adapters | Free-function, member-function, function-object/functor, `pb::runtime_callable` (runtime-bound stateful callable), `pb::c_function_stage` (C-style function-pointer), coroutine stage adapter (`pb::coro::sync_task<T>` / `pb::coroutine_stage` / `pb::adapt_coroutine`), dependency-free synchronous sender/receiver scaffold (`pb::sync_just` / `pb::sync_sender_stage`), and gated `pb::reflect_stage<T>` scaffold are covered. | Working C++26 reflection-driven adapter (currently scaffold only, falls back on C++20); full async/sender coroutine backends; full overloaded/ref-qualified member handling; reference-lifetime adapter; policy docs. |
| Compile-time performance | Smoke targets prove representative translation units build; branch/fan-in compile-time benchmark target (`bench/compile_time/branch_fan_in_10.cpp`) exists; thread-pool fan-in runtime benchmark (`bench/runtime/thread_pool_fan_in.cpp`) exists. CMake has time-trace support. | Recorded release timing baselines, thresholds, CI regression budget, ftime-trace aggregation, compile-time dashboard, and IWYU enforcement. |
| Cross-compiler validation | GitHub workflow covers GCC/Clang C++20/C++23, MSVC C++20, and clean Ubuntu package-release for validated code SHA `87299c14c813753d170911239e251064cbbfee6f`. | Rerun the workflow on the new working-tree SHA; MSVC C++23 if supported; Windows package-release; experimental C++26 feature-gate evidence if those gates are claimed. |
| Optional backends | Compile-guarded seams for TBB, Taskflow, stdexec exist as dormant scaffolds (`include/pb/backends/*.hpp`); `*_backend_available_v` is always `false` on the default build. | Working oneTBB / Taskflow / stdexec backends with real `find_package` integration, isolation tests, examples, and benchmarks. |

## Not implemented yet / roadmap-only

These items remain outside the supported product surface.

### 1. Fan-in / all-branches joins beyond the sequential and thread-pool slices

Sequential + thread-pool fan-in is shipped and comprehensive. The full research-plan backend fan-in story is still incomplete.

Still need to implement:

```text
- preemptive (non-cooperative) cancellation policy for already-running backend case stages
- backend scheduling/trace export semantics rather than helper graph topology only
- backend examples, compile-fail coverage for policy conflicts, runtime stress tests beyond the current smoke, docs, and benchmark coverage
```

### 2. Working optional pipeline execution backends

The standard-library sequential executor and `pb::runtime::thread_pool_backend` (fan-in only) are supported. Compile-guarded seams for oneTBB, Taskflow, and stdexec exist as dormant scaffolds — no working external backends ship.

Need to implement next:

```text
- working oneTBB backend (PB_ENABLE_TBB + find_package(TBB) is wired in CMake but dormant)
- working Taskflow backend (PB_ENABLE_TASKFLOW + find_package(Taskflow) is wired in CMake but dormant)
- working stdexec / sender-receiver experimental backend (PB_ENABLE_STDEXEC wired but dormant)
- backend examples and benchmarks
- dependency isolation target tests
```

### 3. Full policy DSL axes beyond the shipped axes

The three runtime-enforced axes are shipped:
- `errors`: `throwing` / `terminating` / `ignoring` / `propagating` / `result`
- `diagnostics`: `verbose` / `quiet`
- `copying`: `value` / `move_only` / `shared` / `clone` (carried + queryable; runtime enforcement is forward-looking)

Need to implement:

```text
- runtime enforcement of the copying axis beyond existing shared_view/unique_clone fan-in strategies
- runtime reference lifetime/shared-view policy
- executor capability policy
- assertion/contract policy
- policy conflict diagnostics
```

### 4. Stable external graph/export contract

The typed `pb.core.graph.v1` schema-version contract pins the literal identifier and byte-equal helper output via compile-time regression tests; `pb::export_text` ships alongside JSON/DOT. This is still a typed regression net, not yet a frozen external interchange format.

Need to implement:

```text
- frozen v1 stability promise (no field removal without major bump)
- schema migration policy (v1 → v2)
- compatibility tests that fail on unintended schema changes
- broader stable descriptor-backed export API
- CLI/file export contract for arbitrary user pipelines (current pb_cli only handles built-in examples)
- examples for linear, selected-output branch, and fan-in graphs
- unsupported-shape diagnostics
```

### 5. Graph visualization CLI for arbitrary user pipelines

`pb_cli` ships built-in `list` / `describe <name> --format=<dot|json|text> [--out=<path>]` / `features` for five built-in pipelines (`order-linear`, `order-branch`, `order-fan-in`, `order-enrich`, `order-variant`) with `pb::tooling::pipeline_registry` as the documented extension point for user forks. The full research-plan CLI for arbitrary user pipelines is still future work.

Need to implement:

```text
- documented stable CLI command surface for arbitrary user-defined pipelines
- filtered graph view if desired
- broader CLI tests beyond pb_cli describe driver coverage
- CLI docs/examples
```

### 6. Async / sender coroutine backends

The synchronous `pb::coro::sync_task<T>` / `pb::coroutine_stage` adapter is shipped for the sequential engine. The async/sender coroutine backend direction remains future work.

Need to implement later:

```text
- suspending awaiters resumed by a scheduler (stdexec / sender-receiver)
- async coroutine stage adapter for parallel backends
```

### 7. C++26 experimental features

The C++20 path is canonical. Typed `pb::features::*` gate constants now ship; the `pb::reflect_stage<T>` scaffold is gated and falls back to a clear `static_assert` on C++20. No reflection-powered functionality ships until a C++26 reflection toolchain baseline is established.

Need to implement later:

```text
- working reflection-driven adapter on a real C++26 reflection toolchain
- contracts integration once compiler support exists
- pack-indexing optimization paths
- richer static_assert-message integration
- broader compiler/library detection tests beyond the current fallback test
- release-grade C++26 feature-gate validation in CI
```

### 8. Release publication

Release machinery and local/package evidence exist, but a public release is not published.

Need to finish:

```text
- decide final release candidate SHA (current integrated team head, 250/250 ctest)
- rerun cross-compiler validation on that SHA
- update release notes with exact supported/unsupported boundaries
- tag v0.1.0
- publish GitHub release
- attach package/evidence as appropriate
```

## Production-grade priority order

Best next steps from lowest risk / highest production-readiness value to highest complexity:

```text
1. Rerun cross-compiler validation on the integrated team head (250/250 ctest, all wave-1/2/3 plus post-wave API/docs features).
2. Broaden the pb_cli surface to accept user-defined pipelines now that pb::tooling::pipeline_registry provides the extension point.
3. Add backend examples/benchmarks for the thread-pool fan-in backend.
4. Add working oneTBB / Taskflow / stdexec backends once the dormant scaffold is tested.
5. Establish recorded release timing baselines and CI regression budget for the compile-time benchmarks.
6. Add runtime enforcement of the copying policy axis beyond shared_view/unique_clone.
7. Publish v0.1.0 when release notes, exact-SHA validation, package artifact, and unsupported-boundary docs all agree.
```

## Current scorecard

```text
Linear MVP foundation:                    9.0 / 10
Sequential runtime MVP:                   8.5 / 10  (was 8.0 — runtime-enforced ::with<> DSL closes the marker-only gap)
Result / error current surface:           9.0 / 10  (was 8.6 — runtime-enforced error-policy DSL + fan-in multi-error envelope)
Adapters current surface:                 8.5 / 10  (was 7.6 — runtime_callable + c_function_stage + coroutine adapter added)
Diagnostics current surface:              8.0 / 10  (was 7.8 — diagnostics policy axis + verbose compose)
Selected-output branch/join execution:    8.6 / 10
Fan-in join execution:                    9.5 / 10  (was 9.0 — unique_clone + multi-error envelope + lifecycle events + cooperative cancellation)
Move-only selected-branch input support:  8.2 / 10  (was 7.8 — unique_clone closes the owned per-case gap)
Stateful branch storage:                  9.2 / 10  (was 9.0 — thread-local state policy added)
DOT / JSON / text helper export:          8.8 / 10
Stable descriptor/export contract:        5.5 / 10
Compile-time benchmark scaffolding:       7.5 / 10  (was 7.2 — branch/fan-in + thread-pool fan-in bench targets added)
Optional pipeline backends:               5.0 / 10  (was 4.2 — dormant scaffolds for TBB/Taskflow/stdexec added)
C++26 feature gates and reflection:       5.0 / 10
C++20 named module:                       7.0 / 10  (prior modules-ninja evidence exists; current sender/receiver module export syntax-probed, full preset refresh blocked by missing clang-scan-deps)
Policy DSL (full three axes):             8.0 / 10  (new combined score — errors + diagnostics + copying axes shipped)
Cooperative cancellation:                 7.5 / 10  (new — pb.cancel.v1 cooperative token + thread-pool integration)
Release readiness before tag:             7.5 / 10  (was 8.2 — new SHA not yet cross-compiler-validated; ctest 250/250)
Full original research-plan completion:   8.5 / 10  (was 7.8 — wave-1/2/3 close many major gaps)
```

## Main conclusion

Pipeline-c++ now has a comprehensive production-grade middle layer: validated linear pipelines, batch sequential run helpers, a narrow `pb.diagnostics.v1` diagnostic record/collector contract, a sequential runtime, adapters (including runtime-bound stateful callables, C-style function-pointer adapters, and synchronous coroutine stage adapters, synchronous sender/receiver scaffolds), result/error plumbing, a three-axis runtime-enforced `::with<>` policy DSL (errors / diagnostics / copying), observer hooks with fan-in lifecycle events, cooperative cancellation (`pb.cancel.v1`), selected-output branch/join execution, explicit sequential and thread-pool fan-in with multi-error envelope (`pb.fan_in.errors.v1`), fan-in clone/projection policies (`pb::shared_view` / `pb::projected` / `pb::unique_clone`), a `pb::tooling::pipeline_registry`-backed `pb_cli` with five built-in pipelines, descriptor-backed DOT / JSON / text helper output with the typed `pb.core.graph.v1` regression contract, stateful storage DSL (`pb.state.v1`) including thread-local storage policy, compile-guarded optional backend seams (TBB/Taskflow/stdexec — dormant by default), a C++20 named module build (`modules-ninja` preset), typed C++26 feature gates, indexed pipeline I/O aliases, compile-time smoke targets, package smoke, and cross-compiler workflow scaffolding.

It is still not the full research-plan product. The remaining maximum-production work is concentrated in: working external dependency backends (oneTBB / Taskflow / stdexec), frozen descriptor/export compatibility contracts, runtime enforcement of the copying policy axis, full async/sender coroutine backends beyond the synchronous scaffold, release-grade performance budgets, a working C++26 reflection adapter on a real toolchain, and final exact-SHA release publication.
