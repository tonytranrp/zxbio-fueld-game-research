# Release Evidence Template

Use this template when collecting release-candidate evidence for `pipebuilder`. Fill it from fresh command output on the candidate SHA; do not copy older checkpoint results into a release record.

## Candidate summary

```text
candidate_sha=<git-sha>
tree_state=<clean|dirty-with-described-local-changes>
date_utc=<yyyy-mm-ddThh:mm:ssZ>
release_owner=<name-or-handle>
```

## Environment

```text
os=<name-version>
generator=<Ninja|other>
cmake=<cmake --version first line>
compiler=<compiler id and version>
build_directory_state=<clean|incremental>
```

## Preset inventory

```text
command=cmake --list-presets=all
result=<PASS|FAIL>
notes=<document any missing or unexpected preset names>
```

## Developer confidence gate

```text
preset=clang-dev-ninja
commands=
  cmake --preset clang-dev-ninja
  cmake --build --preset clang-dev-ninja
  ctest --preset clang-dev-ninja --output-on-failure
result=<PASS|FAIL>
test_summary=<for example: 30/30 passed>
notes=<blockers or warnings that matter to the release decision>
```

## Package release gate

```text
preset=package-release-clang-ninja
commands=
  cmake --preset package-release-clang-ninja
  cmake --build --preset package-release-clang-ninja
  ctest --preset package-release-clang-ninja --output-on-failure
  ctest --preset package-release-clang-ninja --output-on-failure -R pb_package_config_smoke
  cmake --build --preset package-release-clang-ninja --target package
result=<PASS|FAIL>
package_smoke=<PASS|FAIL and test count>
artifact=<path-to-generated-.tar.gz>
notes=<install/export/archive blockers or limitations>
```


## Cross-compiler validation gate

```text
workflow=cross-compiler-validation.yml
run_url=<github-actions-run-url>
head_sha=<exact-sha-reported-by-run>
result=<PASS|FAIL>
gcc_cxx20=<PASS|FAIL|not-run> compiler=<id-version> tests=<passed/total>
gcc_cxx23=<PASS|FAIL|not-run> compiler=<id-version> tests=<passed/total>
clang_cxx20=<PASS|FAIL|not-run> compiler=<id-version> tests=<passed/total>
clang_cxx23=<PASS|FAIL|not-run> compiler=<id-version> tests=<passed/total>
msvc_cxx20=<PASS|FAIL|not-run> compiler=<id-version> tests=<passed/total>
package_release_clean=<PASS|FAIL|not-run> compiler=<id-version> tests=<passed/total> artifact=<path-or-url>
notes=<runner warnings, explicitly unclaimed lanes, or rerun requirements>
```

## Benchmark smoke gate

Benchmark results are smoke/profiling evidence, not release performance thresholds.

```text
preset=bench-dev-ninja
commands=
  cmake --preset bench-dev-ninja
  cmake --build --preset bench-dev-ninja
  ctest --preset bench-dev-ninja -R '^pb_bench_' --output-on-failure
result=<PASS|FAIL>
test_summary=<benchmark smoke test count>
metric_context=<clean|incremental, Debug|Release, local hardware notes>
notes=<do not compare against another machine without context>
```

## Optional tooling gates

Record these only when the release candidate intentionally includes the optional tooling evidence.

```text
warnings_as_errors=<PASS|FAIL|not-run> command_set=<configure/build/ctest warnings-as-errors-ninja>
clang_tidy=<PASS|FAIL|not-run> command_set=<configure/build/ctest clang-tidy-ninja>
clang_time_trace=<PASS|FAIL|not-run> artifact_summary=<trace paths or reason not collected>
```

## Supported claims checked

Mark each item as `supported`, `roadmap-only`, or `blocked` based on the current docs and tests.

```text
package_targets_pb_core_runtime_pipeline=<supported|blocked>
linear_typed_pipeline_validation=<supported|blocked>
sequential_runtime_execution=<supported|blocked>
compile_pass_fail_runtime_example_package_benchmark_smoke=<supported|blocked>
branch_join_topology=<roadmap-only|supported-with-evidence>
graph_export=<roadmap-only|supported-with-evidence>
observer_callbacks=<supported|roadmap-only>
optional_backends=<roadmap-only|experimental|supported-with-evidence>
benchmark_thresholds=<roadmap-only|supported-with-policy>
cross_compiler_validation=<not-run|blocked|supported-with-evidence>
package_manager_validation=<not-run|blocked|supported-with-evidence>
```

## Blockers and release decision

```text
release_decision=<go|no-go>
blockers=
  - <none or concrete failing command/test/decision>
deferred_limitations=
  - <documented limitation that is not a release blocker>
follow_up_tasks=
  - <post-release or pre-release task id/link>
```

A release candidate is ready only when required gates pass on the candidate SHA, package smoke passes, the local package archive exists, and roadmap-only gaps remain clearly labeled as limitations rather than supported behavior.
