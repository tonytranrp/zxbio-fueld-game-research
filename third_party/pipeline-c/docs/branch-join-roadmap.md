# Branch / Join Roadmap / Status

Sequential branch execution is now a **supported, tested feature** for homogeneous branch outputs with optional join stages. A first heterogeneous-output slice is also implemented: differing branch outputs are represented as `std::variant<...>` and can be joined either by a stage that accepts that variant or by a type-list join stage that declares `pb::meta::type_list<...>` and overloads every raw branch output type. Variant joins preserve duplicate output alternatives by index. Type-list selected-output joins dispatch by C++ type; duplicate same-type branch outputs share the same overload unless the user encodes case identity into the output type. Move-only branch inputs are supported when predicates inspect by `const input_type&` and the selected branch stage consumes the value. A fan-in slice is now implemented through explicit `::fan_in<JoinStage>` / `::join_all<JoinStage>` APIs that evaluate all predicates and pass an ordered `pb::fan_in_results<...>` aggregate to the join stage. The sequential backend runs passing cases in declaration order; `pb::runtime::thread_pool_backend` schedules passing case stages concurrently while preserving declaration-order aggregate results. Fan-in case slots preserve `skipped`, `completed`, and `failed` states, including diagnostic text for predicate/stage failures; later cases continue after earlier failures so the join can inspect the aggregate.

## Current status

Today the repository supports:

- linear compile-time pipeline validation through `pb::from<T>::then<S>::to<U>`
- compile-time metadata/introspection through `describe()` and stage records
- sequential runtime execution for validated linear pipelines
- **public branch/join DSL**: `pb::from<Input>::branch<CaseA, CaseB>::to<Output>`, `pb::from<Input>::branch<CaseA, CaseB>::join<JoinStage>::to<Output>`, and explicit all-passing sequential fan-in via `::fan_in<JoinStage>` / `::join_all<JoinStage>`
- **compile-time join validation**: `pipeline_state::join` validates branch context (must follow `::branch<...>`, join stage input must match the unified branch execution output)
- **runtime branch execution**: predicate evaluation, first-match-wins case selection with short-circuit, branch-stage execution, observer events (`on_case_selected`, `on_case_skipped`), error annotation with `[branch]` prefix
- **branch child identities**: unnamed branch predicates/stages receive runtime fallback keys such as `branch.case.0.predicate` and `branch.case.0.stage` so observer/error events do not collapse to the parent branch stage index
- **branch case labels**: cases may opt into helper/export labels with `pb::case_<Predicate>::label<"case-label">::then<Stage>`; unlabeled cases remain source-compatible and helper output displays the case index as the label
- **stateful branch execution**: branch predicates and stages respect `pb::runtime::stateful_sequential` policy, preserving stage state across multiple runs
- **heterogeneous selected-output branches**: selected branch nodes return `std::variant<Ts...>` when case outputs differ and join stages can consume either that variant or raw `pb::meta::type_list<...>` metadata through overload-based selected-output dispatch
- **move-only selected-output branch inputs**: predicates observe an lvalue reference and the selected branch stage receives the moved input; coverage includes `std::unique_ptr` ownership transfer into a by-value branch stage
- **sequential fan-in branch execution**: `::fan_in<JoinStage>` / `::join_all<JoinStage>` evaluates all predicates, runs every passing branch stage in declaration order, treats zero passing predicates as a valid aggregate, records predicate/stage failures in the per-case slots, supports void-output case aggregation, and passes `pb::fan_in_results<pb::fan_in_case_result<I, T>...>` to the join stage
- branch-aware export helpers: DOT includes branch/case structure and JSON now reports branch topology instead of always reporting linear topology
- branch marker aliases (`case_`, `branch_case`, `branch_node`, `join_node`) plus `branch_case_output` / `branch_outputs` marker metadata, raw output type-list helpers, unified branch output helpers, homogeneous branch-output validation, unified-output validation, branch source compatibility, predicate marker, homogeneous branch-node case-input, branch-output compatibility validation, join consumption validation, invalid join-stage, and branch-output marker misuse diagnostics
- runtime route descriptor and ordered `select_route(...)` helper
- supported branch/join examples (`branch_routing_demo.cpp`, `branch_error_handling.cpp`)
- comprehensive branch runtime tests (multi-case routing, predicate errors, predicate exceptions, stage exceptions, observer events, try_run with join, stateful, multiple runs)

