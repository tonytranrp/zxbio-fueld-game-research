#!/usr/bin/env bash
set -euo pipefail

preset="${1:-clang-time-trace}"
shift || true

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
commit_sha="$(git -C "${repo_root}" rev-parse --short HEAD 2>/dev/null || echo unknown)"
build_dir="${repo_root}/build/${preset}"

if command -v rtk >/dev/null 2>&1; then
  runner=(rtk tokrun --)
  runner_name="rtk-tokrun"
else
  runner=()
  runner_name="direct"
fi

run_command() {
  if [ "${#runner[@]}" -gt 0 ]; then
    "${runner[@]}" "$@"
  else
    "$@"
  fi
}

targets=("${@:-}")
if [ "${#targets[@]}" -eq 0 ] || [ -z "${targets[0]}" ]; then
  targets=(
    pb_bench_include_pipeline
    pb_bench_compile_chain_5
    pb_bench_compile_chain_50
  )
fi

echo "# pipebuilder compile-time baseline collection"
echo "repo_root=${repo_root}"
echo "commit=${commit_sha}"
echo "preset=${preset}"
echo "build_dir=${build_dir}"
echo "runner=${runner_name}"
echo "targets=${targets[*]}"
echo "note=records local timing commands only; no threshold or budget is implied"

run_command cmake --preset "${preset}" -DPB_BUILD_BENCHMARKS=ON

for target in "${targets[@]}"; do
  echo "## build target=${target}"
  run_command cmake -E time cmake --build --preset "${preset}" --target "${target}"
done

if [ "${preset}" = "clang-time-trace" ]; then
  echo "## clang time-trace artifacts"
  find "${build_dir}" -name '*.json' -path '*CMakeFiles*' -print | sort
fi
