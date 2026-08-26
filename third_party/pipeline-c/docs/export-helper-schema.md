# Export Helper Schema

This page documents the current helper-output shape produced by
`pb::core::to_json<Pipeline>()` / `pb::to_dot<Pipeline>()` and the supporting
runtime descriptor records in `include/pb/runtime/descriptor.hpp`.

As of v0.1.0 this page describes the **v1 stable schema** — see the
[formal schema spec](export-schema-v1.md) for the v1 contract, conformance
test list, and migration policy. The v1 output shape is regression-tested
byte-equal across linear, selected-output branch, and explicit fan-in
pipelines.

## Scope

The current helper output is backed by runtime descriptors and is intended for:

- local visualization
- smoke/regression tests
- repository-internal documentation

The roadmap page keeps the same boundary language so the helper schema and
export status notes stay aligned.

It does **not** promise:

- a versioned public export contract
- CLI/file export of user pipeline definitions
- long-term field stability beyond the current helper tests
- backend-specific scheduling graphs or parallel execution trace export semantics

## JSON helper shape

The helper JSON currently uses these top-level fields, in the current helper output shape:

- `schema_version`: `"pb.core.graph.v1"`
- `topology`: `"linear"`, `"branch"`, or `"fan_in"`
- `stage_count`
- `edge_count`
- `stages`: array of stage records
- `edges`: array of edge records

These are the current top-level JSON fields emitted by the helper. The helper
schema does not currently add any other top-level keys.

These are the current top-level field names asserted by the helper tests:

- `schema_version`
- `topology`
- `stage_count`
- `edge_count`
- `stages`
- `edges`

The helper currently writes only those top-level fields, in that order.
`schema_version` identifies the helper schema, `topology` distinguishes linear
from branch graphs, `stage_count` and `edge_count` summarize the graph shape,
and `stages` / `edges` carry the detailed records. String fields are emitted
with JSON escaping for quotes, backslashes, common control escapes, and remaining
ASCII control bytes as `\u00xx`.

### Stage records

- `"linear"`
- `"branch"`
- `"fan_in"`

For the current helper surface, the serialized object is shaped like:

```json
{
  "schema_version": "...",
  "topology": "linear|branch|fan_in",
  "stage_count": 0,
  "edge_count": 0,
  "stages": [],
  "edges": []
}
```

This page documents the helper output that existing compile-pass and runtime smoke tests assert today; it does **not** promise a durable external schema or field-order compatibility across future releases.

## Stage records

Each stage entry currently includes:

- `index`
- `key`
- `name`
- `kind`

#### Branch stage records

Selected-output branch stages use `kind = "branch"`. Explicit all-branches fan-in stages use `kind = "fan_in"`.
They also include `branch_cases`, an array of branch-case records for that
branch stage.

Both branch shapes include `branch_cases`; non-branch stages use `kind = "stage"`.

### Branch-case records

In other words, the current helper schema treats selected-output and fan-in branch stages as normal serialized stage records with a branch-specific `kind` marker (`branch` or `fan_in`) and an additional `branch_cases` array. That is the current helper-output shape, not a commitment to preserve this exact representation forever.

The underlying runtime descriptor records also carry branch-stage-local type-name fields, but the current JSON/DOT helper schema does not promote those fields to the serialized helper contract.

Current runtime and compile-pass coverage also checks the branch node `key` and `name` strings and the nested branch-case `predicate_*` / `stage_*` strings.

## Branch case records

Each branch case entry currently includes:

- `index`
- `case_id`
- `case_key`
- `case_label`
- `predicate_node_id`
- `stage_node_id`
- `predicate_key`
- `predicate_name`
- `stage_key`
- `stage_name`
- `predicate_edge`
- `stage_edge`

In plain terms, the current branch-case helper record names the selected case by
`index`, exposes `case_label` as the user-supplied label or the case index
fallback, then records the predicate and stage identifiers/names plus two
helper-rendered edge objects:

- `predicate_edge` for the helper-rendered branch-to-predicate edge
- `stage_edge` for the helper-rendered predicate-to-case-stage edge

The current tests assert the branch case keys/names and the presence of the nested edge objects. The nested edge objects are helper-rendered records, not a public stability promise.

