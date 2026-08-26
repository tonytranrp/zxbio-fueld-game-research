# Cross-Compiler Validation Status

This page records the latest completed GitHub cross-compiler pass. It is evidence, not a feature claim: roadmap-only items stay roadmap-only even when the existing tests pass across compilers. If public-header, runtime, CMake, or test changes land after the recorded code SHA, rerun the workflow on the final candidate SHA before release tagging.

## Latest completed validation run

```text
validated_code_sha=87299c14c813753d170911239e251064cbbfee6f
date_utc=2026-05-20
workflow=Cross Compiler Validation
run=https://github.com/tonytranrp/Pipeline-c-/actions/runs/26145779030
result=PASS
```

This is the latest completed GitHub matrix for the exact SHA above, after the backend fan-in/export/policy hardening commit that added the standard-library `thread_pool_backend` fan-in slice, fan-in descriptor/export helper topology, and policy metadata for scheduling/cancellation/clone/lifetime boundaries. The current branch has advanced past this run, so treat it as historical exact-SHA evidence until the workflow is rerun on the final candidate SHA. The workflow configures, builds, and runs CTest for Linux GCC/Clang C++20 and C++23, validates MSVC C++20 on Windows, and runs the package-release preset in a clean Ubuntu job.

## Matrix results

| Lane | Environment | Result | Test summary | Compiler/tool evidence |
| --- | --- | --- | --- | --- |
| GCC C++20 | Ubuntu GitHub Actions, Ninja Debug | PASS | `163/163` | `g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`, CMake `3.31.6`, Ninja `1.11.1` |
| GCC C++23 | Ubuntu GitHub Actions, Ninja Debug | PASS | `163/163` | `g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`, CMake `3.31.6`, Ninja `1.11.1` |
| Clang C++20 | Ubuntu GitHub Actions, Ninja Debug | PASS | `163/163` | `Ubuntu clang version 18.1.3 (1ubuntu1)`, CMake `3.31.6`, Ninja `1.11.1` |
| Clang C++23 | Ubuntu GitHub Actions, Ninja Debug | PASS | `163/163` | `Ubuntu clang version 18.1.3 (1ubuntu1)`, CMake `3.31.6`, Ninja `1.11.1` |
| MSVC C++20 | Windows GitHub Actions, Visual Studio 2022 Debug | PASS | `163/163` | Visual Studio 2022 Enterprise, MSVC `19.44.35226`, CMake `3.31.6` |
| Package release clean Ubuntu | `package-release-clang-ninja` preset | PASS | `163/163` + TGZ package generated | `Ubuntu clang version 18.1.3 (1ubuntu1)`, CMake `3.31.6`, Ninja `1.11.1`; generated `build/package-release-clang-ninja/pipebuilder-0.1.0-Linux.tar.gz` (`/home/runner/work/Pipeline-c-/Pipeline-c-/build/package-release-clang-ninja/pipebuilder-0.1.0-Linux.tar.gz` in the runner workspace) |

The normal CI workflow also passed on the same code SHA:

```text
workflow=CI
run=https://github.com/tonytranrp/Pipeline-c-/actions/runs/26145765932
result=PASS
jobs=Clang dev preset, Package release preset, Benchmark smoke preset
```

The cross-compiler and CI logs were checked for compiler-style `warning:` diagnostics for this candidate; none were present. GitHub still emits a hosted-runner annotation about the Node 20 runtime used by `actions/checkout@v4`; that is workflow maintenance noise, not a compiler warning or test failure.

## Local validation paired with this SHA

Before pushing the validated code SHA, the local tree passed:

```text
git diff --check
cmake --build --preset clang-dev-ninja
ctest --preset clang-dev-ninja --output-on-failure                 # 163/163
cmake --build --preset package-release-clang-ninja
ctest --preset package-release-clang-ninja --output-on-failure     # 163/163
cmake --build --preset package-release-clang-ninja --target package
```

Local package artifact:

```text
build/package-release-clang-ninja/pipebuilder-0.1.0-Linux.tar.gz
```

## Validation boundaries

This pass supports claims that the current tested surface configures, builds, packages, and passes its test suite on the matrix above. It covers the current linear pipeline surface, selected-output branch/join surface, explicit sequential fan-in slice, DOT/JSON helper-output tests, compile-time/header benchmark smoke targets, package smoke, and Release/NDEBUG smoke-test checks.

It does **not** mean these roadmap items are implemented:

- stable descriptor/export compatibility beyond descriptor-record-backed helper DOT/JSON output
- CLI/file export of user pipeline definitions
- oneTBB / Taskflow / stdexec pipeline backends
- broader C++20 named-module compatibility/install guarantees; module build evidence was added after this run
- C++26 reflection/contracts integrations
- benchmark thresholds or CI-enforced performance budgets
- stable/frozen diagnostic wording across all future features

## Known workflow limitation

The MSVC validation job disables `PB_ENABLE_PACKAGE_SMOKE` because it is a Debug configure/build/CTest lane. Package installation/export/archive behavior is validated separately by the clean Ubuntu `package-release-clang-ninja` job. Keep this split unless a Windows package-release lane is added.
