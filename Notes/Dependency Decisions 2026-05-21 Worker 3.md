# Dependency Decisions — 2026-05-21 Worker 3

Owner: worker-3  
Scope: OMX team `omx-team-launch-promp-a45b77ab`, integration/docs/verification lane  
Decision date: 2026-05-21

## Decision

No new dependency is adopted in this worker slice.

A dependency may be added only after all adoption-gate answers are **YES**:

1. It replaces/deletes local code or makes a weak contract compile-time checked.
2. The replaced code is identified by exact paths/functions.
3. MSVC and CPM/CMake compatibility are acceptable.
4. The license is acceptable for this project.
5. Compile-time cost is acceptable for a student/game repo.
6. Worker 3 has documented the decision and the leader has approved adoption.
7. Required builds/tests pass after adoption.

Current result: every new candidate is rejected or deferred because at least one gate is **NO** or unproven. Existing dependencies are kept where they already own a clear subsystem.

## Candidate records

| Candidate | Upstream URL | License | MSVC support status | CMake/CPM integration shape | Compile-time cost risk | Local replacement/deletion target | Decision |
|---|---|---|---|---|---|---|---|
| `magic_enum` | https://github.com/Neargye/magic_enum | MIT | Upstream states MSVC++ >= 14.11 / Visual Studio >= 2017. | Header-only/CMake package; CPM could add a pinned tag archive and link/include only targets that need enum reflection. | Low-to-medium: constexpr enum-name extraction can slow builds if broadly included. | Possible targets: manual enum name/count helpers such as `src/engine/debug/MemoryTelemetry.cpp:136-144`, `src/engine/custom/procedural/hand/HandTypes.hpp:87-103`, `src/game/presentation/world/TileRenderer.cpp:26-35`. These are small and domain-specific. | Defer. It does not currently delete enough local code to justify a new dependency. |
| `serge-sans-paille/frozen` | https://github.com/serge-sans-paille/frozen | Apache-2.0 | Upstream documentation says Visual Studio 2017 works. | Header-only with CMake install/config support; CPM could add a pinned tag/archive and expose an interface target. | Medium: constexpr perfect-hash/map generation can add compile-time cost. | No current immutable runtime lookup table is large enough. Typed registries use type membership (`src/engine/core/typed/Meta.hpp`) and screen policy switches are small (`src/game/screens/GameScreenCatalog.hpp`). | Defer. Revisit only if generated constexpr lookup tables replace repeated switch/map code. |
| Boost.MP11 | https://www.boost.org/library/latest/mp11/ | Boost Software License 1.0 | Boost docs list Visual Studio 2013+ support. | Prefer pinned Boost.MP11 archive/subdir or Boost package target; header-only/no Boost deps per docs. | Medium: template metaprogramming can increase diagnostic noise and compile-time. | Would replace `TypeList`, `Contains`, `TypeIndex`, `UniqueTypes`, `RegistryConcat` in `src/engine/core/typed/Meta.hpp:12-94`. | Defer/reject for now. The local implementation is under 100 lines and has clearer project-specific `static_assert` messages. |
| Boost.PFR | https://www.boost.org/library/latest/pfr/ | Boost Software License 1.0 | Boost library; intended portable C++14 header-only use. Native MSVC still must be verified in this repo before adoption. | Header-only Boost component; CPM/Boost headers target only where aggregate reflection is needed. | Medium: reflection templates can produce harder errors if used in public headers. | No repetitive aggregate serialization/comparison code was found. `src/engine/world/WorldManager.cpp` has explicit JSON mapping with validation/defaulting, not blind field reflection. | Reject for current repo state. |
| Boost.SML | https://github.com/boost-ext/sml | Boost Software License 1.0 | Upstream docs/changelog note MSVC support, including MSVC 2019; older MSVC 2015 has limitations. | Header-only/CMake support; CPM could add a pinned tag/archive and link only UI/state-machine targets. | Medium-to-high: state-machine DSL can obscure simple flows and increase template diagnostics. | Possible targets: `ScreenTransitionRuntime` in `src/engine/ui/typed/ScreenSlot.hpp:20-80`, screen policy dispatch in `src/game/screens/GameScreenCatalog.hpp:99-129`, pause/calibration switches. Current code is small and readable. | Reject/defer. It would not remove enough boilerplate and may obscure the simple screen stack. |
| `glaze` | https://github.com/stephenberry/glaze | MIT with embedded-form exception | Docs require C++23, MSVC 2022+, and `/Zc:preprocessor` for MSVC. | FetchContent/CPM style target `glaze::glaze`; would require C++23 policy review because project currently sets C++20. | High for this repo: C++23 reflection/serialization templates plus AVX2 options can complicate student builds. | Could theoretically replace nlohmann/json use in `src/engine/world/WorldManager.cpp`, but that file has custom validation, fallback behavior, and logging that would still remain. | Reject now. Do not replace `nlohmann/json` without measured JSON bottleneck and C++23 approval. |
| `fmt` | https://github.com/fmtlib/fmt | MIT | Cross-platform; fmt docs provide CMake targets `fmt::fmt` and `fmt::fmt-header-only`. | Prefer no direct CPM dependency unless direct formatting survives spdlog/fmt availability check; if needed, use target-scoped link. | Low-to-medium; compiled target is recommended by fmt docs for build times. | Current formatting needs are logging through `spdlog` (`src/**`), not independent format APIs. | Reject as direct dependency. Use existing spdlog/fmt path. |
| `google/highway` | https://github.com/google/highway | Apache-2.0 / BSD-3-Clause dual license | Upstream CI includes MSVC 2019; CMake build is supported. | CMake package/static or shared library; target-scoped link only for a measured hot loop. | Medium-to-high: SIMD dispatch and platform flags add build/test matrix cost. | No measured hot loop exists in this audit. Candidate areas like terrain/render loops need benchmarks before dependency review. | Reject until benchmark evidence identifies a hot loop. |

