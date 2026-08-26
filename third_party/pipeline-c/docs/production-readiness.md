# Production Readiness Status

This project is still an MVP foundation for the roadmap in `research/pipeline_builder_cpp_research_plan.md`. Treat the current tree as production-readiness scaffolding rather than a stable release.

## Supported today

- C++20 target-based CMake project with `pb::core`, `pb::runtime`, and the `pb::pipeline` compatibility target.
- Standard-library-only public core/runtime surfaces, including the sequential backend and the `pb::runtime::thread_pool_backend` fan-in backend slice.
- Linear typed pipeline validation with `pb::from<T>::then<S>::to<U>`.
- Stage metadata through explicit `input_type`, `output_type`, `error_type`, and `stage_name()` or adapter-provided traits.
- Public pipeline introspection helpers: `pipeline_size_v`, `pipeline_input_t`, `pipeline_output_t`, `pipeline_stage_t`, `pipeline_stage_descriptor_t`, `describe()`, stage records, linear edge records, and finalized-pipeline `stage_count` / `empty` constants.
- Free-function and function-object adapters for legacy code integration.
- `pb::runtime_callable<In,Out>` — type-erased runtime-bound stateful callable adapter (owns a stateful lambda, `std::function`, or any callable with runtime state); `pb::bind_callable` factory. Requires the stateful storage policy. Header: `include/pb/adapt/runtime_callable.hpp`. Tests: `tests/runtime/runtime_callable_adapter_smoke.cpp`, `tests/compile_pass/public_headers/adapt_runtime_callable.cpp`.
- `pb::c_function_stage<In,Out,Fn>` — C-style function-pointer adapter (non-type template parameter); carries no runtime state, works with the default sequential engine. Same header.
- Synchronous coroutine stage adapter: `pb::coro::sync_task<T>` eager synchronous task + `pb::coroutine_stage<Stage>` / `pb::adapt_coroutine` — `co_return`-style coroutine callables compose as stages on the sequential engine. Header: `include/pb/adapt/coroutine.hpp`. Tests: `tests/runtime/coroutine_adapter_smoke.cpp`, `tests/compile_pass/public_headers/adapt_coroutine.cpp`.
- Dependency-free synchronous sender/receiver scaffold: `pb::sync_just(value)` plus `pb::sync_sender_stage<Factory, Input>` unwrap exactly one synchronous `set_value` into a pipeline stage and turn `set_error`, `set_stopped`, or no-completion into normal sequential runtime failures. This is **not** a full stdexec/P2300 backend. Header: `include/pb/adapt/sender_receiver.hpp`. Tests: `tests/runtime/sender_receiver_adapter_smoke.cpp`, `tests/compile_pass/public_headers/adapt_sender_receiver.cpp`.
- Sequential runtime execution for validated linear pipelines, including zero-stage identity pipelines and batch helpers `try_run_each(...)` / `try_run_range(...)` for range/sentinel and move-iterator inputs.
- Sequential branch execution with optional selected-output join stages, observer case events, stateful branch storage, branch-child fallback identities, heterogeneous branch outputs represented as `std::variant`, selected-output type-list joins, and move-only selected-branch input consumption when predicates observe by `const input_type&`. Variant joins preserve duplicate output alternatives by index. Type-list selected-output joins dispatch by C++ type; duplicate same-type branch outputs share the same overload unless the user encodes case identity into the output type. Explicit sequential fan-in joins are supported through `::fan_in<JoinStage>` / `::join_all<JoinStage>` with ordered `pb::fan_in_results<...>` aggregates for zero/one/many passing cases, failed predicate/stage slots with diagnostics, void-output case aggregation, and borrowed move-only fan-in when every passing case stage accepts `const input_type&`. Compile-fail coverage still rejects predicates that consume move-only inputs and fan-in stages that would require copying a non-copyable input by value.
- `pb::shared_view<T>` — copy-cheap `shared_ptr`-backed wrapper for non-copyable fan-in inputs; `pb::make_shared_view` convenience factory; `pb::fan_in_uses_copy_v` / `pb::fan_in_uses_borrow_v` introspection variable templates.
- `pb::unique_clone<T, Clone>` — owned per-case deep-copy fan-in policy; each passing fan-in case receives an independently-owned clone. Closes the "owned per-case unique-clone beyond shared-view" gap. Header: `include/pb/runtime/clone.hpp`. Tests: `tests/runtime/fan_in_unique_clone_smoke.cpp`, `tests/compile_pass/public_headers/runtime_clone_unique.cpp`.
- `pb::projected<From, Projection, Stage>` — projection adapter wrapping an existing stage with a user-supplied projection callable, enabling plug-in borrow/extract strategies for fan-in branches and linear chains without rewriting the underlying stage. Same header. Test: `tests/runtime/projected_stage_smoke.cpp`.
- Fan-in observer lifecycle events — four additive v1-ABI-safe no-op virtuals in `include/pb/runtime/observer.hpp`: `on_fan_in_started(branch_id, case_count)`, `on_fan_in_case_scheduled(branch_id, case_index, case_stage_id)`, `on_fan_in_case_completed(branch_id, case_index, case_stage_id, success)`, `on_fan_in_completed(branch_id, selected_count, completed_count, failed_count)`. Emitted on both sequential and thread-pool fan-in paths. Test: `tests/runtime/fan_in_observer_events_smoke.cpp`.
- `pb::fan_in_error_envelope` + `pb::collect_fan_in_errors` — structured aggregation of every failed fan-in case into one object; stable schema `"pb.fan_in.errors.v1"`. Header: `include/pb/runtime/fan_in_error.hpp`. Tests: `tests/runtime/fan_in_error_envelope_smoke.cpp`, `tests/compile_pass/public_headers/runtime_fan_in_error.cpp`.
- Cooperative cancellation: `pb::cancellation_source` / `pb::cancellation_token` over a shared `std::atomic<bool>` (lock-free, acquire/release); schema `"pb.cancel.v1"`. Thread-pool fan-in backend checks the token before enqueueing each case — not-yet-started cases are skipped and reported via `on_case_skipped`. Preemptive interruption of an already-running stage is documented as out of scope. Header: `include/pb/runtime/cancellation.hpp`. Tests: `tests/runtime/thread_pool_cancellation_smoke.cpp`, `tests/compile_pass/public_headers/runtime_cancellation.cpp`.
- Machine-readable diagnostics contract: `pb::diagnostics::diagnostics_schema_version == "pb.diagnostics.v1"`, `severity`, `diagnostic_record`, `diagnostic_collector::add(...)` with `std::source_location` capture, and the initial `pb::diagnostics::suggestions::*` catalog. Header: `include/pb/core/diagnostics.hpp`. Tests: `tests/compile_pass/diagnostics_contract.cpp`, `tests/compile_pass/public_headers/core_diagnostics.cpp`.
- Error-policy DSL: five factory-built, engine-side wrappers — `pb::with_throw_on_error` → `throwing_engine` (throws `pb::pipeline_exception` on failure), `pb::with_terminate_on_error` → `terminating_engine` (calls `std::terminate()` on failure), `pb::with_ignore_errors` → `ignoring_engine` (returns user-supplied fallback on failure), `pb::with_propagate_exceptions` → `propagating_engine` (re-throws exceptions captured as `error_category::exception`), `pb::with_verbose_diagnostics` → `verbose_engine` (owns a `pb::verbose_observer` that logs every transition to a `std::ostream` under the `[pb.verbose]` prefix). Header: `include/pb/runtime/error_policy.hpp`. Cross-wrapper `run()` / `try_run()` parity is proven over linear, selected-output branch, and explicit fan-in topologies via the parity matrix tests. The wrappers compose: `verbose_engine` can wrap any of the other four because all five expose `set_observer` / `get_observer` / `describe` / `descriptor` directly. `std::expected`-returning stages flow through every wrapper with `error_category::expected_error` semantics where `PB_HAS_STD_EXPECTED == 1`.
- Runtime-enforced `::with<>` policy DSL (three axes): `pipeline` gained a 4th defaulted `Policies` type-list parameter; `pb::compile<P>(sequential{})` inspects it and wraps the engine accordingly:
  - **errors axis:** `::with<pb::policy::errors::throwing/terminating/ignoring/propagating/result>` selects the matching engine wrapper. `pb::has_error_policy_v<P>` is queryable. Tests: `tests/runtime/with_policy_dsl_runtime_smoke.cpp`, `tests/compile_pass/with_policy_dsl_compile_pass.cpp`.
  - **diagnostics axis:** `::with<pb::policy::diagnostics::verbose>` wraps the (possibly error-policy-wrapped) engine in `pb::with_verbose_diagnostics`; `::with<diagnostics::quiet>` leaves the engine unchanged. `pb::has_diagnostics_policy_v<P>` is queryable. Tests: `tests/runtime/policy_axes_runtime_smoke.cpp`, `tests/compile_pass/policy_axes_compile_pass.cpp`.
  - **copying axis:** `::with<pb::policy::copying::value/move_only/shared/clone>` is carried + queryable; `pb::has_copying_policy_v<P>` / `pb::pipeline_copying_policy_t<P>`. Runtime enforcement beyond `pb::shared_view` / `pb::unique_clone` is forward-looking.
  - All three axes are independent and order-insensitive; mixed `::with<throwing>::with<verbose>::with<move_only>` compiles and each `has_*_policy_v` reports only its own axis. Header: `include/pb/core/policy.hpp`, `include/pb/runtime/sequential.hpp`.