## Branch output metadata vocabulary

`pb::branch_outputs<Cases...>` exposes two intentionally different output views:

- `output_types` / `pb::branch_raw_output_types_t<Cases...>` is raw metadata: a `pb::meta::type_list<...>` preserving every case-stage output type in case order.
- `output_type` / `pb::branch_unified_output_t<Cases...>` is the execution output: a single `T` for homogeneous branches or `std::variant<Ts...>` for heterogeneous branches.

The selected-output branch type model intentionally does **not** add a tuple-style or
per-case tagged result wrapper:

- raw branch case outputs stay in `pb::meta::type_list<...>` for validation and
  metadata
- execution uses a single selected-output value, either `T` or `std::variant<...>`
- the selected-output sequential branch path does not produce `std::tuple<...>` or a
  tagged-per-case result container; explicit fan-in uses the separate
  `pb::fan_in_results<...>` aggregate
- `pb::runtime::result<T>` remains orthogonal and is only for value-or-error
  reporting around a stage result, not for encoding branch multiplicity

`pb::branch_output_validation<Outputs, Output>` remains the homogeneous compatibility check that every raw case output is exactly `Output`. `pb::branch_unified_output_validation<Outputs, Output>` checks the execution output contract and should be used when validating a heterogeneous branch against its `std::variant` join input.

## Type model

For the current branch/join slice, the type model is intentionally split into three layers:

- **Raw case metadata**: `pb::meta::type_list<...>` preserves the per-case output types in case order. This is the right mental model for “all case output types as a list,” not a runtime tuple value.
- **Execution output**: a homogeneous branch yields a single `T`; a heterogeneous branch yields `std::variant<Ts...>`. This is the value that runtime routing and joins consume.
- **Join-stage dispatch**: a join stage may consume either the heterogeneous `std::variant` value or the raw `pb::meta::type_list<...>` metadata through overload-based selected-output dispatch.

What selected-output joins do **not** currently mean:

- a runtime `std::tuple` of per-case values
- optional/result-per-case fan-out semantics
- tagged per-case result wrappers as the default selected-output branch model

For all-passing fan-in, the runtime value model is explicit: a join stage must
accept `pb::fan_in_results<pb::fan_in_case_result<I, T>...>`. Each slot is
`skipped`, `completed`, or `failed`. Failed slots retain a diagnostic string through
`diagnostic_message()`, and void-output stages use `pb::fan_in_case_result<I, void>`
so joins can count completed/skipped/failed cases without inventing placeholder
values.

If future work introduces richer error-envelope policies, cancellation rules, or tagged selected-output results, it should be documented as a new slice with its own tests and compatibility notes rather than retrofitted into the current supported model.

Today the repository does **not** yet support:

- dependency backend fan-in or backend multi-input join lowering beyond the current sequential/thread-pool fan-in slices
- backend/parallel failure aggregation, cancellation, and scheduling policy beyond the current sequential aggregate
- predicate patterns that require consuming a move-only input before routing; predicates must remain callable with `const input_type&`
- by-value fan-in stages for non-copyable inputs unless the input is copy-constructible; the supported move-only fan-in path requires every passing case stage to borrow via `const input_type&`
- stable external descriptor/export compatibility for fan-in topology, or dependency-backend branch execution

## Why branch/join matters

Branch/join support is the main step from a linear pipeline DSL to a richer graph model. The research plan treats it as the feature that enables:

- parallel or conditional flow through multiple stage paths
- explicit join points after divergent processing
- richer graph export and execution backends
- diagnostics for multi-input and multi-output pipeline shapes

Without this feature, the current library remains intentionally limited to one validated chain of stages.

## Scope relative to the research plan

The research plan originally treated branch/join as post-MVP work after the linear chain. The current repository now ships the first production-readiness slice of that plan: homogeneous sequential branch/join execution with public DSL, validation, runtime routing, observer events, stateful storage, tests, and examples.

