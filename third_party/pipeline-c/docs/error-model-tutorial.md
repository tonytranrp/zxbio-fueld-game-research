# Runtime Error Model Tutorial

This tutorial explains the current runtime error model in `pipebuilder`: how `pb::runtime::result<T>` carries success or failure, what lives inside `pb::runtime::error`, how `try_run()` captures exceptions, how expected-like stage results are normalized, and where the current behavior still has known limits.

## The two core runtime types

The current runtime path revolves around two public types:

- `pb::runtime::result<T>` — a value-or-error wrapper for runtime stage execution.
- `pb::runtime::error` — the structured error payload used when execution does not produce a value.

A stage or executor path that succeeds returns a value. A stage or executor path that fails returns an `error` with enough metadata for logs, tests, and policy decisions.

## What `pb::runtime::error` carries

The current error object is intentionally small and explicit. The most important fields are:

- `category` — high-level failure kind.
- `stage.key` / `stage.name` — where the failure came from.
- `message` — a human-readable explanation.

The current categories used in tests and docs are:

- `stage_failure` — a stage returned a deliberate runtime failure.
- `expected_error` — a stage returned an expected-like error that was normalized into the runtime model.
- `exception` — an exception was caught and converted into runtime error form.
- `contract_violation` — a runtime contract was broken.

`tests/runtime/error_diagnostic_smoke.cpp` is the best current anchor for how these fields are described and formatted.

## Using `result<T>` directly

`pb::runtime::result<T>` is the direct runtime carrier for success/failure-aware stages and helpers.

The current tests show these common operations:

- `has_value()` / `has_error()`
- `value()` / `value_or(...)`
- `error()` / `error_or(...)`
- `to_result(...)` for expected-like normalization
- `make_result(...)` as a convenience builder

That behavior is covered in `tests/runtime/result_smoke.cpp`, which demonstrates:

- value success paths
- explicit `pb::runtime::error` failures
- `result<void>` behavior
- conversion from expected-like objects with `value()` / `error()`
- move-only value normalization

## Stage-local runtime failures

A stage can opt into the runtime error model by declaring `error_type = pb::runtime::error` and returning `pb::runtime::result<Output>`.

That lets the stage return structured failures such as:

- `stage_failure` for domain-specific runtime rejection
- stage metadata (`stage.key` / `stage.name`) for diagnostics
- a message suitable for logs or assertions

The current sequential-result smoke tests use exactly that pattern. It is the clearest route when a stage wants to fail without throwing and still preserve structured metadata.

## `try_run()` and exception capture

`try_run()` is the current runtime path designed to normalize exceptions into `pb::runtime::result<T>`.

`tests/runtime/sequential_try_run_smoke.cpp` shows the supported behavior today:

- a normal throwing pipeline can still return a successful value
- a thrown `std::runtime_error` becomes an `exception` error with stage name and message
- an unknown thrown value becomes an `exception` error with a generic message
- a stage that already returns `pb::runtime::result<T>` preserves its deliberate failure category

If you need one call path that can return either values or structured failures without escaping exceptions to the caller, `try_run()` is the current documented choice.

## Expected-like normalization

The current runtime model also accepts expected-like stage results and normalizes their error payloads.

In practice, that means:

- if a stage result has `value()` / `error()`-style expected semantics, `to_result(...)` can normalize it
- non-`pb::runtime::error` failures become `expected_error`
- the normalized `error.message` keeps the original expected-like error text when possible

This is why `docs/examples.md` currently advises keeping other failure forms (for example `std::string`) as expected-like results and letting the runtime normalize them.

## How this differs from the compile-fail diagnostic example

`examples/error_diagnostic.cpp` is related, but it teaches a different boundary.

That example demonstrates a **compile-time** pipeline edge mismatch, not the runtime error model described here. Use it when you want to show invalid stage wiring. Use the runtime result, error-diagnostic, and `try_run()` tests when you want to explain runtime failure behavior.

## Current limitations and policy boundaries

The current runtime error model is useful, but it is not the final policy surface yet.

Important limits to keep in mind:

- `try_run()` captures stage exceptions into structured `exception` errors.
- `run()` and `try_run()` are not fully harmonized yet; current production-readiness docs treat that split as an active hardening gap.
- The runtime tests show the current supported behavior, but they do not establish richer future policies such as observer hooks, custom retry frameworks, or backend-specific execution guarantees.
- Expected-like normalization exists today, but broader policy customization around error categories and conversion remains future work.
- Compile-time validation, package-smoke behavior, and benchmark evidence are separate concerns and should not be treated as proof of runtime error policy guarantees.

For the current supported contract, prefer this tutorial together with `docs/examples.md`, `docs/production-readiness.md`, and the runtime smoke tests that back the behavior described above.