- `pb::tooling::pipeline_registry` — name-keyed, insertion-ordered registry for built-in and user-registered pipelines. Header: `include/pb/tooling/cli_registry.hpp`. `pb_cli` is refactored to use it; 5 built-in pipelines: `order-linear`, `order-branch`, `order-fan-in`, `order-enrich`, `order-variant`. Tests: `tests/run_pb_cli_list.cmake`.
- Optional backend integration seams (dormant compile-guarded scaffolds): `include/pb/backends/{tbb,taskflow,stdexec}.hpp` — fully `#if PB_HAS_*`-guarded; each exposes `*_backend_available_v == false` on the default build. `PB_ENABLE_{TBB,TASKFLOW,STDEXEC}` CMake options are wired (default OFF). Test: `tests/compile_pass/backend_scaffolds_smoke.cpp` static-asserts all three are dormant. **Working external backends are NOT implemented.**
- C++20 named module build: `PB_BUILD_MODULE` CMake option builds `include/pb/pipeline.mpp` via `FILE_SET CXX_MODULES`; `modules-ninja` preset; `tests/module/use_module.cpp` does `import pb.pipeline;` and passes as `pb_use_module`. Verified on clang 22 (llvm-mingw), CMake 4.3, Ninja. Default header build unaffected. Docs: `docs/modules.md`.
- `pb.core.graph.v1` typed schema-version contract: the literal `"pb.core.graph.v1"` identifier is enforced by compile-time regression in `tests/compile_pass/schema_v1_contract.cpp` and byte-equal golden output is locked in `tests/compile_pass/schema_v1_extended_golden.cpp`. Schema specification: `docs/export-schema-v1.md`.
- `pb::export_text` — compact text-format export helper alongside the existing JSON/DOT helpers. Header: `include/pb/core/export_text.hpp`. Tests: `tests/compile_pass/export_text.cpp`, `tests/compile_pass/export_text_golden.cpp`, `tests/compile_pass/public_headers/export_text.cpp`.
- Typed C++26 / C++23 feature gate constants: `pb::features::has_cpp26`, `pb::features::has_reflection`, `pb::features::has_contracts`, `pb::features::has_pack_indexing`, `pb::features::has_std_expected`, `pb::features::has_deducing_this` as `inline constexpr bool`; all evaluate to `false` on the C++20 baseline. Implication invariants also exported. Header: `include/pb/core/cpp26_features.hpp`. Docs: `docs/cpp26-feature-gates.md`. Test: `tests/compile_pass/cpp26_features_fallback.cpp`.
- Reflection adapter scaffold in `include/pb/adapt/reflect.hpp` gated on `PB_HAS_REFLECTION` with a graceful no-op C++20 fallback. C++26 reflection is not supported on current baselines — the scaffold is a forward-compat seam only. Test: `tests/compile_pass/reflect_stage_gate.cpp`.
- Substantially expanded `pb_cli` (`tools/pb_cli.cpp`) with `tests/run_pb_cli_describe.cmake` and `tests/run_pb_cli_list.cmake` test drivers.
- Compile-pass, compile-fail, runtime, example, package, compile-time/header benchmark smoke scaffolding, branch/fan-in compile-time benchmark (`bench/compile_time/branch_fan_in_10.cpp`), thread-pool fan-in runtime benchmark (`bench/runtime/thread_pool_fan_in.cpp`), and cross-compiler validation workflow. Targeted thread-pool backend fan-in runtime coverage including stress test is present. Current working tree: **250/250 local ctest** on `clang-dev-ninja`; `include/pb/pipeline.mpp` passes a direct `clang++ -std=c++20 -I include -fsyntax-only -x c++` syntax probe after the sender/receiver module export update. The `modules-ninja` preset was not refreshed in this environment because `clang-scan-deps` is missing; prior clang-22 module evidence remains historical. Cross-compiler validation on the new SHA pending.
- Stateful storage DSL (`pb.state.v1`): thread-local `pb::current_state<S>()` / `pb::try_current_state<S>()` plus five engine-wrapper factories — `pb::with_state<S>(engine)` (owned, persists across runs), `pb::with_borrowed_state<S>(engine)` (caller supplies state per call via `run_with_state`), `pb::with_shared_state<S>(engine, ptr)` (`shared_ptr<S>` across engines/threads), `pb::with_reset_per_run_state<S>(engine)` (owned + reset before each run), `pb::with_thread_local_state<State>` (thread-local storage for safe stateful stages under concurrent invocation). Composes with every error-policy wrapper; exception-safe via `pb::state_context<S>` RAII frame. Header: `include/pb/runtime/state.hpp`. Docs: `docs/state-dsl.md`. Tests: `tests/runtime/state_dsl_smoke.cpp`, `tests/runtime/state_thread_local_smoke.cpp`. Schema-pinned in `tests/compile_pass/schema_v1_cross_version_regression.cpp`.

