# Continue checkpoint — Pipeline-c++ long-horizon resume

Snapshot UTC: `2026-06-09T09:00:00Z`
Repo: `C:\Users\Tonyt\Documents\GitHub\Pipeline-c++`
Current branch: `main`
Last committed upstream release-evidence anchor remains `87299c14c813753d170911239e251064cbbfee6f`; the current local team batch is ahead of `origin/main` and must be pushed only after fresh full verification.

## 2026-06-09 coordinator checkpoint — integrated team evidence refresh in progress

Team batch baseline at startup: `a7494d3` (`Introduce string_sink; optimize exports and fan-in`) on `origin/main`. The integrated local team head also includes Worker 4's dependency-free synchronous sender/receiver scaffold (`pb::sync_just`, `pb::sync_sender_stage`) and Worker 3's fan-in trace/verbose observer hardening (`fan_in_started`, `fan_in_case_scheduled`, `fan_in_case_completed`, `fan_in_completed`).

Worker-1 coordinator batch is aligning docs/checkpoints with those shipped surfaces while preserving boundaries: working oneTBB/Taskflow/stdexec backends, preemptive cancellation, full async/sender coroutine backends beyond the synchronous scaffold, exported diagnostic artifacts, frozen diagnostic wording/schema migrations, and release tagging remain unclaimed. Fresh local verification for this batch must cite the current `clang-dev-ninja` configure/build/CTest, `warnings-as-errors-ninja` build, and whitespace check before any commit/push. Verified local CTest count after integrating the sender/receiver runtime/example, compile-fail boundary tests, export-helper misuse diagnostics, tooling registry/CLI checks, and state DSL accessor hardening plus reflect_stage fallback diagnostics is **250/250**; the sender/receiver public-header coverage is part of the existing `pb_public_headers_compile_pass` CTest target.

## 2026-06-02 multi-agent waves 1–3 — ALL integrated and green

Three long-horizon multi-agent waves closed the codeable roadmap gaps. Default preset `clang-dev-ninja`: **220/220 ctest, 0 failures**. Also verified: `warnings-as-errors-ninja` builds clean (`-Werror`); `package-release-clang-ninja` 220/220 + `pipebuilder-0.1.0-win64.tar.gz` generated; `bench-dev-ninja` builds the new benches; `modules-ninja` builds the C++20 module and `pb_use_module` (`import pb.pipeline;`) passes.

Session commits on top of `8d612b1`:

```
f9cbe26 wave-3: runtime-callable adapter, backend scaffolds, C++20 module build
e9605fd ::with policy DSL diagnostics + copying axes
99fc579 cooperative cancellation token (thread-pool fan-in)
94d59ba runtime-enforced ::with<pb::policy::errors::...> DSL
6ecf98b wave-2 batch1: fan-in multi-error envelope, coroutine adapter, pb_cli registry
331c6e8 docs: wave-1 + branch/fan-in compile-time benchmark
b57805b wave-1: unique_clone, thread-local state, fan-in observer events, thread-pool demo/stress/bench, branch compile-time bench
```

### Wave 2 (commits 6ecf98b, 94d59ba, 99fc579) — 208 → 216
- `pb::fan_in_error_envelope` + `pb::collect_fan_in_errors` — structured multi-error aggregation, stable `pb.fan_in.errors.v1` text schema (`runtime/fan_in_error.hpp`).
- Synchronous coroutine stage adapter: `pb::coro::sync_task<T>`, `pb::coroutine_stage` / `adapt_coroutine` (`adapt/coroutine.hpp`).
- `pb::tooling::pipeline_registry` + `pb_cli` refactor: registry-driven CLI; 3 original built-ins re-registered byte-identically + 2 new (`order-enrich`, `order-variant`).
- **Runtime-enforced `::with<pb::policy::errors::{throwing,terminating,ignoring,propagating,result}>` DSL** — `pb::core::pipeline` gained a 4th defaulted `Policies` type-list param; `compile<>(sequential{})` wraps the engine in the matching `error_policy.hpp` wrapper; `pb::has_error_policy_v`. (All `pipeline<...>` specializations updated for the 4th param.)
- Cooperative cancellation: `pb::cancellation_source`/`token` (`pb.cancel.v1`); `thread_pool_backend.cancel` skips not-yet-started fan-in cases. Preemptive interruption documented as out-of-scope.

