# Diagnostic Example Walkthrough

This walkthrough explains what `examples/error_diagnostic.cpp` demonstrates today, how to inspect its intentionally failing path, and how that example relates to the repository's dedicated smoke tests.

## What the example is for

`examples/error_diagnostic.cpp` is a teaching example for compile-time pipeline validation. Its main job is to show what a stage-connection mismatch looks like in a small, copyable program without breaking the default example build.

The file contains two distinct paths:

- A normal executable path that only constructs and runs `ParseOrder`, then exits successfully.
- A macro-gated invalid pipeline path that exists only when `PB_EXAMPLE_ENABLE_COMPILE_FAILURE` is defined.

That split lets the repository ship one source file that is both buildable as an ordinary example and reusable as a stable compile-fail diagnostic fixture.

## The example's normal runtime behavior

Without extra definitions, `pb_error_diagnostic_example` builds and runs as a regular executable.

At runtime it only does one small thing:

1. Adapts `legacy::parse_order` into `ParseOrder`.
2. Invokes `ParseOrder` on `domain::RawText{"order-1001"}`.
3. Discards the resulting draft and returns `0`.

This means the default example does **not** run the invalid pipeline. Its runtime path is intentionally boring so the example target stays green in ordinary builds.

Build just that target with:

```bash
cmake --build --preset clang-dev-ninja --target pb_error_diagnostic_example
```

If the executable runs successfully, it proves the source file still compiles in its non-failing mode and that the adapted parse stage remains usable as a normal example snippet.

## The intentional compile-fail path

The interesting part is behind this definition:

```cpp
PB_EXAMPLE_ENABLE_COMPILE_FAILURE
```

When enabled, the example declares:

- `ParseOrder`, which produces `domain::OrderDraft`
- `PersistOrder`, which incorrectly declares `domain::Receipt` as its `input_type`
- `BrokenPipeline`, which tries to connect those incompatible stages

The follow-up `static_assert(pb::valid<BrokenPipeline>);` is expected to fail. The current diagnostic contract is that the compiler output includes `Pipeline edge mismatch`.

Use the dedicated CTest entry instead of hand-writing a compile command:

```bash
ctest --preset clang-dev-ninja --output-on-failure -R pb_example_error_diagnostic_compile_fail
```

That test compiles `examples/error_diagnostic.cpp` through `tests/run_compile_fail.cmake` with `PB_EXAMPLE_ENABLE_COMPILE_FAILURE` defined and checks for the expected diagnostic substring.

## How it relates to the smoke tests

The repository uses three separate checks around this example area:

1. `pb_example_error_diagnostic` — runs the example in its normal non-failing mode.
2. `pb_example_error_diagnostic_compile_fail` — forces the invalid pipeline path and verifies the compile-time edge-mismatch diagnostic.
3. `pb_runtime_error_diagnostic_smoke` — a separate runtime diagnostic test that covers runtime error formatting/helpers, not this compile-time example.

That separation matters:

- The example source documents compile-time shape mismatches.
- The compile-fail test locks the diagnostic wording contract.
- The runtime diagnostic smoke test covers runtime error metadata and formatting behavior elsewhere in the library.

## Current limitations

Users should keep these boundaries in mind:

- This example demonstrates a compile-time type-edge mismatch, not a runtime failure path.
- The default executable path does not exercise `BrokenPipeline`; it only keeps the file buildable as an example.
- The compile-fail contract is substring-based (`Pipeline edge mismatch`), so it proves the high-level diagnostic category rather than every exact compiler line.
- The example does not demonstrate richer runtime error propagation, observer hooks, graph export, branch/join behavior, or optional backend execution.
- If you need runtime diagnostic behavior, use the runtime smoke tests and runtime docs rather than treating this example as a runtime error tutorial.

For the current supported contract, treat this walkthrough plus `docs/examples.md` and `examples/CMakeLists.txt` as the source of truth for the example/test relationship.