## Current release gate status

Last cross-compiler-validated code snapshot: code SHA `87299c14c813753d170911239e251064cbbfee6f` passed the local developer and package-release verification loop: `clang-dev-ninja` build/full CTest `163/163`, `package-release-clang-ninja` build/CTest `163/163`, and TGZ package generation (`build/package-release-clang-ninja/pipebuilder-0.1.0-Linux.tar.gz`).

Latest GitHub cross-compiler validation snapshot: code SHA `87299c14c813753d170911239e251064cbbfee6f` passed the cross-compiler workflow: GCC C++20/C++23 `163/163`, Clang C++20/C++23 `163/163`, MSVC C++20 `163/163`, and clean Ubuntu `package-release-clang-ninja` `163/163` plus TGZ package generation. See [Cross-Compiler Validation Status](cross-compiler-validation.md).

Current working tree state: **250/250 local ctest** on `clang-dev-ninja` (integrated worker-1/3/4 team head after post-wave diagnostics/batch-run/export optimization, sender/receiver scaffold, export-helper misuse diagnostics, tooling registry/CLI option hardening, state DSL accessor hardening, fan-in observer/trace hardening, and reflect_stage fallback diagnostics commits — `pb::unique_clone`, thread-local state, fan-in observer lifecycle events, fan-in multi-error envelope `pb.fan_in.errors.v1`, synchronous coroutine adapter, `pb::tooling::pipeline_registry`, runtime-enforced `::with<>` errors + diagnostics + copying DSL axes, cooperative cancellation `pb.cancel.v1`, `pb::runtime_callable` / `pb::c_function_stage`, dormant backend scaffolds, C++20 named module `modules-ninja` preset). Cross-compiler validation on the integrated team head has not yet been rerun. Rerun the cross-compiler workflow on the final candidate SHA before any release-facing claim.

