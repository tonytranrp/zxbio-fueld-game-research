# Compile-Time Benchmark Baselines

Reproducible compile-time baseline measurements for the smoke
translation units shipped under `bench/compile_time/`. These are
**reference numbers**, not release thresholds — they exist so future
contributors can detect compile-time regressions early, and so release
notes can attach concrete evidence to performance claims.

The baseline-collection workflow lives in
[bench/compile_time/collect_baseline.sh](../bench/compile_time/collect_baseline.sh).
This page records the latest collected numbers and the exact context
they were collected under.

> **Important:** these numbers depend on machine, OS, compiler version,
> background load, and disk caches. The goal is qualitative — to see
> whether a refactor moves the include-pipeline time from 1.3s to
> 2.5s, not to enforce a ±5% threshold across machines.
>
> Re-collect locally with the script above before relying on the
> numbers for performance work.

## Latest baseline (2026-05-25)

| Translation unit | Median wall time | Min wall time | Max wall time | Method |
| --- | --- | --- | --- | --- |
| `pb_bench_include_pipeline` | **1.31 s** | 1.31 s | 1.38 s | 3 clean rebuilds, single-target ninja |
| `pb_bench_compile_chain_5`  | **1.33 s** | 1.32 s | 1.35 s | 3 clean rebuilds, single-target ninja |
| `pb_bench_compile_chain_50` | **1.41 s** | 1.37 s | 1.47 s | 3 clean rebuilds, single-target ninja |

### Branch / fan-in compile-time coverage (added 2026-06-02)

The linear `chain_N` benchmarks only exercise straight-line pipelines. A
dedicated **branch + fan-in** translation unit now covers the heavier
topology-instantiation cost (multi-case `::branch<...>` plus
`::fan_in<...>` plus descriptor/export helper instantiation):

| Translation unit | Median wall time | Method |
| --- | --- | --- |
| `pb_bench_compile_branch_fan_in_10` | _to be recorded on candidate SHA_ | 3 clean rebuilds, single-target ninja |

The same target is also exercised inside the default test suite as the
compile-pass smoke `pb_branch_compile_time_smoke` (label
`compile-pass;bench;branch`), so branch/fan-in compile health is checked
on every `ctest` run even when the benchmark numbers are not collected.
Measure it the same way as the chain targets — add it to the
`$benches` array below with object path
`build/clang-dev-ninja/bench/CMakeFiles/pb_bench_compile_branch_fan_in_10.dir/compile_time/branch_fan_in_10.cpp.obj`.

**Reproducer:**

```powershell
$benches = @(
  @{ Target = "pb_bench_include_pipeline";   Obj = "build/clang-dev-ninja/bench/CMakeFiles/pb_bench_include_pipeline.dir/compile_time/include_pipeline.cpp.obj" },
  @{ Target = "pb_bench_compile_chain_5";    Obj = "build/clang-dev-ninja/bench/CMakeFiles/pb_bench_compile_chain_5.dir/compile_time/chain_5.cpp.obj" },
  @{ Target = "pb_bench_compile_chain_50";   Obj = "build/clang-dev-ninja/bench/CMakeFiles/pb_bench_compile_chain_50.dir/compile_time/chain_50.cpp.obj" }
)
foreach ($b in $benches) {
  for ($i = 0; $i -lt 3; $i++) {
    if (Test-Path $b.Obj) { Remove-Item $b.Obj }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    cmake --build --preset clang-dev-ninja --target $b.Target | Out-Null
    $sw.Stop()
    Write-Output "$($b.Target) run $i = $($sw.Elapsed.TotalSeconds) s"
  }
}
```

## Collection context

| Field | Value |
| --- | --- |
| Date | 2026-05-25 |
| Pipeline-c++ SHA | `9299bac` (post error-policy parity hardening batch) |
| Preset | `clang-dev-ninja` |
| Compiler | clang version 22.1.4 (llvm-project 35990504507d79e0b9deb809c8ee5e1b34ceef20) |
| Toolchain | llvm-mingw-20260421-ucrt-x86_64 |
| Build type | Debug (`-g -std=c++20 -Wall -Wextra -Wpedantic`) |
| OS | Microsoft Windows 11 Home 10.0.26200 |
| CPU | 13th Gen Intel Core i7-13620H |
| Generator | Ninja |
| Measurement method | PowerShell `Diagnostics.Stopwatch` around single-target `cmake --build` after `.obj` removal |
| Runs per target | 3 |
| Aggregation | Median |

## Observations

- **The whole header costs ~1.3 s on its own.** `pb_bench_include_pipeline`
  is a `#include <pb/pipeline.hpp>` + `int main(){}` translation unit;
  it sets the floor for any TU that uses the library.
- **5-stage chain adds only ~20 ms over the include floor.** The
  template machinery for typical pipelines is essentially free relative
  to header parse cost.
- **50-stage chain adds ~100 ms over the include floor.** Heavy
  pipelines stay under +10% of header cost. This matches the
  `chain_depth<N>::is_heavy` qualitative classification documented in
  `docs/benchmark-workflow.md`.

## Why no CI budget yet

Per [docs/research-verification-matrix.md](research-verification-matrix.md):
release-grade compile-time thresholds and CI regression budgets remain
roadmap-only. Adding them requires:

1. A stable reference machine in CI (current GitHub runners vary
   significantly in compile-time performance).
2. A statistically meaningful sample size per run (3 is plenty for
   spot-checks; CI would want 5+ with outlier rejection).
3. A documented tolerance band per target (e.g. `chain_50 < 2.0s` for
   the reference runner).
4. Hook into the cross-compiler validation workflow so the budget
   fails the PR check, not just the local build.

This page is the seed for that future work: until those four items
exist, the table above is reference data, not a budget.

## See also

- [bench/compile_time/collect_baseline.sh](../bench/compile_time/collect_baseline.sh) — repeatable collection script
- [docs/benchmark-workflow.md](benchmark-workflow.md) — overall benchmark surface
- [docs/benchmark-evidence-template.md](benchmark-evidence-template.md) — template for release-note evidence blocks