### Wave 3 (commits e9605fd, f9cbe26) — 216 → 220
- `::with` policy axes: `pb::policy::diagnostics::{verbose,quiet}` (verbose enforced via `with_verbose_diagnostics`, composes around the error wrapper) + `pb::policy::copying::{value,move_only,shared,clone}` (carried + queryable; enforcement forward-looking). `has_diagnostics_policy_v` / `has_copying_policy_v`.
- `pb::runtime_callable` / `bind_callable` / `c_function_stage` — runtime-bound stateful-callable + C-style function-pointer adapters (`adapt/runtime_callable.hpp`).
- Optional backend SCAFFOLDS `include/pb/backends/{tbb,taskflow,stdexec}.hpp` — compile-guarded (`#if PB_HAS_*`), dormant by default, ungated `*_backend_available_v`; `PB_ENABLE_{TBB,TASKFLOW,STDEXEC}` options.
- C++20 named module build: `PB_BUILD_MODULE` + `FILE_SET CXX_MODULES` on `include/pb/pipeline.mpp` + `modules-ninja` preset; verified building + `import pb.pipeline;` passing.

### Still roadmap-only / blocked (NOT shipped)
- WORKING oneTBB / Taskflow / stdexec backends (only the gated seam exists; deps absent).
- Real C++26 reflection adapter (gated scaffold only; needs a C++26 reflection toolchain).
- Preemptive (non-cooperative) cancellation (impossible in portable standard C++).
- Cross-compiler validation rerun on the new SHA (needs CI/push; latest validated SHA is older).
- v0.1.0 release publication / tag.

### Environment note
Windows Defender / WDAC intermittently blocks freshly-linked `.exe`s, causing false `BAD_COMMAND` / "Process not started" ctest failures. Recover by relinking the target (`rm` the `.exe` + rebuild) and re-running — it is environmental, never a code regression.

## 2026-06-02 multi-agent wave 1 — integrated (208/208 ctest, clang-dev-ninja)

A 5-agent isolated-worktree workflow implemented and verified five file-disjoint roadmap gaps from `research/not done.md`; each was build+test verified in its own worktree, then integrated into `main` and re-verified together (208/208, up from 201/201):

- **`pb::unique_clone<T, Clone>`** (`include/pb/runtime/clone.hpp`) — owned per-case unique-clone fan-in policy: a copyable wrapper whose copy ctor deep-copies via `Clone` so each passing fan-in case gets an independently-owned value (contrast `shared_view`). Closes the "owned per-case unique-clone beyond shared-view" gap. Test: `tests/runtime/fan_in_unique_clone_smoke.cpp` + public-header check.
- **`pb::with_thread_local_state<State>`** (`include/pb/runtime/state.hpp`) — thread-local stateful storage policy so stateful stages stay isolated under concurrent invocation (groundwork for parallel backends). Test: `tests/runtime/state_thread_local_smoke.cpp` (multi-threaded).
- **Fan-in observer lifecycle events** (`observer.hpp` + `sequential.hpp` + `thread_pool_backend.hpp`) — additive v1-ABI-safe no-op virtuals `on_fan_in_started` / `on_fan_in_case_scheduled` / `on_fan_in_case_completed` / `on_fan_in_completed`, emitted on both sequential and thread-pool fan-in paths. Test: `tests/runtime/fan_in_observer_events_smoke.cpp`.
- **Thread-pool fan-in example + stress test + runtime benchmark** — `examples/thread_pool_fan_in_demo.cpp`, `tests/runtime/thread_pool_fan_in_stress_smoke.cpp`, `bench/runtime/thread_pool_fan_in.cpp` (builds under `bench-dev-ninja`).
- **Branch/fan-in compile-time benchmark** — `bench/compile_time/branch_fan_in_10.cpp` + `pb_compile_time_benchmarks` target + `tests/compile_pass/branch_compile_time_smoke.cpp`.

Verification at integration: `cmake --preset clang-dev-ninja` clean, full build clean (208 targets), `ctest --preset clang-dev-ninja` → **100% passed, 0 failed / 208**; `bench-dev-ninja` builds `pb_bench_thread_pool_fan_in` clean. Cross-compiler validation on the new SHA still pending (rerun before tagging).

> Note: freshly-linked `.exe` files can transiently fail ctest with `BAD_COMMAND` / "Permission denied" due to Windows Defender file locks; re-running the single test (or relinking it) clears it. This is environmental, not a code regression.

### Prior checkpoint (superseded below)

Last committed HEAD before this wave: `cd5a80c` (`Add meta utilities, threadpool APIs and refactor`).
Working tree at checkpoint: **189/189 local ctest passing** on `clang-dev-ninja` after integrating the 2026-05-25 three-agent wave. Uncommitted: the production-grade batch (schema v1, error-policy DSL, clone/projection, C++26 typed gates, reflection adapter, export_text, pb_cli expansion) PLUS the integrated 3-agent wave additions (indexed stage I/O aliases + `terminate_on_error` compile-pass smoke + docs refresh). Cross-compiler validation has **not** been rerun on the new code; rerun before release tagging.

