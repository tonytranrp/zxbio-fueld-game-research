# Release Readiness Checklist

Use this page as the release-facing companion to [Production Readiness Status](production-readiness.md) and [Build and Verification](build.md). It consolidates the exact checks that are supportable **today** and keeps the roadmap-only gaps visible so a release candidate does not overclaim the current MVP.

For copyable evidence capture fields, use [Release Evidence Template](release-evidence-template.md) while running the commands below.
Use [Research Verification Matrix](research-verification-matrix.md) to check whether roadmap gaps have enough code, tests, and docs evidence to appear in release notes as shipped behavior.

## What is supported today

A release candidate can only claim the currently shipped and tested surface:

- C++20 CMake package targets `pb::core`, `pb::runtime`, and `pb::pipeline`
- linear typed pipeline validation with explicit stage metadata
- sequential runtime execution for validated linear pipelines
- supported sequential branch/join slice: homogeneous outputs, variant-based heterogeneous outputs including duplicate alternatives by index, selected-output type-list joins, move-only selected-stage consumption when predicates observe by `const input_type&`, observer events, and stateful branch predicate/stage storage
- DOT/JSON helper export for linear and supported branch pipelines, including JSON branch topology and DOT label escaping
- compile-pass, compile-fail, runtime, example, package-consumer, and benchmark **smoke** scaffolding
- the intentional compile-fail diagnostic example and the package-consumer contract described in the current docs

Do **not** treat roadmap pages as if they are already releaseable features. Preemptive cancellation, dependency backend fan-in / true backend multi-input joins beyond the thread-pool slice, stable descriptor/export compatibility, fully stabilized observer contracts, a stable runtime descriptor, and richer diagnostics remain separate follow-on slices.

## Release evidence to collect before tagging

Collect fresh command output for the release candidate commit instead of relying on older checkpoints from `continue.md`. The earlier checkpoint is useful as historical context, but release evidence must be regenerated on the candidate SHA.

### 1. Preset inventory

Confirm the documented preset surface is present and uniquely named:

```bash
cmake --list-presets=all
```

Archive the output with the release candidate commit SHA.

### 2. Local developer confidence loop

Run the default Clang development loop so the baseline examples/tests still hold on the candidate tree:

```bash
cmake --preset clang-dev-ninja
cmake --build --preset clang-dev-ninja
ctest --preset clang-dev-ninja --output-on-failure
```

If the release touches docs/examples or diagnostics messaging, also collect the intentional diagnostic-example evidence explicitly:

```bash
cmake --build --preset clang-dev-ninja --target pb_error_diagnostic_example
ctest --preset clang-dev-ninja --output-on-failure -R '^pb_example_error_diagnostic$|^pb_example_error_diagnostic_compile_fail$'
```

### 3. Package/install release gate

Run both package-focused lanes because the current release contract depends on downstream CMake consumer support:

```bash
cmake --preset package-dev-ninja
cmake --build --preset package-dev-ninja
ctest --preset package-dev-ninja --output-on-failure

cmake --preset package-release-clang-ninja
cmake --build --preset package-release-clang-ninja
ctest --preset package-release-clang-ninja --output-on-failure
ctest --preset package-release-clang-ninja --output-on-failure -R pb_package_config_smoke
cmake --build --preset package-release-clang-ninja --target package
```

The `pb_package_config_smoke` result is the minimum install-time consumer contract for the release candidate.

### 4. Cross-compiler validation gate

For a release candidate, run the GitHub workflow and archive the run URL, exact SHA, compiler versions, and test counts:

```bash
gh workflow run cross-compiler-validation.yml --ref <candidate-ref>
```

Required passing lanes:

```text
GCC C++20 configure/build/CTest
GCC C++23 configure/build/CTest when supported by the runner image
Clang C++20 configure/build/CTest
Clang C++23 configure/build/CTest when supported by the runner image
MSVC C++20 configure/build/CTest
package-release-clang-ninja configure/build/CTest/package in at least one clean environment
```

The latest completed evidence is summarized in [Cross-Compiler Validation Status](cross-compiler-validation.md). Treat that page as historical evidence for its recorded SHA, not as proof for future code changes.

### 5. Benchmark smoke context

Benchmark targets are not release performance gates yet, but the release lane should still prove that the scaffold builds and runs and that any numbers are recorded with context:

```bash
cmake --preset bench-dev-ninja
cmake --build --preset bench-dev-ninja
ctest --preset bench-dev-ninja -R '^pb_bench_' --output-on-failure
```

When a release note includes benchmark numbers, record the commit SHA, preset, compiler, build type, target name, and whether the run was clean or incremental.

## What to record with the release evidence

For each archived command set, capture at least:

- release candidate commit SHA
- preset name and generator
- compiler ID/version
- build type
- whether the build directory was clean or incremental
- any targeted test names or regex filters used
- package artifact path for the local archive created by `--target package`
- benchmark context if any timings are quoted

This keeps the release note aligned with the current docs and avoids treating local smoke output as anonymous evidence.

## Roadmap-only gaps that must stay explicit

Before tagging, confirm the release notes and docs still describe these as **not current guarantees**:

- dependency backend fan-in / true backend multi-input join execution, preemptive cancellation, and backend branch execution beyond the supported sequential and thread-pool fan-in slices
- stable descriptor/export compatibility beyond the current descriptor-record-backed helper output
- observer behavior beyond the current sequential runtime callback path
- optional backend execution beyond the sequential MVP path
- fully stable observer ABI, event schema, and cross-executor contracts beyond current sequential support
- a stable runtime descriptor/export contract
- fully hardened diagnostic wording or broader machine-readable diagnostic schemas
- compiler/platform claims beyond the toolchains that were actually exercised for the candidate
- benchmark thresholds or CI-enforced regression budgets

If any of those items are mentioned in release notes, they must be labeled as roadmap or future work rather than supported behavior.

## Release decision rule

Do not tag a release candidate until all of the following are true:

1. The supported-today commands above have fresh passing evidence on the candidate commit.
2. The cross-compiler workflow passes on the candidate commit, or the release notes explicitly narrow the compiler claim to the exercised subset.
3. `pb_package_config_smoke` passes in the release package lane.
4. The package archive is produced for inspection.
5. Known roadmap-only gaps are either documented as limitations or intentionally removed from release claims.
6. Example and diagnostic docs still match the tests that currently pass.

If any command fails, either fix the candidate or record the issue as a blocker instead of treating the release checklist as satisfied.
