# Coordinated Work Lanes

This project is being hardened through four reviewable production-readiness lanes. Each lane owns a distinct surface so changes stay small, testable, and easy to merge.

## Lane 1: Architecture, API, and core DSL

- Owns public pipeline APIs under `include/pb/`.
- Owns typed stage metadata, compile-time validation, diagnostics, and header hygiene.
- Success evidence: compile-pass and compile-fail tests that prove DSL acceptance and rejection paths.

## Lane 2: Runtime, executor, and error model

- Owns sequential execution behavior, `result`/error propagation, runtime diagnostics, and executor abstractions.
- Keeps safe failure behavior explicit before optional parallel or async backends are added.
- Success evidence: runtime smoke tests for value propagation, expected-like failures, and zero-stage behavior.

## Lane 3: CMake, packaging, and tooling

- Owns targets, install/export config, package-consumer behavior, presets, warning options, optional tooling, and benchmark toggles.
- Keeps build hygiene independent from API/runtime feature work.
- Success evidence: configure/build/install/package-consumer smoke checks and preset coverage.

## Lane 4: Tests, CI, and regressions

- Owns compile-pass, compile-fail, runtime, package-consumer, example, and regression test coverage.
- Converts bug fixes and readiness gaps into repeatable checks before behavior changes land.
- Success evidence: focused regression tests plus full local `ctest` runs.

## Coordination rules

- Prefer one lane-owned change per commit.
- Touch shared files only when the lane's success evidence requires it.
- Include the verification command and result in the task report so the leader can reconcile worktrees before shutdown.


## Resume model: next 3-agent long-horizon wave

When usage resets and the next team starts, collapse the prior five-lane model into three active lanes so the leader can keep coding throughput high without losing documentation/release coordination.

### Worker 1: Core API / diagnostics coding

- Owns `include/pb/core/*`, `include/pb/pipeline.hpp`, and matching compile-pass/compile-fail tests.
- Pulls from Lane 1 plus the compile-time diagnostics portion of Lane 4.
- First safe stage: audit the existing linear DSL, `stage_traits`, `describe`, public aliases, and validation diagnostics against `research/pipeline_builder_cpp_research_plan.md`.
- Next stages: add one narrow public API or diagnostic ergonomics improvement at a time, then prove it with compile-pass and compile-fail coverage.
- Explicit stop boundary: do not implement branch/join, graph export, or broad DSL rewrites without a new leader-scoped plan.

### Worker 2: Runtime / adapters coding

- Owns `include/pb/runtime/*`, `src/runtime/*`, `include/pb/adapt/*`, and matching runtime/adapter tests.
- Pulls from Lane 2 plus adapter/runtime regression work from Lane 4.
- First safe stage: audit `run()`/`try_run()`, result factories, expected-like conversion, observer lifecycle, adapter contracts, and runtime diagnostics.
- Next stages: add one narrow hardening slice at a time around error construction, observer evidence, adapter edge cases, or safe execution seams.
- Explicit stop boundary: keep the runtime standard-library-only and preserve current `run()`/`try_run()` semantics unless the leader scopes a policy change.

### Worker 3: Updater / docs / release coordination

- Owns `README.md`, `docs/*`, `examples/*`, checkpoint notes, PR/release summaries, and GitHub/readiness monitoring.
- Pulls from Lane 3, Lane 5-style docs work, and release-evidence coordination.
- First safe stage: reconcile docs/examples with current supported MVP behavior and the latest Worker 1/2 patches.
- Next stages: refresh roadmap gap maps, release-readiness notes, package/benchmark evidence, and PR summaries after each completed coding batch.
- Explicit stop boundary: do not claim branch/join, graph export, optional backends, stable runtime descriptors, or benchmark thresholds until implementation and tests exist.

### Batch protocol

1. Each worker starts by reading `continue.md`, this file, `docs/roadmap-gap-map.md`, and the relevant sections of `research/pipeline_builder_cpp_research_plan.md`.
2. Each batch must name its lane, files touched, verification command, result, remaining risk, and next proposed task.
3. Prefer one lane-owned commit per batch using the Lore commit protocol.
4. If blocked, switch to another safe lane-local task instead of idling.
5. The leader reconciles shared-file edits before assigning the next batch.