The package-consumer smoke gate now uses the target-specific downstream executables generated by `cmake/PBPackageSmoke.cmake.in`: `pipebuilder_core_target_smoke`, `pipebuilder_runtime_target_smoke`, and `pipebuilder_pipeline_alias_smoke`. Treat `pb_package_config_smoke` as the minimum package-verification gate, and require a fresh `package-release-clang-ninja` configure/build/CTest/package run on the candidate SHA before calling the package lane release-ready.

## Historical local MVP audit

Baseline `aa3b8f6` on `main` was audited against `research/pipeline_builder_cpp_research_plan.md` as a linear MVP foundation. The developer preset configure/build/CTest loop passed with **78/78 tests**. That historical audit supports the linear MVP claims below, but later candidate evidence is required for newer branch/join support and any roadmap-only items such as full graph export, optional backends, stable runtime descriptors, or release-grade benchmark budgets.

See `continue.md` for the resume checklist and next 3-agent long-horizon work queue.

## Current branch-execution update

Post-`aa3b8f6` branch hardening promoted sequential branch execution into the supported surface. All wave-1/2/3 fan-in features are now shipped: unique_clone, thread-local state policy, fan-in observer lifecycle events, fan-in multi-error envelope, synchronous coroutine adapter, pipeline registry, runtime-enforced `::with<>` DSL (three axes), cooperative cancellation, runtime-bound callable adapters, dormant optional backend seams, and C++20 named module build.

