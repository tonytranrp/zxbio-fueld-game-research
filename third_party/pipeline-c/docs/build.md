# Build and Verification

The project uses CMake 3.23+ and C++20. Prefer Ninja and Clang for local development. After preset reconciliation, `cmake --list-presets=all` should report these unique configure/build/test presets:

| Preset | Primary use |
| --- | --- |
| `dev-ninja` | Generic debug development loop. |
| `clang-dev-ninja` | Clang debug development loop and default docs examples. |
| `clang-time-trace` | Clang compile-time profiling with `PB_ENABLE_CLANG_TIME_TRACE=ON`. |
| `clang-tidy-ninja` | Clang-Tidy tooling smoke. |
| `pch-ninja` | Precompiled-header smoke build. |
| `release-ninja` | Generic release build. |
| `release-clang-ninja` | Clang release build. |
| `package-dev-ninja` | Debug package/install smoke. |
| `package-release-clang-ninja` | Release Clang package/install smoke. |
| `bench-dev-ninja` | Debug benchmark smoke targets. |
| `benchmark-ninja` | Release benchmark smoke targets. |
| `warnings-as-errors-ninja` | Warning-policy smoke build with project warnings promoted to errors. |

Use the Clang debug preset for the normal local loop:

```bash
cmake --preset clang-dev-ninja
cmake --build --preset clang-dev-ninja
ctest --preset clang-dev-ninja
```

Run specialized presets only when you need their lane-specific evidence, for example:

```bash
cmake --preset clang-tidy-ninja
cmake --build --preset clang-tidy-ninja
ctest --preset clang-tidy-ninja

cmake --preset bench-dev-ninja
cmake --build --preset bench-dev-ninja
ctest --preset bench-dev-ninja -R '^pb_bench_' --output-on-failure

cmake --preset package-dev-ninja
cmake --build --preset package-dev-ninja
ctest --preset package-dev-ninja --output-on-failure

cmake --preset package-release-clang-ninja
cmake --build --preset package-release-clang-ninja
ctest --preset package-release-clang-ninja --output-on-failure
cmake --build --preset package-release-clang-ninja --target package
```

In the package lanes, `pb_package_config_smoke` installs the build into a temporary prefix, checks that `find_package(pipebuilder CONFIG REQUIRED)` defines `pb::core`, `pb::runtime`, and `pb::pipeline`, builds and runs separate consumers against each imported target, invokes CPack with TGZ output, and verifies that the archive contains the key public header, package config, targets file, and runtime library entries. Use `package-dev-ninja` for a fast local install/export smoke and `package-release-clang-ninja` for the release-readiness gate. Treat a failing `pb_package_config_smoke` result as a packaging verification blocker, not as passing package evidence.

`PB_ENABLE_CLANG_TIME_TRACE=ON` is available through the `clang-time-trace` preset for compile-time profiling. Optional backend flags are present but intentionally off in the base scaffold.


## Cross-compiler validation workflow

The repository includes `.github/workflows/cross-compiler-validation.yml` for release-hardening validation beyond the local Clang loop. Run it from GitHub Actions when compiler-matrix evidence is needed:

```bash
gh workflow run cross-compiler-validation.yml --ref main
```

The workflow records the exact commit SHA and tool versions, then runs configure/build/CTest for GCC C++20, GCC C++23, Clang C++20, Clang C++23, and MSVC C++20. It also runs `package-release-clang-ninja` in a clean Ubuntu job and builds the TGZ package. The latest completed matrix evidence is summarized in [Cross-Compiler Validation Status](cross-compiler-validation.md).

## Benchmark smoke targets

The lightweight compile-time/header smoke targets are available whenever tests are enabled, including the normal `clang-dev-ninja` developer preset. The runtime benchmark target remains opt-in through `PB_BUILD_BENCHMARKS=ON` or the dedicated benchmark presets.

```bash
cmake --preset clang-dev-ninja
cmake --build --preset clang-dev-ninja --target pb_compile_time_benchmarks
ctest --preset clang-dev-ninja --output-on-failure -R 'benchmark|compile_time|header|bench'

cmake --preset clang-dev-ninja -DPB_BUILD_BENCHMARKS=ON
cmake --build --preset clang-dev-ninja --target pb_bench_sequential_5_stage
./build/clang-dev-ninja/bench/pb_bench_sequential_5_stage
```

Use `pb_bench_include_pipeline` as a guard for the public-header include surface. Use `pb_bench_compile_chain_5` and `pb_bench_compile_chain_50` as the small and larger compile-time chain baselines; both instantiate the linear chain, public metadata aliases, and descriptor view for the requested stage count. Use `pb_bench_sequential_5_stage` as a smoke check that the runtime benchmark scaffold still exercises the sequential executor. These targets are not pass/fail performance gates yet; record timings alongside compiler, build type, and preset before comparing results across machines.

### Recording benchmark results

Benchmark smoke results are only comparable when the environment is recorded with the result. For a release-readiness note or regression investigation, capture at least:

- Git commit SHA and whether the tree had local changes.
- Preset name, build type, compiler ID/version, generator, and target name.
- Machine/OS summary and whether the build directory was clean or incremental.
- Command used to build or run the benchmark.
- Observed wall time or trace summary, plus whether the number came from CTest, shell `time`, or a Clang time-trace artifact.

Use a short text record such as:

```text
commit=<sha> tree=<clean|dirty>
preset=bench-dev-ninja target=pb_bench_include_pipeline compiler=<id-version>
mode=<clean|incremental> metric=<wall-time|trace-summary> value=<value>
notes=<local context, suspected outlier, or follow-up>
```

Do not compare Debug and Release presets as regressions against each other. Use `bench-dev-ninja` for fast local smoke checks and `benchmark-ninja` when a release-like benchmark signal is needed.

For compile-time profiling, build the same translation units with the Clang time-trace preset:

```bash
cmake --preset clang-time-trace -DPB_BUILD_BENCHMARKS=ON
cmake --build --preset clang-time-trace --target pb_bench_include_pipeline
cmake --build --preset clang-time-trace --target pb_bench_compile_chain_5
cmake --build --preset clang-time-trace --target pb_bench_compile_chain_50
find build/clang-time-trace -name '*.json' -path '*CMakeFiles*' -print
```

Treat the generated trace files as local diagnostics. They are useful for spotting expensive headers and template instantiations, but they should not be committed unless a future release process explicitly asks for captured benchmark artifacts.
