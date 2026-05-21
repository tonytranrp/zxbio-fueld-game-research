# Dependency Decisions — 2026-05-21 Worker 2

Owner: worker-2  
Scope: OMX team `omx-team-launch-promp-d6881907`, engine/build verification lane

## Decision

No new dependency is adopted by Worker 2 in this slice.

## Rationale

Worker 2 changes used existing repository facilities:

- C++20 concepts/`consteval`/`static_assert` checks in `src/engine/tasks/TaskModule.hpp`.
- Deletion of unreferenced `src/engine/physics/CharacterController.cpp` and `.hpp`.

No CMake, CPM, Cargo, or Rust bridge dependency manifest was changed.

## Candidate records

| Candidate | Upstream URL | License | MSVC support status | CMake/CPM integration shape | Compile-time cost risk | Local replacement/deletion target | Decision |
|---|---|---|---|---|---|---|---|
| Pipeline-c++ / `pipeline_c` | https://github.com/tonytranrp/Pipeline-c- | MIT, per upstream `LICENSE`; repository page also identifies an MIT license. | Not proven in this worker. Upstream presets are Ninja/Clang-oriented; no Visual Studio/MSVC preset was observed. This project still needs native MSVC build evidence. | Existing `CPMAddPackage(NAME pipeline_c URL https://github.com/tonytranrp/Pipeline-c-/archive/refs/heads/main.tar.gz URL_HASH ... OPTIONS PB_BUILD_TESTS OFF, PB_BUILD_EXAMPLES OFF, PB_BUILD_BENCHMARKS OFF, PB_ENABLE_WARNINGS OFF, PB_WARNINGS_AS_ERRORS OFF, PB_ENABLE_UNITY OFF)`. | Medium: C++20 template pipeline builder used in gameplay and task metadata; compile-time can grow with pipeline/stage count. Upstream has a clang time-trace preset, suggesting compile-time measurement is expected for changes. | Existing local use replaces manual gameplay pipeline dispatch/observer boilerplate in `src/game/gameplay/**` and `tests/pipeline/**`; no Worker 2 engine code was replaced by adopting or changing it. | Defer any hash/pin update. Current fetch hash mismatch blocks configure; integration owner must pin a stable archive/ref or update the hash with provenance, then run builds/tests. |

## Current dependency blockers observed

| Dependency | Status | Evidence | Required owner/action |
|---|---|---|---|
| `pipeline_c` CPM archive | Existing dependency fetch is blocked | `cmake -S . -B build-drm -DCMAKE_BUILD_TYPE=Debug -DBIOFUEL_BUILD_TESTS=ON -DPLATFORM=DRM` reports expected SHA256 `a4312156951c3b0fe52c15ab1c4bccb1e9298399d5940b4b701c93dd4c718038`, actual `3a2ee9c5283c3b31a7ab5feb892bb80c8f8d2c3531b8dbe711d8416183a4370d`. | Integration/dependency owner must decide whether to pin a stable archive/ref, update the hash with provenance, vendor a known-good source, or reject/defer. |
| Raylib GLFW/X11 path | Local Linux configure is blocked | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBIOFUEL_BUILD_TESTS=ON` reports missing `X11_X11_INCLUDE_PATH` and `X11_X11_LIB`. | Build environment needs X11 development packages or a non-X11 platform configure path. |
| Rust bridge tooling | Not verified in this worker | `cargo`/`rustc` are absent in this worker environment. | Run Rust bridge test/clippy in an environment with Rust installed. |
| Windows/MSVC lane | Not verified in this worker | `out/build/x64-Debug` is absent and `cl`, `cl.exe`, and `vswhere` are not available. | Run native Windows/MSVC build and CTest before push/merge. |

## Gate

Do not add or update dependencies until a concrete replacement/deletion target is recorded and the relevant build/test lane passes.