### Current production-grade boundary table

| Surface | Production-grade for current scope | Partial / not production-grade yet |
| --- | --- | --- |
| Linear DSL/runtime | Typed linear DSL, compile-time validation, finalized-pipeline `stage_count` / `empty` helpers, sequential runtime, batch `try_run_each` / `try_run_range` helpers, result/expected-like handling, adapters (including `pb::runtime_callable`, `pb::c_function_stage`, synchronous `pb::coroutine_stage`), examples, and package smoke are tested and documented. | Stable observer ABI and richer topology coverage are still future work. |
| Branch/join | Selected-output branch/join, heterogeneous `std::variant` outputs, type-list selected-output joins, explicit sequential and thread-pool fan-in, failed/void fan-in slots, borrowed move-only fan-in, `pb::shared_view<T>` non-copyable fan-in inputs, `pb::unique_clone<T,Clone>` owned per-case deep-copy policy, `pb::projected` projection adapters, fan-in observer lifecycle events, fan-in multi-error envelope (`pb.fan_in.errors.v1`), cooperative cancellation (`pb.cancel.v1`), stateful branch storage, and selected-branch move-only consumption are supported. | Preemptive (non-cooperative) cancellation, dependency backend branch execution, selected-output tagged case-result policy, and broader ownership/lifetime policies remain roadmap-only. |
| Error-policy DSL | Five named wrappers shipped: `throwing_engine`, `terminating_engine`, `ignoring_engine`, `propagating_engine`, `verbose_engine` (with owned `verbose_observer`). `pb::pipeline_exception` carries the runtime diagnostic. Cross-wrapper `run()` / `try_run()` parity is proven across linear, branch, and fan-in topologies. Composability is shipped. `std::expected`-shape stages flow through every wrapper. Runtime-enforced `::with<pb::policy::errors::...>` DSL axis is active. | Async/sender coroutine backends; unified policy-surface narrative docs. |
| Policy DSL (three axes) | Errors and diagnostics axes are runtime-enforced via `compile<>`. Copying axis is carried + queryable; `pb::has_copying_policy_v<P>`. All three axes are independent and order-insensitive. | Runtime enforcement of the copying axis beyond `pb::shared_view` / `pb::unique_clone`; executor capability / assertion / contract axes. |
| Backend execution | `sequential` plus the standard-library `thread_pool_backend` fan-in slice are supported and validated on the last cross-compiler SHA. Dormant compile-guarded seams for TBB/Taskflow/stdexec exist. | Working oneTBB, Taskflow, stdexec backends; broad backend examples/benchmarks; preemptive cancellation. |
| Adapters | Free-function, member-function, function-object/functor, `pb::runtime_callable`, `pb::c_function_stage`, synchronous `pb::coroutine_stage` / `pb::adapt_coroutine`, and dependency-free synchronous `pb::sync_sender_stage` / `pb::sync_just` are shipped and tested. | Full async/sender coroutine backends; working C++26 reflection adapter. |
| Descriptor/export/diagnostics | DOT/JSON/text helpers cover linear, selected-output branch, and explicit fan-in helper output with golden tests. `pb.core.graph.v1` typed contract pins the helper schema-version string and field layout at compile time. `pb::export_text` ships. `pb.diagnostics.v1` pins the narrow machine-readable diagnostic record/collector shape. | A frozen external interchange schema, CLI/file export for arbitrary user pipelines, backend graph export, diagnostic artifact export, and schema migration policy remain roadmap-only. |
| C++26 feature gates | `pb::features::*` typed `constexpr bool` constants for six language facilities plus implication invariants. All evaluate to `false` on C++20 baselines; fallback path is regression-tested. | C++26 reflection is gated and falls back — it is not supported on current compiler baselines. |
| C++20 named module | `PB_BUILD_MODULE` + `modules-ninja` preset + `import pb.pipeline;` verified on clang 22. Default header build unaffected. | Broader compiler compatibility; install/export story for module consumers. |
| Reflection adapter | `include/pb/adapt/reflect.hpp` scaffold compiles on C++20 with a graceful no-op fallback when `PB_HAS_REFLECTION` is absent. | No reflection-powered functionality ships until a C++26 reflection toolchain baseline is established. |
| Release evidence | Working tree at 250/250 local ctest; sender/receiver module export syntax probe passes, but `modules-ninja` was not refreshed locally because `clang-scan-deps` is missing. Last cross-compiler-validated SHA has local full/package validation plus GitHub CI and cross-compiler/package evidence. | Cross-compiler workflow not yet rerun on the integrated team head. No published tag/release yet; MSVC C++23, Windows package-release, and benchmark budgets remain unclaimed. |