- `predicate_edge.from = "branch"`
- `predicate_edge.to = "predicate"`
- `predicate_edge.style = "dashed"`
- `predicate_edge.label = "test"`
- `stage_edge.from = "predicate"`
- `stage_edge.to = "case_stage"`

The underlying runtime record fields come from:

- `pb::runtime::descriptor_stage_record`
- `pb::runtime::descriptor_branch_case_record`
- `pb::runtime::descriptor_edge_record`
- `pb::runtime::descriptor_view<StageCount, CaseCount>`

The runtime descriptor layer also carries branch-stage-local metadata such as
type names and branch-stage indexes, but those fields remain implementation
details rather than part of the serialized helper schema.

### Branch observer events

The current runtime observer/trace surface represents branch lifecycle events
with the generic stage hooks plus the case hooks:

- branch stage start and completion use `on_stage_start`, `on_stage_success`,
  `on_stage_failure`, and `on_stage_exception`
- branch case selection uses `on_case_selected`
- branch case skipping uses `on_case_skipped`
- branch case observers receive the branch stage id, case index, and predicate
  id, while trace export records the branch stage key/name and case index

The branch event kinds currently covered by trace export are:

- `stage_start`
- `stage_success`
- `stage_failure`
- `stage_exception`
- `case_selected`
- `case_skipped`

### Branch type model

The current selected-output branch helper model is:

- raw branch outputs are collected as `pb::meta::type_list<...>`
- the unified branch output type is the corresponding `std::variant<...>`
- branch cases are not wrapped in `optional`, `result`, or a tagged case-result
  object in the helper schema

That model is what the compile-pass and runtime pipeline-state tests assert for
the current branch/join surface.

## DOT helper shape

- `schema_version`
- `topology`
- `kind`
- `branch_cases`
- stage `key` and `name`
- edge `from_key`, `from_name`, `to_key`, and `to_name`
- branch case `case_id`, `case_key`, `predicate_node_id`, and `stage_node_id`
- branch case `predicate_key`, `predicate_name`, `stage_key`, and `stage_name`
- branch case `label` values for labeled and unlabeled cases

- a `digraph` wrapper
- `rankdir=LR`
- a `from_input` node
- a `to_output` node
- linear stage nodes for linear pipelines
- branch diamonds for branch pipelines
- per-case subgraphs for supported branch pipelines

### Branch DOT nodes and edges

For each branch case, the current helper output renders:

- a `branch_<index>` diamond node
- a `cluster_case_<branch_index>_<case_index>` subgraph
- a predicate node labeled `pred: <predicate name>`
- a case-stage node labeled with the stage name
- a dashed `branch -> predicate` test edge
- a `predicate -> case_stage` edge

The branch-aware rendering is driven by:

- `pb::core::detail::append_branch_case_subgraph(...)`
- `pb::core::detail::append_branch_stage_dot(...)`
- `pb::core::detail::dot_emitter<Pipeline, true>`
- `pb::runtime::make_descriptor<Pipeline>()`

## Current validation coverage

The current helper-schema claims are covered by:

- `tests/compile_pass/export_golden.cpp`
- `tests/compile_pass/export_json_branch.cpp`
- `tests/compile_pass/export_dot_branch.cpp`
- `tests/compile_pass/export_json.cpp`
- `tests/compile_pass/export_dot.cpp`
- `tests/compile_pass/public_headers/export_json.cpp`
- `tests/compile_pass/public_headers/export_dot.cpp`
- `tests/runtime/descriptor_smoke.cpp`

## Contract caution

This helper schema documents current output shape only. The regression-tested
portion covers linear pipelines and selected-output branch pipelines, including
branch case identity fields and JSON escaping. If a future change alters field
names, field order, labels, escaping, or branch rendering, the docs and helper
regression tests should be updated together.


### Fan-in helper model

Explicit fan-in pipelines use the same branch-case record shape as selected-output branches, but their branch stage reports:

- top-level `topology = "fan_in"`
- stage record `kind = "fan_in"`
- branch stage key/name from the fan-in marker (`fan_in`)
- deterministic `case_id`, `case_key`, `predicate_node_id`, and `stage_node_id` fields for every case

The helper output remains graph-shape metadata. It does not serialize backend scheduling decisions, worker counts, completion timing, cancellation decisions, or runtime result values. Thread-pool backend execution preserves the same descriptor/export graph shape as sequential fan-in.
