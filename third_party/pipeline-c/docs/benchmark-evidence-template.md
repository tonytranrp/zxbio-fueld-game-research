# Benchmark Evidence Template

Use this template when you need to record benchmark smoke or compile-time profiling evidence for `pipebuilder` without overstating what the current benchmark lane proves.

## What this template is for

The current benchmark targets are smoke and profiling scaffolding. They prove that representative benchmark-oriented translation units still build and run, and they provide trace artifacts for investigation, but they do **not** establish release thresholds or performance budgets on their own.

Use this template when you need to capture:

- benchmark smoke evidence from `bench-dev-ninja` or `benchmark-ninja`
- compile-time profiling evidence from `clang-time-trace`
- enough environment context that another maintainer can interpret the result later

## Commands to gather evidence

### Benchmark smoke lanes

Use the existing benchmark presets for smoke-style evidence:

```bash
cmake --preset bench-dev-ninja
cmake --build --preset bench-dev-ninja
ctest --preset bench-dev-ninja --output-on-failure -R '^pb_bench_'

cmake --preset benchmark-ninja
cmake --build --preset benchmark-ninja
ctest --preset benchmark-ninja --output-on-failure -R '^pb_bench_'
```

These lanes cover the current benchmark targets:

- `pb_bench_include_pipeline`
- `pb_bench_compile_chain_5`
- `pb_bench_compile_chain_50`
- `pb_bench_sequential_5_stage`

### Compile-time profiling lane

Use the Clang time-trace lane when you need JSON artifacts instead of only pass/fail evidence:

```bash
cmake --preset clang-time-trace -DPB_BUILD_BENCHMARKS=ON
cmake --build --preset clang-time-trace --target pb_bench_include_pipeline
cmake --build --preset clang-time-trace --target pb_bench_compile_chain_5
cmake --build --preset clang-time-trace --target pb_bench_compile_chain_50
find build/clang-time-trace -name '*.json' -path '*CMakeFiles*' -print
```

For the current header-inclusion, 5-stage, and 50-stage compile-time baseline surface, you can run the local helper instead of typing those build commands by hand:

```bash
bench/compile_time/collect_baseline.sh
```

The helper prints local timing command output and trace artifact paths only; copy the relevant observed values into the template below and keep the raw output machine/context-specific.

## Minimum context to record

Every benchmark note should record at least:

- Git commit SHA
- whether the tree was clean or dirty
- preset name
- build type
- compiler ID and version
- generator
- target name or test name
- whether the build directory was clean or incremental
- exact command used to build or run the benchmark
- the observed measurement or smoke outcome
- brief notes about unusual local factors

## Short evidence record template

Copy this block into notes, tickets, or a release-readiness log:

```text
commit=<sha> tree=<clean|dirty>
preset=<bench-dev-ninja|benchmark-ninja|clang-time-trace>
build_type=<Debug|Release|RelWithDebInfo>
compiler=<id-version> generator=<generator>
target=<pb_bench_include_pipeline|pb_bench_compile_chain_5|pb_bench_compile_chain_50|pb_bench_sequential_5_stage>
mode=<clean|incremental>
command=<exact command>
result=<pass/fail|wall-time|trace-summary>
artifact=<path to executable, test output, or trace json>
notes=<local context, suspected outlier, or follow-up>
```

## Example smoke evidence

```text
commit=abc1234 tree=clean
preset=bench-dev-ninja
build_type=Debug
compiler=GNU-13.3.0 generator=Ninja
target=pb_bench_compile_chain_50
mode=clean
command=ctest --test-dir build/bench-dev-ninja --output-on-failure -R '^pb_bench_compile_chain_50$'
result=PASS
artifact=build/bench-dev-ninja/bench/pb_bench_compile_chain_50
notes=Smoke-only result; no release threshold implied.
```

## Example compile-time profiling evidence

```text
commit=abc1234 tree=clean
preset=clang-time-trace
build_type=RelWithDebInfo
compiler=Clang-18.1.3 generator=Ninja
target=pb_bench_include_pipeline
mode=clean
command=cmake --build build/clang-time-trace --target pb_bench_include_pipeline
result=trace-summary
artifact=build/clang-time-trace/bench/CMakeFiles/pb_bench_include_pipeline.dir/compile_time/include_pipeline.cpp.json
notes=Use the JSON trace for investigation; this is not a benchmark pass/fail threshold.
```

## Smoke evidence vs. release thresholds

Keep these distinctions explicit:

- A benchmark smoke pass means the target still builds and runs.
- A time-trace artifact means the compiler emitted diagnostic profiling data.
- Neither result alone establishes an approved performance budget.
- Debug and Release numbers are not regression-equivalent by default.
- Release claims need an explicit threshold policy; the current repo does not define one.

## Current limitations

The benchmark evidence process is intentionally lightweight today:

- No CI-enforced runtime or compile-time thresholds are defined.
- The targets are representative scaffolding, not a full performance matrix.
- The time-trace lane is Clang-specific.
- Local machine differences can dominate wall-time numbers.
- Benchmark evidence should be interpreted together with `docs/benchmark-workflow.md`, not as a replacement for it.