## 2026-05-25 team wave — integrated

The three-lane wave landed three narrow, verified slices on top of the production-grade batch:

- **core-diagnostics** — added `pb::pipeline_stage_input_t<P, N>` and `pb::pipeline_stage_output_t<P, N>` indexed I/O aliases with named out-of-range `static_assert` diagnostics. Touches `include/pb/core/describe.hpp` (append-only detail helpers + two public aliases) and `include/pb/pipeline.hpp` (two `using` re-exports). New tests: `tests/compile_pass/pipeline_stage_io_aliases.cpp`, `tests/compile_fail/pipeline_stage_input_index_oob.cpp`, `tests/compile_fail/pipeline_stage_output_index_oob.cpp`. The OOB diagnostic begins with the alias name and references `pipeline_size_v<Pipeline>` as the bound, replacing an opaque deep backtrace. Follow-up diagnostics now name the missing `input_type` or `output_type` member for invalid stages appended through `pb::from<...>::then<Stage>`.
- **runtime-adapters** — added `tests/compile_pass/terminate_on_error_smoke.cpp` exercising `pb::terminating_engine` AND `pb::ignoring_engine` type-instantiation surfaces (`input_type`, `output_type`, `underlying_engine`, `try_result_type`, factory deduction, `underlying()` ref-qualifier overloads, success-path `try_run()`/`run()`). Does NOT trigger `std::terminate`. Symmetry block also covers `ignoring_engine` `set_fallback`/`get_fallback` round-trip. **Next:** audit `run()` / `try_run()` boundaries, result factories, expected-like conversion, observer lifecycle, adapter diagnostics, and descriptor/observer/error identity consistency.
- **docs-release** — refreshed `docs/roadmap-gap-map.md` and `docs/production-readiness.md` to reflect the production-grade batch: error-policy DSL (`throwing`/`terminating`/`ignoring`/`propagating`/`verbose` engines), `pb::shared_view`, `pb::projected`, schema v1 typed contract (`pb.core.graph.v1`), typed `pb::features::*` C++26 gate constants, gated reflection-adapter scaffold, `pb::export_text`, expanded `pb_cli`. Test-count language bumped 163/163 → 189/189 local with explicit "cross-compiler validation on the new SHA pending" caveat. Did NOT promote: frozen external schema, Taskflow/oneTBB/stdexec, published release, MSVC C++23, C++26 reflection as supported. **Next:** align `docs/research-verification-matrix.md` with the new surfaces after the next code batch lands.

Verification done at integration time: `cmake --preset clang-dev-ninja` clean, `cmake --build --preset clang-dev-ninja` clean (175/175 link targets), `ctest --preset clang-dev-ninja --output-on-failure` → **100% tests passed, 0 tests failed out of 189**.

Worktree from the core-diagnostics agent still exists at `.claude/worktrees/agent-ac89e3bed5d15c9ba` (branch `worktree-agent-ac89e3bed5d15c9ba`); changes are already merged into the main tree, so the worktree can be removed when convenient.

## Historical: 2026-05-18 validation snapshot

Latest code-validation HEAD on GitHub: `6805543ede6946aa283be7f24fb3736c762f47b2` — cross-compiler validation passed.
Remote status at that checkpoint: pushed to `origin/main` at the validation SHA before that docs refresh.

This checkpoint replaces older OMX team pause notes. Use it to resume the next 3-agent long-horizon wave after usage resets without re-reading the entire prior team transcript.

## Current verified product state

The current tree is a **strong MVP foundation** for `research/pipeline_builder_cpp_research_plan.md`:

- C++20 target-based CMake project with `pb::core`, `pb::runtime`, and `pb::pipeline`.
- Typed linear DSL: `pb::from<T>::then<S>::to<U>`.
- Compile-time validation for stage shape, adjacent edge type compatibility, and final sink compatibility.
- Stage traits, stage names/keys, optional error metadata, descriptor helpers, and linear pipeline introspection.
- Free-function, member-function, and functor adapters for existing user code.
- Sequential runtime with `run()`, `try_run()`, result/expected-like plumbing, zero-stage identity behavior, and observer hooks.
- Compile-pass, compile-fail, runtime, example, package-consumer, and benchmark smoke scaffolding.
- Sequential branch/join support for the current standard-library sequential runtime: homogeneous outputs, variant-based heterogeneous outputs plus type-list selected-output joins including duplicate alternatives by index, optional joins, observer case events, stateful branch predicate/stage storage, and move-only selected-stage consumption when predicates inspect by `const input_type&`.
- DOT/JSON descriptor-backed helper export for linear and supported branch pipelines, including JSON branch topology, DOT label escaping, and helper-output golden regressions.
- Docs separate supported branch/export helper behavior from roadmap-only parallel fan-in joins, stable descriptor/export schema guarantees, CLI/file export, optional backends, runtime descriptor stability, and performance-budget work.

