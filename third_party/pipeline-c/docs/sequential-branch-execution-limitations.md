# Sequential Branch Execution — Supported Features and Limitations

Sequential branch routing with optional join stages is now supported for homogeneous outputs, heterogeneous outputs through `std::variant`, selected-output type-list joins, explicit fan-in joins, and move-only selected-branch input consumption. This page documents what works today and what remains roadmap work.

## Supported

- **Public DSL**: `pb::from<Input>::branch<CaseA, CaseB>::to<Output>` and `pb::from<Input>::branch<CaseA, CaseB>::join<JoinStage>::to<Output>`
- **Compile-time validation**: join context enforced (must follow `::branch<...>`), type compatibility checked, branch predicates validated, and branch outputs mapped to either a single homogeneous type or `std::variant<...>` when they differ
- **Runtime execution**: first-match-wins predicate evaluation with short-circuit, observer events (`on_case_selected`, `on_case_skipped`), error propagation with `[branch]` prefix annotation
- **Stateful storage**: branch predicates and stages respect `pb::runtime::stateful_sequential`, preserving mutable state across multiple pipeline runs
- **Join stages**: `::join<JoinStage>` appends a linear stage after the branch node, validated for type compatibility, including join stages that consume the heterogeneous branch `std::variant` or declare the raw branch output `pb::meta::type_list<...>` and overload every output type
- **Observer coverage**: standard observer events (`on_stage_start`, `on_stage_success`, `on_stage_failure`, `on_stage_exception`) plus branch-specific events (`on_case_selected`, `on_case_skipped`)
- **Multiple cases**: any number of branch cases (1+) supported via recursive template evaluation
- **Move-only selected input**: move-only branch inputs can be routed when predicates accept `const input_type&`; the selected branch stage receives the moved value
- **Explicit sequential fan-in**: `::fan_in<JoinStage>` / `::join_all<JoinStage>` evaluates all predicates, runs passing cases in declaration order, and passes `pb::fan_in_results<...>` to the join stage
- **Fan-in aggregation states**: fan-in slots expose skipped/completed/failed state, diagnostic text for predicate/stage failures, and void-output case completion without placeholder values
- **Borrowed move-only fan-in**: a move-only input may be used in fan-in when every passing case stage accepts `const input_type&`; by-value fan-in stages still require copyable input

## Not yet supported

### Backend/parallel fan-in / all-branch joins
Branch cases with different output types in the selected-output path produce a selected branch result: either the single homogeneous output type or a `std::variant<...>` for heterogeneous cases. A selected-output join may declare `pb::meta::type_list<...>` to select overloads for that one routed output. Explicit all-passing fan-in is available separately through `::fan_in<JoinStage>` / `::join_all<JoinStage>`, where the join receives `pb::fan_in_results<...>` with per-case skipped/completed/failed state. The sequential backend runs passing cases in declaration order. `pb::runtime::thread_pool_backend` may run passing fan-in case stages concurrently, while preserving declaration-order aggregate results and draining already-running cases before the join inspects the aggregate.

Current type model summary:

- branch outputs are tracked as a raw `pb::meta::type_list<...>` of case output types
- when case outputs differ, the execution boundary uses a unified `std::variant<...>` for the selected branch result
- a branch case or join stage may still use `pb::runtime::result<T>` for value-or-error execution, but that is orthogonal to the branch type model
- the type-list join path is selected-output dispatch: the join stage declares `pb::meta::type_list<...>` and overloads each raw output type
- the selected-output branch path has no per-case tagged result wrapper or tuple-style fan-in type; explicit fan-in uses `pb::fan_in_results<...>` instead
- selected-output branches either select one output value or report a runtime contract error if no case matches; explicit fan-in uses per-case result slots and allows zero passing cases

### Consuming predicates for move-only branch inputs
Move-only branch inputs are supported only when predicates can inspect the input by `const input_type&`. A predicate that must consume the move-only input before routing is not supported, because the selected branch stage still needs to receive the value.

### Branch-specific observer event ordering
The observer interface provides `on_case_selected` and `on_case_skipped` events, but the exact ordering relative to other observer events is documented as the current implementation behavior, not a stable contract.

Current selected-output observer sequence for a branch run is:

- `on_stage_start(branch)` when the branch node starts
- `on_stage_start(predicate)` / `on_stage_success(predicate)` for each evaluated predicate
- `on_case_skipped(branch, index, predicate)` for a false predicate
- `on_case_selected(branch, index, predicate)` for the first true predicate
- `on_stage_start(case_stage)` / `on_stage_success(case_stage)` for the selected case stage
- `on_stage_start(join_stage)` / `on_stage_success(join_stage)` for a following join stage, when present

`on_stage_failure(...)` and `on_stage_exception(...)` are used when the predicate, case stage, or join stage fails or throws; the branch-specific callbacks do not replace the standard failure callbacks. Explicit fan-in additionally reports failed predicate/stage slots through `on_case_failed(...)` before the join receives the aggregate.

### Unnamed stage identity collisions
When predicates and branch stages do not define `stage_key()` / `stage_name()`, the runtime generates deterministic fallback identities such as `branch.case.0.predicate` and `branch.case.0.stage` so observer/error output can distinguish branch children. Explicit keys are still recommended for stable user-facing diagnostics.

### Branch exhaustiveness
There is no compile-time enforcement that branch cases cover all possible inputs. Unmatched inputs produce a runtime `contract_violation` error.

### Parallel/async backends
The sequential runtime (`pb::runtime::sequential`) is supported for selected-output and fan-in branch execution. The standard-library `pb::runtime::thread_pool_backend` is supported for the current fan-in backend slice. oneTBB, Taskflow, stdexec, preemptive cancellation, backend examples, and backend benchmarks remain roadmap work.

### Stable graph export
DOT/JSON helper output exists for supported branch and fan-in shapes, and JSON now reports `branch` or `fan_in` topology for those pipelines. Stable descriptor/export schemas, release-grade external compatibility fixtures, CLI/file export, and backend scheduling/trace graph-export semantics are not yet supported.

## Related pages

- [Branch / Join Roadmap / Status](branch-join-roadmap.md)
- [Production Readiness Status](production-readiness.md)
- [Roadmap Gap Map](roadmap-gap-map.md)
