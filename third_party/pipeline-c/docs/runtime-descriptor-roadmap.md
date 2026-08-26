# Runtime Descriptor Roadmap / Status

The repository now ships a **runtime-descriptor helper** for validated linear, selected-output branch, and explicit fan-in pipelines. Treat that helper as a metadata projection for the current supported linear, selected-output branch, and explicit fan-in surfaces, **not** as a general export/graph contract or a cross-release stability promise.

## Current status

Today the repository supports:

- compile-time pipeline metadata/introspection through `describe()` and stage records
- public `pipeline_descriptor` metadata for validated linear pipelines
- a fixed-size runtime descriptor projection for validated linear/branch/fan-in pipelines through `pb::runtime::descriptor_view`, `pb::make_descriptor<Pipeline>()`, `pb::make_branch_descriptor<Pipeline>()`, and `engine.descriptor()`
- sequential runtime execution for the current MVP linear/branch model and thread-pool backend descriptors for supported fan-in pipelines

Today the repository does **not** support:

- a stable runtime descriptor/export contract with cross-release compatibility promises
- a general runtime graph view for arbitrary non-linear topologies beyond selected-output branch and explicit fan-in
- public APIs that lower pipeline metadata into a durable graph/export format beyond the current helper
- stable descriptor-backed DOT/JSON export compatibility
- broad compatibility guarantees beyond the current fixed helper shape

Keep release notes and examples aligned with that boundary. The current helper, descriptor alias symmetry, linear descriptor/error/observer identity checks, and `pb::runtime::error_record` strengthen current introspection/diagnostic projections, but they do not create a stable graph export surface, a non-linear runtime graph view, or a durable cross-release export contract.

## Current implemented descriptor helper for the supported MVP

The repository already exposes a small, fixed descriptor shape for validated pipelines. Linear pipelines use only stage/edge records; selected-output branch and explicit fan-in pipelines additionally expose branch-case identity records and a topology marker.

### `pb::runtime::descriptor_stage_record`

| Field | Type | Meaning |
| --- | --- | --- |
| `index` | `std::size_t` | Zero-based stage position in pipeline order. |
| `key` | `std::string_view` | Stage key reported by the validated pipeline metadata. |
| `name` | `std::string_view` | Stage display name reported by the validated pipeline metadata. |

### `pb::runtime::descriptor_edge_record`

| Field | Type | Meaning |
| --- | --- | --- |
| `index` | `std::size_t` | Zero-based edge position between adjacent stages. |
| `from_stage_index` | `std::size_t` | Source stage index for the edge. |
| `to_stage_index` | `std::size_t` | Target stage index for the edge. |
| `from_key` | `std::string_view` | Source stage key. |
| `from_name` | `std::string_view` | Source stage name. |
| `to_key` | `std::string_view` | Target stage key. |
| `to_name` | `std::string_view` | Target stage name. |

### `pb::runtime::descriptor_view<StageCount, CaseCount, Topology>`

The view is a fixed-size aggregate with:

- `stage_count == StageCount`
- `case_count == CaseCount`
- `topology == Topology`, currently `linear`, `branch`, or `fan_in`
- `edge_count == StageCount > 0 ? StageCount - 1 : 0`
- `empty == (StageCount == 0)`
- `stages` as an `std::array<stage_record, StageCount>`
- `edges` as an `std::array<edge_record, edge_count>`
- `branch_cases` as an `std::array<branch_case_record, CaseCount>`

The view preserves pipeline order. Stage records and edge records are indexed in the same pipeline order as the validated pipeline, and branch/fan-in cases are indexed by branch-stage index plus case index. It is produced by:

- `pb::make_descriptor<Pipeline>()` for direct metadata projection
- `pb::make_branch_descriptor<Pipeline>()` when branch/fan-in case metadata is desired explicitly
- `engine.descriptor()` on sequential and thread-pool backend engines for the same validated pipeline

### Stability rules

- The helper schema covers linear, selected-output branch, and explicit fan-in topology markers; arbitrary graph schemas remain future work.
- Field names and ordering are intentionally fixed so docs, tests, and tooling can refer to them consistently.
- Keys and names are borrowed views; they reflect stage metadata and are not owning strings.
- The schema is suitable for compile-time metadata inspection and deterministic test assertions.
- The helper exists today and is tested, but it does **not** imply a stable runtime export format, graph serialization contract, non-linear graph contract, or cross-release ABI promise.