## Existing dependency decisions

| Existing dependency | Upstream URL | License note | Current local use | Decision |
|---|---|---|---|---|
| Raylib | https://github.com/raysan5/raylib | zlib/libpng-style license | Rendering, audio, window/input, texture/model APIs throughout `src/engine/**` and `src/game/**`. | Keep. It is the selected platform layer and not a candidate for replacement in this slice. |
| EnTT | https://github.com/skypjack/entt | MIT | `src/engine/events/EventManager.*` owns the `entt::dispatcher` bridge. | Keep. It backs the current event bus cleanly. |
| `nlohmann/json` | https://github.com/nlohmann/json | MIT | `src/engine/world/WorldManager.cpp` world load/save and validation. | Keep. Glaze is rejected because current C++20/custom-validation code is clear and no JSON bottleneck is measured. |
| Taskflow | https://github.com/taskflow/taskflow | MIT | Task scheduling behind `src/engine/tasks/**`; architecture tests enforce boundary/scoped linkage. | Keep. Existing dependency has a clear engine subsystem. |
| `spdlog` | https://github.com/gabime/spdlog | MIT | Logging across engine/game; also covers fmt-style formatting needs. | Keep. Do not add standalone fmt unless direct non-logging formatting remains. |
| Pipeline-c++ / `pipeline_c` | https://github.com/tonytranrp/Pipeline-c- | MIT per existing Worker 2 decision note | Gameplay pipeline validation in `src/game/gameplay/**` and `tests/pipeline/**`. | Keep/defer pin repair. Existing CPM archive hash mismatch must be resolved with provenance before verification can pass. |
| Corrosion | https://github.com/corrosion-rs/corrosion | MIT/Apache-2.0 style Rust ecosystem licensing should be rechecked before pin changes | CMake/Rust bridge import for `src/engine/physics/rapier_bridge`. | Keep. It is the current bridge for Rust physics integration. |
| Rapier bridge/Rust crates | https://github.com/dimforge/rapier | Apache-2.0 | Physics bridge crate under `src/engine/physics/rapier_bridge`. | Keep. Rust toolchain verification is required in an environment with Cargo/Rust installed. |

## Gate answers for this slice

| Gate | Answer | Evidence |
|---|---|---|
| Replaces/deletes local code or strengthens compile-time contract? | NO for every new candidate today. | Candidate table identifies only small or unproven replacement targets. |
| Exact replacement paths/functions identified? | YES for possible targets, but insufficient benefit. | See candidate table path references. |
| MSVC and CPM/CMake compatible? | Mixed/unproven. | Most candidates advertise CMake/MSVC support, but this repo has no native MSVC verification in this worker environment. |
| License acceptable? | Likely acceptable for listed permissive licenses, pending project/legal owner review. | Licenses recorded above. |
| Compile-time cost acceptable? | NO/unproven for metaprogramming, state-machine, JSON, and SIMD candidates. | No compile-time benchmark proves benefit. |
| Worker 3 documented and leader approved adoption? | Documented; no leader approval requested because no adoption is proposed. | This note. |
| Required builds/tests pass after adoption? | Not applicable; no adoption. | Verification remains focused on no CMake/dependency changes. |

## Sources checked

- Local: `CMakeLists.txt`, `src/CMakeLists.txt`, `src/engine/core/typed/README.md`, `src/engine/core/typed/Meta.hpp`, `src/engine/runtime/typed/**`, `src/engine/ui/typed/**`, `src/game/screens/GameScreenCatalog.hpp`, `src/engine/world/WorldManager.cpp`, `Notes/Dependency Decisions 2026-05-21 Worker 2.md`.
- Upstream/license/support pages: candidate upstream URLs in the tables above, plus project docs/search snippets for MSVC/CMake/license metadata.

## Follow-up

Before any future dependency adoption, update this note with the exact tag/hash, CMake target shape, replacement diff, build/test evidence, and leader approval message ID.