The broader graph-shaped plan is still not complete. Heterogeneous branch outputs, move-only selected-output branch inputs, and sequential fan-in now have implementation slices, including duplicate-output variant routing, explicit raw-vs-unified output validation helpers, negative coverage for move-only predicates that would consume by value, failed-case fan-in aggregation, void-output fan-in aggregation, borrowed-input fan-in for move-only values when all branch stages accept `const input_type&`, and an ordered fan-in aggregate for all cases. Thread-pool backend fan-in now has an implementation slice; preemptive cancellation, dependency backend fan-in, stable external descriptor/export compatibility, release-grade golden export schemas, and broad backend branch execution remain roadmap work.

## Non-goals for the current MVP

The current MVP should **not** claim:

- backend/parallel multi-input stage support before backend join lowering exists
- multi-output branch lowering for ordinary stage outputs
- feedback/cycle execution
- stable join result policy

Those decisions belong to a later implementation slice with explicit tests, examples, and executor contracts.

## What is supported for branch/join

1. **Public graph-builder API** ✅  
   `pb::from<Input>::branch<CaseA, CaseB>::to<Output>` and `pb::from<Input>::branch<CaseA, CaseB>::join<JoinStage>::to<Output>` with compile-time validation.
2. **Compile-time validation coverage** ✅  
   Checks for branch predicates, join context, and type compatibility. Branch-output and join validation have accepted evidence.
3. **Runtime execution coverage** ✅  
   Sequential branch routing with first-match-wins short-circuit, observer events, error propagation, stateful storage, const-input predicate evaluation, join stage execution, variant joins and type-list selected-output joins for heterogeneous outputs including duplicate output alternatives by index, move-only input consumption by the selected branch stage, and explicit sequential fan-in joins for zero/one/many passing cases, failed predicate/stage slots, void-output cases, and borrowed move-only inputs.
4. **Targeted tests and examples** ✅  
   Positive runtime tests, negative compile-fail diagnostics, branch export compile-pass tests, and user-facing examples.

## Remaining roadmap items

- dependency backend fan-in / true backend multi-input join execution outside the standard-library thread-pool slice
- preemptive cancellation and richer backend fan-in failure policies
- Stable external descriptor/export compatibility with schema docs and release-grade golden outputs
- Parallel/async backend support for branch execution
- Broader move-only edge coverage beyond const-reference predicates plus selected-stage consumption

## Verification status today

- compile-pass coverage including `branch_join_validation_compile_pass.cpp`
- compile-fail diagnostic coverage for join-without-branch, join-type-mismatch, and 15+ other branch/join boundary diagnostics
- runtime branch execution tests: `sequential_branch_smoke.cpp`, `sequential_branch_execution_smoke.cpp`, `sequential_branch_comprehensive.cpp`, `sequential_branch_heterogeneous.cpp` (including duplicate-output `std::variant` routing), and `sequential_branch_move_only.cpp`
- user-facing branch examples: `branch_routing_demo.cpp` (3-case document routing + join), `branch_error_handling.cpp` (error propagation)
- observer event coverage for branch case selection/skipping and unnamed predicate/stage fallback identities
- stateful branch storage tested via `sequential_branch_comprehensive.cpp`
- graph helper coverage: `pb_export_dot_branch_compile_pass`, `pb_export_json_compile_pass`, `pb_export_json_branch_compile_pass`, and helper-output regression coverage in `pb_export_golden_compile_pass`

Preemptive cancellation, dependency backend fan-in, stable external descriptor/export compatibility, and broad backend branch execution remain future slices.

## Release guidance

Release notes and docs should describe branch/join support as:

> sequential branch routing with optional selected-output join stages is supported for homogeneous outputs, heterogeneous outputs through `std::variant` or type-list selected-output joins (including duplicate output alternatives by index), move-only selected-branch input consumption, and explicit sequential fan-in joins through `::fan_in` / `::join_all` with skipped/completed/failed slots, void-output cases, and borrowed move-only inputs; preemptive cancellation/error policy extensions, dependency backend fan-in, stable external descriptor/export compatibility, and broad backend branch execution remain roadmap work

If a future slice adds branch/join support, update this page together with:

- `docs/production-readiness.md`
- graph-export and observer/tracing docs when those surfaces begin to depend on graph topology
- the relevant tests/examples/bench entries that validate topology, diagnostics, and runtime behavior