Fresh cross-compiler validation from the current code snapshot:

```text
validated_code_sha=6805543ede6946aa283be7f24fb3736c762f47b2
workflow=https://github.com/tonytranrp/Pipeline-c-/actions/runs/26058848575
GCC C++20: passed, 150/150
GCC C++23: passed, 150/150
Clang C++20: passed, 150/150
Clang C++23: passed, 150/150
MSVC C++20: passed, 149/149
package-release clean Ubuntu: passed, 150/150 plus TGZ package generated
```

## Current working tree note

At the validation checkpoint, `main` was pushed and GitHub Actions passed on SHA `6805543`. This docs-only refresh may create a later SHA; rerun the matrix before release tagging if exact tag-SHA evidence is required.


## Latest team-batch checkpoint (updated 2026-05-18)

Docs-lane monitoring observed current integration head `30447ae` plus runtime diagnostic identity integration `c13a54c` / `2939908` and docs checkpoint `1182b6d` after the core/runtime/adapter/docs hardening batch:

- `4be3ab1` / `3e48488` locked linear descriptor, observer failure callback, and `try_run()` error identity consistency.
- `371312a` / `218664c` added runtime `error_record` / `to_record(...)` diagnostic projection and smoke coverage.
- `c13a54c` / `2939908` preserved/filled custom expected-like diagnostic stage identity and aligned `error_record` / observer failure/exception stage identity with `engine.describe()` in the linear runtime path.
- `30447ae` required homogeneous `branch_node` case inputs and locked the mismatch diagnostic while staying marker-only.
- `4c6419e` added marker-only `branch_case_output` / `branch_outputs` metadata scaffolding and public-header coverage.
- `5afcb3a` / `f0652be` added compile-fail diagnostics for branch-output marker misuse.
- `dbb8d5b` added marker-only `join_node` validation and invalid join-stage diagnostics.
- `b737175` checked branch_case source compatibility at the marker boundary.
- `b293a65` added sequential observer accessor coverage in the runtime path.
- `8a6ea8a` validated branch predicate marker shape while keeping branch execution unsupported.
- `4722296` added branch marker aliases and an explicit unsupported-topology compile-fail boundary.
- `8b5fda6` strengthened descriptor alias symmetry for the current linear introspection helpers.
- `7a483b7` added adapter/member hardening in `include/pb/adapt/fn.hpp`.
- `ee412f7` added public-header coverage for core `stage_traits` aliases and tightened related diagnostic misuse cases.
- `3b1a231` / `23f1d60` hardened runtime `error_or(...)` fallback selection for expected-like/result normalization boundaries.
- `ec45eae` / `62820ed` / merge `caa43ee` carried sequential observer replacement/accessor coverage into the integrated history.

Release-facing wording should stay narrow but current: branch/join execution is now supported for the sequential slice, including homogeneous outputs, variant-based heterogeneous outputs plus type-list selected-output joins with duplicate alternatives by index, move-only selected-stage input consumption, observer events, stateful branch storage, and compile-time join validation. DOT/JSON export is helper-level for linear and supported branch pipelines. Do not claim parallel fan-in joins, stable descriptor/export compatibility, CLI/file export, optional backend execution, stable observer ABI/event schema, runtime descriptor export, or release-grade benchmark budgets. Use `docs/current-release-summary.md`, `docs/cross-compiler-validation.md`, and `docs/research-verification-matrix.md` as compact PR/release note seeds.

## What is done from the research plan

Treat these as shipped MVP support unless a future regression proves otherwise:

1. **Phase 1 — MVP type core:** implemented and tested through core concepts, stage traits, linear pipeline state, validation, and compile-pass/compile-fail coverage.
2. **Phase 2 — Runtime sequential execution:** implemented for validated linear pipelines with runtime tests for success, failure, exceptions, expected-like results, observer paths, and zero-stage identity.
3. **Phase 3 — User adapters:** implemented for free functions, member functions, and functors with runtime and compile-fail coverage.
4. **Phase 4 — Diagnostics/tooling foundation:** partially implemented through metadata helpers, compile-fail diagnostics, runtime error formatting, package smoke, benchmark smoke, and docs.
5. **Phase 7 — Packaging/release scaffolding:** partially implemented through install/export/package smoke checks, cross-compiler validation workflow/evidence, and release-readiness docs.