## Local readiness checks

Run the developer build and test loop before relying on the tree:

```bash
cmake --preset clang-dev-ninja
cmake --build --preset clang-dev-ninja
ctest --preset clang-dev-ninja --output-on-failure
```

To build and test the C++20 named module (clang 22 + CMake 4.3 + Ninja required):

```bash
cmake --preset modules-ninja
cmake --build --preset modules-ninja
ctest --preset modules-ninja --output-on-failure -R pb_use_module
```

When touching examples or benchmark guidance, also run the relevant smoke targets from [Build and Verification](build.md). For compile-time/header scaffolding, use `cmake --build --preset clang-dev-ninja --target pb_compile_time_benchmarks` plus `ctest --preset clang-dev-ninja --output-on-failure -R 'benchmark|compile_time|header|bench'`. Record the preset, compiler, and build type with any benchmark timing because the benchmark targets are not release performance gates yet.

## Package consumer smoke

The installed package is expected to support downstream CMake consumers that use:

```cmake
find_package(pipebuilder CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE pb::pipeline)
```

`pb::pipeline` remains the documented compatibility target for consumers that want the combined surface. The installed package also exports `pb::core` and `pb::runtime` for consumers that need the split targets directly, and `pb::pipeline` forwards to `pb::runtime`.

The package release preset in [Build and Verification](build.md) is the release-readiness gate for this contract. The `pb_package_config_smoke` test installs the current build into a temporary prefix, verifies that `find_package(pipebuilder CONFIG REQUIRED)` defines `pb::core`, `pb::runtime`, and `pb::pipeline`, builds and runs separate consumers against each imported target, and checks the generated TGZ for key package entries. Treat that test as the minimum package-consumer contract only when it passes on the release candidate SHA.

## Release checklist

Before cutting a release candidate, collect evidence for:

- `cmake --list-presets=all` succeeds and the preset names are unique.
- The debug package preset and release Clang package preset both configure, build, and pass CTest on the candidate SHA.
- `pb_package_config_smoke` passes in the release package preset and its target-specific downstream consumers run successfully.
- The release package target produces a local archive for inspection before any external publication.
- Benchmark smoke targets run, and any recorded numbers include the context listed in [Build and Verification](build.md).
- Public examples still build/run, including the intentional compile-fail diagnostic example.
- Known gaps below are either documented for the release or converted into blocking issues.
- Cross-compiler validation rerun on the integrated team head (currently pending).

## Known gaps before a stable release