## Why an exportable runtime descriptor matters

The research plan treats an exportable runtime descriptor as the boundary between template-heavy compile-time metadata and tooling/runtime consumers that need a stable representation. That kind of descriptor would eventually support:

- graph export to human-readable formats such as DOT or JSON
- stable tooling integration without exposing internal template structure
- runtime graph inspection and visualization
- future backend lowering or trace/reporting surfaces that should not depend on raw compile-time internals

Without that descriptor layer, current introspection remains useful but intentionally limited to compile-time-oriented metadata helpers.

## Intended scope relative to the research plan

The research plan treats the feature as planned work, not current behavior:

- it explicitly says the graph should be exportable at compile time into a runtime descriptor
- it sketches future `export_dot` / `export_json` usage built on descriptor-style metadata
- it warns that graph export should avoid leaking unstable internal template details
- it sketches a future runtime graph/view shape separate from the current public compile-time descriptor helpers

The current repository documents the direction, but it does not yet ship the broader runtime-facing representation or compatibility guarantees needed to claim stable export support.

For selected-output branch and explicit fan-in pipelines, the current runtime descriptor helpers now also expose deterministic branch-case identity metadata:

- `case_id` / `case_key` in the form `branch.<branch_stage_index>.case.<case_index>`
- `predicate_node_id` in the form `branch.<branch_stage_index>.case.<case_index>.predicate`
- `stage_node_id` in the form `branch.<branch_stage_index>.case.<case_index>.stage`

Those fields strengthen helper stability for the supported branch and fan-in slices, but they still do not imply a stable runtime export contract or broader graph/schema compatibility promise.

## Non-goals for the current MVP

The current MVP should **not** claim:

- a stable runtime descriptor ABI or export schema guarantee across releases
- guaranteed descriptor compatibility across releases
- public export APIs built on today’s compile-time metadata alone
- a runtime graph view that tools can depend on as supported surface

Those decisions belong to later slices with explicit API definitions, tests, and export contracts.

## What would be required before claiming support

Before the current helper can graduate into a broader supported export contract, the repo needs:

1. **A defined public export/compatibility contract**  
   Stable names, fields, ownership/lifetime rules, and compatibility expectations beyond the current helper.
2. **Lowering from validated pipeline metadata**  
   A supported path from compile-time descriptors/stage records into a broader export/runtime surface without exposing unstable implementation details.
3. **Targeted tests and examples**  
   Positive checks for export/compatibility paths plus negative checks for unsupported graphs or unstable metadata.
4. **Tooling/export integration**  
   Clear linkage to graph export or visualization consumers once those features exist.
5. **Documentation and compatibility guidance**  
   Explicit notes about what is stable, what is versioned, and what remains experimental.

## Verification status today

The current verification evidence covers the implemented helper and adjacent runtime metadata behavior, not a stable runtime descriptor/export contract:

- compile-pass coverage
- compile-fail diagnostic coverage
- runtime smoke coverage
- package-consumer smoke coverage
- benchmark smoke scaffolding

Concrete proof points already in-tree include:

- `include/pb/runtime/descriptor.hpp`
- `include/pb/runtime/sequential.hpp` (`engine.descriptor()`)
- `tests/compile_pass/public_headers/runtime_descriptor.cpp`
- `tests/compile_fail/runtime_make_descriptor_non_pipeline_misuse.cpp`
- `tests/runtime/descriptor_smoke.cpp`
- `tests/compile_pass/export_golden.cpp`

## Release guidance

Release notes and docs should describe the current runtime descriptor as:

> a tested metadata helper for linear, selected-output branch, and explicit fan-in pipelines, but not a stable graph export contract or a broader runtime-descriptor compatibility guarantee

If a future slice broadens the helper into a stable export/runtime contract, update this page together with:

- `docs/production-readiness.md`
- graph-export docs that depend on descriptor-backed export
- examples/tests that prove the descriptor shape and stability contract