## What is still roadmap-only

Do not claim these as supported until code, tests, examples, and docs land together:

- Parallel all-branches fan-in / true backend multi-input join execution beyond the supported selected-output sequential branch slice.
- Stable descriptor/export schema, CLI/file export, and backend graph-export semantics beyond helper DOT/JSON output.
- Stable runtime descriptor/export contract.
- Optional Taskflow/oneTBB/stdexec backends.
- Fully unified exception/error policy between `run()` and `try_run()`.
- Release-grade compile-time/performance thresholds and CI-enforced budgets.
- Stable, frozen diagnostic wording/schema across all future slices.

## Recommended next 3-agent wave

Use the 3-agent structure from `docs/work-lanes.md`.

### Worker 1 — Core API / diagnostics coding

Owns: `include/pb/core/*`, `include/pb/pipeline.hpp`, compile-pass/compile-fail tests.

Next safe queue:

1. Audit `stage_traits`, `describe`, public aliases, and validation diagnostics against the research plan.
2. Add one narrow compile-time ergonomics improvement only if it has an obvious test target.
3. Prefer diagnostic clarity and alias symmetry over new topology features.
4. Add compile-pass and compile-fail coverage for every public API behavior change.

Avoid: branch/join, graph export, expression-template rewrites, or broad public API churn.

### Worker 2 — Runtime/adapters/diagnostics coding

Owns: `include/pb/runtime/*`, `src/runtime/*`, `include/pb/adapt/*`, runtime/adapter tests.

Next safe queue:

1. Audit `run()`/`try_run()` boundaries, result factories, expected-like conversion, observer lifecycle, adapter diagnostics, and runtime diagnostic record/export boundaries, and descriptor/observer/error identity consistency.
2. Add one narrow runtime hardening slice: clearer error construction, observer ordering evidence, adapter edge-case coverage, or exception-policy documentation tests.
3. Preserve current `run()` and `try_run()` semantics unless the leader scopes a policy change.
4. Keep standard-library-only design.

Avoid: async/parallel backends, dependency additions, or hidden implicit conversions.

### Worker 3 — Updater / docs / release coordination

Owns: `README.md`, `docs/*`, `examples/*`, checkpoint notes, PR/release summaries, GitHub/readiness monitoring.

Next safe queue:

1. Keep docs aligned with Worker 1/2 changes and current preset/test names, especially branch marker/output diagnostics and runtime descriptor/observer seams.
2. Refresh `docs/roadmap-gap-map.md`, `docs/production-readiness.md`, `docs/research-verification-matrix.md`, and release evidence notes when new behavior lands.
3. Monitor `git status`, branch divergence, and GitHub readiness. Push/pull only when explicitly authorized for the concrete action.
4. Prepare a compact PR summary with changed files, tests, risks, and remaining roadmap gaps.

Avoid: claiming roadmap-only features, editing generated build artifacts, or mixing docs-only commits with unrelated code changes.

## Leader resume checklist

Before starting the next team:

1. `git status --short --branch`
2. `git log --oneline -12`
3. Confirm what to do with token-tooling files listed above.
4. Re-run at least `cmake --preset clang-dev-ninja`, `cmake --build --preset clang-dev-ninja`, and `ctest --preset clang-dev-ninja --output-on-failure` if significant time or upstream changes passed; rerun `cross-compiler-validation.yml` before release tagging if non-doc code changed after `6805543`.
5. Launch the 3-agent wave with finite batch tasks and ask each worker to report changed files, tests, risks, and next task.

Before ending the next team wave:

1. Integrate or explicitly defer each worker output.
2. Run the developer preset configure/build/CTest.
3. Run package-release configure/build/CTest/package if packaging/docs/release surfaces changed.
4. Rewrite or replace any system-generated auto-checkpoint commits with Lore-format commits before final merge if history is still local and safe to rewrite.
5. Update this file with the new HEAD, test count, known risks, and next safe queue.

## Token-saving note

Use the local token-saving workflow during resume:

```bash
rtk tokrun -- cmake --build --preset clang-dev-ninja
rtk tokrun -- ctest --preset clang-dev-ninja --output-on-failure
tokgain
```

If `tokrun` reports failures, warnings, or missing detail, inspect the referenced `raw_log=...` file or rerun with `tokrun --raw` before making code decisions.