- Branch/join: sequential + thread-pool fan-in with all wave-1/2/3 features are supported. oneTBB/Taskflow/stdexec backends (dormant scaffold seams exist), preemptive (non-cooperative) cancellation, broad backend branch examples/benchmarks, and a stable external runtime descriptor/export contract remain roadmap-only. See [Research Verification Matrix](research-verification-matrix.md), [Branch / Join Roadmap / Status](branch-join-roadmap.md), [Graph Export Roadmap / Status](graph-export-roadmap.md), [Observer Hooks Roadmap / Status](observer-hooks-roadmap.md), [Optional Backends Roadmap / Status](optional-backends-roadmap.md), and [Runtime Descriptor Roadmap / Status](runtime-descriptor-roadmap.md) for current boundaries.
- The active missing-feature queue is tracked in [Roadmap Gap Map](roadmap-gap-map.md#active-missing-feature-priority-queue). Treat worker-lane reports as integration leads, not release claims, until the corresponding commits, tests, and docs are present on the candidate branch.
- Public diagnostics are covered by compile-fail smoke tests plus the `pb.diagnostics.v1` record/collector contract test, but exact compiler wording and graph-aware/exported diagnostic artifacts are still being hardened. See [Diagnostics Roadmap / Status](diagnostics-roadmap.md) for the current supported boundary versus the richer roadmap.
- The error-policy DSL is shipped and parity-hardened: five factory-built wrappers (`throwing_engine`, `terminating_engine`, `ignoring_engine`, `propagating_engine`, `verbose_engine`) are tested via parity matrix, composability, and `std::expected` lanes. Runtime-enforced `::with<pb::policy::errors::...>` DSL axis is active on the sequential engine. The remaining gaps are runtime enforcement of the copying policy axis and full policy-surface narrative docs.
- DOT/JSON/text export helpers now cover linear, selected-output branch, and explicit fan-in pipelines. The `pb.core.graph.v1` schema-version string is backed by a typed contract with byte-equal golden regressions. `pb::export_text` adds a compact text-format helper. A frozen external interchange schema, CLI/file export for arbitrary user pipelines, backend graph export, and schema migration policy remain roadmap-only.
- Optional backend support now includes the standard-library `thread_pool_backend` for the current fan-in slice (with cooperative cancellation). Dormant compile-guarded seams for TBB, Taskflow, and stdexec exist — `*_backend_available_v == false` on the default build. Working external backends are NOT shipped. The `tests/compile_pass/backend_scaffolds_smoke.cpp` test static-asserts the dormancy.
- C++20 named module: `PB_BUILD_MODULE` + `modules-ninja` preset + `import pb.pipeline;` verified on clang 22 (llvm-mingw), CMake 4.3, Ninja. Broader compiler compatibility (GCC, MSVC) and install/export story for module consumers remain unclaimed.
- Compile-time/header benchmark smoke targets can prove the representative header, 5-stage, 50-stage, and branch/fan-in translation units build and run from the developer preset. Thread-pool fan-in runtime benchmark exists. Release timing thresholds and CI regression budgets are not established.
- Local and GitHub validation passed for code SHA `87299c14c813753d170911239e251064cbbfee6f`: local `clang-dev-ninja` and `package-release-clang-ninja` each passed `163/163` plus package generation, normal CI passed Clang dev/package/benchmark lanes, and GitHub cross-compiler validation passed GCC C++20/C++23, Clang C++20/C++23, MSVC C++20, and clean Ubuntu package-release. The current working tree passes **250/250 local ctest**; cross-compiler validation on the integrated team head is pending. MSVC C++23, Windows package-release, C++26 gates, and other platform/package-manager matrices remain unclaimed.

## Small production-readiness slice (safe, immediate)

For the next minimal safe release-hardening step, refresh exact-SHA release evidence on the final candidate, then archive the result:

1. `cmake --preset package-release-clang-ninja`
2. `cmake --build --preset package-release-clang-ninja`
3. `ctest --preset package-release-clang-ninja --output-on-failure -R pb_package_config_smoke`
4. `cmake --build --preset package-release-clang-ninja --target package`
5. Run or rerun `Cross Compiler Validation` on the integrated team head (250/250 ctest local; non-doc code changes have landed since last revalidation).
6. Record the package smoke pass, package artifact path, cross-compiler workflow URL, preset, compiler versions, and commit SHA.

This keeps the slice narrow, deterministic, and low-risk: it validates install-time consumer compatibility plus compiler-matrix portability without changing runtime behavior.

## Documentation contract

Examples should show one complete successful path and one intentional failure path. If an example relies on future roadmap behavior, label it as aspirational instead of presenting it as supported. Keep public API promises aligned with the tests that currently pass.
