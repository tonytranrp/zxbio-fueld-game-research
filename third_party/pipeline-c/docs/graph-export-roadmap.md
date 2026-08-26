# Graph Export Roadmap / Status

Graph export is currently a **descriptor-backed helper surface** for supported
linear, selected-output branch, and explicit fan-in pipelines, not a stable graph interchange contract. The
current implementation lives in `include/pb/core/export_json.hpp` and
`include/pb/core/export_dot.hpp`, with schema data sourced from
`include/pb/runtime/descriptor.hpp`.

The current helper output is useful for local visualization and regression
testing, but it remains narrower than a release-grade export schema.
The current smoke coverage also checks the runtime descriptor helper strings
that mirror the helper schema fields documented in `export-helper-schema.md`.

At the JSON helper boundary, the current top-level keys are
`schema_version`, `topology`, `stage_count`, `edge_count`, `stages`, and
`edges`.

## Current status

Today the repository supports:

- linear, branch-aware, and fan-in-aware DOT helper output
- JSON helper output with `topology` reporting for linear, `branch`, and `fan_in` pipelines
- descriptor-backed helper rendering for stage, edge, and branch-case records
- targeted compile-pass coverage for JSON/DOT helper strings and runtime
  descriptor smoke coverage

Today the repository does **not** support:

- a stable export schema contract
- CLI/file export of user pipeline definitions
- backend scheduling/trace graph export semantics
- release-grade compatibility guarantees for helper field names or ordering

## Current helper boundary

The current helper boundary is:

```text
pipeline type -> runtime descriptor -> JSON/DOT helper output
```

The helper schema currently exposes these top-level JSON fields:

Current branch/fan-in case helper descriptors also carry deterministic identity fields (`case_id` / `case_key`, `predicate_node_id`, `stage_node_id`) so branch graph rendering and helper tests can refer to a stable per-case identity without claiming a stable external graph schema.

## Planned implementation checkpoints

In other words, the current selected-output branch-stage helper shape is `kind = "branch"`, the explicit fan-in branch-stage helper shape is `kind = "fan_in"`, and both carry a nested `branch_cases` array.

Runtime descriptor branch metadata such as branch-stage indices and branch-case
type names stays behind the helper boundary for now.

The current sequential branch observer events already cover:

- branch start / success / failure
- case selected / skipped
- stage start / success / failure / exception on the predicate and case-stage
  nodes

Those observer events are exercised by the runtime branch smoke and
comprehensive branch execution tests rather than by the helper-schema page.

See `docs/export-helper-schema.md` for the current field-level summary.

Relevant helper-schema source files:

- `include/pb/runtime/descriptor.hpp`
- `include/pb/core/export_json.hpp`
- `include/pb/core/export_dot.hpp`
- `tests/compile_pass/export_json.cpp`
- `tests/compile_pass/export_json_branch.cpp`
- `tests/compile_pass/export_dot.cpp`
- `tests/compile_pass/export_dot_branch.cpp`
- `tests/runtime/descriptor_smoke.cpp`

## Why this remains roadmap work

The current helper output is intentionally limited:

- it is not a versioned public schema
- it is not a CLI/file export format
- it is not a promise of stable field ordering or long-term compatibility

That means future schema changes should still be treated as roadmap work until
the helper surface is explicitly promoted.

## Verification status today

Current targeted evidence includes:

- `tests/compile_pass/export_json.cpp`
- `tests/compile_pass/export_json_branch.cpp`
- `tests/compile_pass/export_dot.cpp`
- `tests/compile_pass/export_dot_branch.cpp`
- `tests/compile_pass/export_golden.cpp`
- public-header coverage for the export helpers
- `tests/runtime/descriptor_smoke.cpp`

## Release guidance

Release notes and docs may describe the current slice as:

> Descriptor-backed DOT/JSON helper export for linear, selected-output branch, and explicit fan-in pipelines, including `branch` and `fan_in` topology in JSON; not a stable graph schema and not a CLI/file export contract.

If a future slice stabilizes the export contract, update this page together with:

- `docs/production-readiness.md`
- `docs/examples.md`
- `docs/runtime-descriptor-roadmap.md`
- the relevant helper tests/examples
