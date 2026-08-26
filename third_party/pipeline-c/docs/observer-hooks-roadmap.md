# Observer Hooks Roadmap / Status

Observer hooks are **partially** supported in the current tree for the sequential runtime path. Treat this page as a roadmap/status note for contract hardening and future cross-runtime semantics.

## Current status

Today the repository supports:

- linear compile-time pipeline validation through `pb::from<T>::then<S>::to<U>`
- compile-time metadata/introspection through `describe()` and stage records
- sequential runtime execution with the current result/error plumbing
- zero-stage identity pipelines that execute without emitting stage observer callbacks
- a public observer interface and callback registration API
- runtime `set_observer(...)` hooks on the shipped sequential execution surface
- structured start/success/failure/exception event hooks
- runtime observer coverage in existing smoke tests

For the current branch-aware sequential slice, the event vocabulary also includes branch case callbacks:

- case selected
- case skipped
- case failed for explicit fan-in predicate/stage failures

Keep release notes and examples aligned with that boundary: sequential observer hooks are supported, while event schema and long-term ABI/lifecycle guarantees are not yet finalized.

## Why observer hooks matter

Observer hooks are part of the project’s operational story, not just a convenience feature. The research plan treats them as a way to make runtime execution easier to inspect, debug, and measure without forcing users to infer behavior from compiler backtraces or ad hoc logging.

For this project, observer hooks would eventually help with:

- stage-level execution visibility
- structured error/event reporting
- optional trace export and runtime diagnostics
- measuring feature cost when tracing is disabled, compiled out, or enabled

## Intended scope relative to the research plan

The research plan treats observer hooks as planned work, while the core sequential callback path is now implemented and tested:

- it explicitly calls for observer hooks alongside graph export and test harness improvements
- it sketches a future runtime surface that includes `set_observer(pb::observer*)`
- it lists runtime tests for broader observer contracts as future verification work
- it tracks follow-on trace/export work and cost measurement as separate slices

In other words, the current repo ships an operational observer hook surface on the sequential runtime path; it does not yet claim full contract completeness for ABI stability, event schema, cross-executor behavior, or documented long-horizon policy.

## Non-goals for the current MVP

The current MVP should **not** claim:

- stable observer ABI or ownership rules
- built-in logging/tracing sinks
- guaranteed event schemas or trace file formats
- backend-agnostic observer behavior across future executors

Those decisions belong to a later implementation slice with tests, examples, and explicit runtime contracts.

## What would be required before claiming support

Before observer hooks can move from partial support to fully stable support, the repo needs:

1. **A defined public API contract**  
   Stable type names, lifetime/ownership rules, event semantics, and enable/disable behavior.
2. **Runtime implementation coverage**  
   Cross-executor behavior and lifecycle coverage beyond existing sequential smoke paths.
3. **Targeted tests**  
   Positive runtime tests plus boundary/negative tests for invalid sinks, disabled tracing paths, or unsupported event flows.
4. **Documentation and examples**  
   One supported usage path and one clearly bounded failure/limitation example.
5. **Cost validation**  
   Evidence for tracing-disabled vs tracing-enabled overhead before treating hooks as production-ready.

## Verification status today

The current verification evidence includes:

- compile-pass coverage
- compile-fail diagnostic coverage
- runtime smoke coverage, including observer callback behavior on the sequential engine and zero-stage no-callback behavior
- package-consumer smoke coverage
- benchmark smoke scaffolding

There is currently no observer example or observer benchmark lane; current coverage is runtime-smoke-centric.

## Release guidance

Until contract-hardening work lands, release notes and docs should describe observer hooks as:

> partial sequential support; ABI and cross-executor observer contracts remain roadmap

If a future slice adds observer hooks, update this page together with:

- `docs/production-readiness.md`
- any runtime API docs or examples that expose the hook surface
- the relevant tests/bench entries that validate event behavior and cost
