# OMX Team Launch Prompt — BiofuelGame Windows Refactor

Use this prompt in the next session from the repository root:

```bash
omx team 3:executor "$(cat promt/promt.md)"
```

Or paste everything below as the `$team` task body.

---

## Mission

Refactor the BiofuelGame C++20/Raylib repository for **Windows-first production-grade engine architecture**, stronger **compile-time type safety**, and reduced redundant code. The goal is not visual polish. The goal is a modular backend/engine core that is future-proof, plug-and-play, and difficult to misuse: more typed registries, templates, concepts, constexpr tables, static assertions, and clear ownership boundaries where they remove real duplication or lock important invariants.

## Repo and current facts to verify first

- Repo root: `/mnt/c/Users/Tonyt/Documents/GitHub/zxbio-fueld-game-research`
- Main language/build: C++20, CMake, Raylib 5.5 via CPM, Windows/MSVC primary target.
- Existing dependencies already in top-level `CMakeLists.txt`: Raylib, EnTT, nlohmann/json, Taskflow, spdlog, Pipeline-c-, Corrosion/Rust bridge, optional ASIO for hand tracking.
- Engine/game boundary already documented in `src/README.md`, `src/engine/README.md`, and `src/game/README.md`.
- Existing compile-time areas: `src/engine/core/typed`, `src/engine/runtime/typed`, `src/engine/ui/typed`, generated typed registries, `src/engine/physics/PhysicsTypes.hpp`, `src/engine/world/WorldTypes.hpp`, and Pipeline-c gameplay pipelines.
- Known duplication/refactor targets seen in this repo:
  - `TileType` metadata is spread across `FarmState.hpp`, `FuelFarmData.hpp`, `TileRenderer.cpp`, `GamePlayScreen.cpp`, and `WorldPhysicsIntegration.cpp`.
  - Gameplay setup/debug/sample farm logic is still inside `GamePlayScreen.cpp`.
  - Multiple pipeline runners repeat the same observer/engine wrapper pattern.
  - `CharacterController.cpp` contains `#if 0` guarded code and should be audited before deletion.
  - `src/CMakeLists.txt` is long and has manual manifests for typed modules/tests; improve only if doing so preserves generated-registry guards.
- Run `git status --short` first. Preserve all pre-existing dirty files. Do not overwrite user work. Record the starting dirty state in the team report.

## Non-negotiable rules

1. Follow `AGENTS.md`, `src/README.md`, `src/engine/README.md`, and `src/game/README.md`.
2. Engine code must not depend on concrete game screens or Fuel Farm rules.
3. Game code owns Fuel Farm domain behavior, balance data, screens, and presentation.
4. Prefer deletion/reuse over new abstractions, but deliberately use templates/concepts/constexpr/static_asserts when they remove duplicated runtime switches or make illegal states unrepresentable.
5. Do **not** add a dependency because it looks modern. Add a dependency only when the integration worker proves a concrete replacement/deletion target and all builds/tests pass.
6. Do not touch generated build outputs (`build/`, `build-clean/`, `out/build/`, `.cargo-target/`, `.vs/`) except as build artifacts.
7. Keep diffs small and reviewable. Split work into logical commits using the Lore commit protocol.
8. Push only after verification passes and the leader/user has authorized an external GitHub push. If auth or permission blocks pushing, leave local commits and report the exact blocker.

## Team structure

### Leader session — coordinator/architect

Own coordination, file ownership, dependency gates, and final merge quality.

Responsibilities:

- Preflight: run `git status --short`, inspect dirty files, and write a short context snapshot under `.omx/context/` before workers make changes.
- Assign file ownership so Worker 1 and Worker 2 never edit the same files at the same time.
- Keep Worker 1 focused on game/domain code and Worker 2 focused on engine/backend code.
- Require Worker 3 approval before any new library is added to CMake.
- Reject dependency additions that do not delete/reduce local code or improve compile-time contracts.
- Require tests before accepting worker changes.
- Review for engine/game boundary violations after each worker submits changes.
- Stop only when builds/tests pass, docs match actual changes, commits exist, and push is either completed or blocked with evidence.

### Worker 1 — GAME CODER

Owns `src/game/**`, `tests/game/**`, and `tests/pipeline/**`. Do not edit `src/engine/**` unless the leader explicitly reassigns a narrow interface change.

Primary objective: make Fuel Farm gameplay data and pipelines more compile-time checked, less duplicated, and easier to extend.

Tasks:

1. Create or improve a single authoritative constexpr typed table for `TileType` metadata.
   - Include label/name, render color, optional `CropId`, crop predicate, walkability/solidness, physics material traits, and any other repeated tile facts.
   - Add `TileType::Count` or an equivalent compile-time count if needed.
   - Add static assertions that table size matches enum count and crop mappings are complete.
2. Replace repeated `switch (TileType)` and parallel color/name tables in game code with the new typed table.
   - Targets include `FarmState.hpp`, `TileRenderer.cpp`, `GamePlayScreen.cpp`, and `WorldPhysicsIntegration.cpp`.
   - Keep hot paths fast: indexed constexpr arrays are preferred over runtime maps.
3. Move sample/debug farm setup out of `GamePlayScreen.cpp` into a reusable game gameplay/presentation helper if it remains needed.
4. Refactor Pipeline-c gameplay runners to reduce repeated observer/engine boilerplate.
   - Preserve existing pipeline type aliases and static assertions.
   - Consider a small templated runner helper only if it reduces duplicate code across `TurnPipeline`, `HarvestPipeline`, `FuelProcessPipeline`, and `TechTreePipeline`.
5. Delete placeholder/pass-through stage files only if behavior is represented by a clearer generic type alias/helper and tests prove equivalence.
6. Add or update smoke tests for every gameplay behavior changed.

Acceptance checks for Worker 1:

- `BiofuelFarmStateSmoke`, `BiofuelPipelineSmoke`, `BiofuelTurnPipelineSmoke`, and `BiofuelHarvestPipelineSmoke` pass.
- No gameplay change relies on stringly typed lookups where an enum/tag/table can be compile-time validated.
- Duplicate tile metadata/switches are reduced.

### Worker 2 — ENGINE CODER

Owns `src/engine/**`, `cmake/**` only when needed for engine module generation, and `tests/engine/**` / `tests/runtime/**` / `tests/architecture/**`. Do not edit `src/game/**` unless the leader explicitly assigns a narrow integration point.

Primary objective: make the heavy backend/engine core more modular, production-grade, and plug-and-play without making the game layer touch backend internals.

Tasks:

1. Audit typed registries and meta utilities:
   - `src/engine/core/typed/Meta.hpp`
   - `src/engine/runtime/typed/**`
   - `src/engine/ui/typed/**`
   - `cmake/GenerateTypedRegistries.cmake`
2. Improve compile-time contracts where they reduce fragile hand-written meta code or produce clearer compiler errors.
   - Concepts, consteval validators, typed tags, `Registry<T...>`, and template adapters are preferred.
   - Keep generated registry manifest tests passing.
3. Evaluate replacing hand-rolled typelist/meta pieces with Boost.MP11 only if it deletes real local code and keeps compile errors readable.
4. Evaluate Boost.PFR only for aggregate reflection/serialization/comparison code if it deletes repetitive field code; do not force it into code that is already clear.
5. Evaluate Boost.SML only for screen/lifecycle state machines if it removes current transition boilerplate and does not obscure the simple screen stack.
6. Keep raw Raylib resource lifetime inside managers/caches/RAII helpers. Do not let game code own raw Raylib lifetimes.
7. Audit `CharacterController.cpp` `#if 0` and obsolete world/physics code. Delete only after proving there are no callers and tests/architecture guards cover the behavior.
8. Keep Windows/MSVC build health first: strict warnings, `_WIN32` paths, CRT compatibility, DLL copy behavior, and Rust bridge linkage must remain clean.

Acceptance checks for Worker 2:

- `BiofuelEngineUsabilitySmoke`, `BiofuelLoadingTaskQueueSmoke`, `BiofuelTaskManagerSmoke`, `BiofuelVideoFailureSmoke`, and `BiofuelArchitecturePlanRuntimeSmoke` pass.
- All architecture guards pass, especially engine boundary, runtime safety, model safety, registry manifest, hand tracking safety, screen flow, pause flow, and task manager boundary guards.
- Engine remains independent of concrete game screens and Fuel Farm rules.

### Worker 3 — INTEGRATION / GITHUB / TEST / DOCS

Owns dependency decisions, CMake integration, docs, full verification, commits, and push gate. Coordinate with the leader before editing CMake or docs.

Primary objective: make dependency and verification decisions safe, evidence-backed, Windows-compatible, and commit-ready.

Tasks:

1. Create a dependency decision note before any dependency is added. Suggested path: `Notes/Architecture Decisions.md` or a new dated note under `.omx/context/` if the leader wants it temporary first.
2. Evaluate candidate high-performance / compile-time libraries against current repo needs:
   - `magic_enum` — enum names/counts/string conversion.
   - `serge-sans-paille/frozen` — compile-time immutable lookup tables.
   - Boost.MP11 — typelist/meta replacement for local registry internals.
   - Boost.PFR — aggregate reflection/comparison/serialization helper.
   - Boost.SML — compile-time state machines for lifecycle/screen transitions if justified.
   - `glaze` — high-performance JSON/config only if it replaces nlohmann/json use with clear benefit.
   - `fmt` — only if direct formatting needs remain after spdlog/fmt availability is checked.
   - `google/highway` or SIMD helpers — only if a measured hot loop exists; do not add without benchmark/evidence.
   - Existing EnTT, Taskflow, Pipeline-c-, Raylib, Corrosion/Rapier bridge — document whether they are kept and why.
3. For every candidate, record:
   - upstream URL,
   - license,
   - MSVC support status,
   - CMake/CPM integration shape,
   - compile-time cost risk,
   - local code it would replace/delete,
   - decision: adopt/reject/defer.
4. Add a dependency to `CMakeLists.txt` only after leader approval and only with a hash/pinned version when practical.
5. Keep CMake clean and Windows-friendly. Avoid global include pollution; link dependency targets to only the targets that need them.
6. Update README/Notes/docs only for actual changes. Do not write speculative docs claiming future work is already done.
7. Run formatting/lint/static checks if the repo has a configured tool; otherwise document that no formatter config was found.
8. Commit in small logical Lore commits after verification.
9. Push only if verification passes and the leader/user authorizes the external push. If push is blocked, report exact auth/remote blocker and leave commits local.

Acceptance checks for Worker 3:

- Dependency decision note exists and matches actual CMake changes.
- No dependency was added without a concrete replacement/deletion target.
- Build and test evidence is saved in the final report.
- Commits follow the Lore protocol.

## Dependency adoption gate

A new library can be adopted only if all answers are YES:

1. Does it replace/delete local code or make a currently weak contract compile-time checked?
2. Is the replaced code identified by exact paths/functions?
3. Is it compatible with MSVC and CPM/CMake?
4. Is the license acceptable for this project?
5. Is compile-time cost acceptable for a student/game repo?
6. Did Worker 3 document the decision and leader approve it?
7. Do all required builds/tests pass after adoption?

If any answer is NO, reject/defer the library and keep the code simple.

## Required verification commands

Run the strongest available subset for the active machine. Prefer Windows/MSVC where available; use WSL/Linux as a secondary compatibility check.

```bash
# Preflight / configure when needed
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBIOFUEL_BUILD_TESTS=ON

# Local build and tests
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure

# Windows Visual Studio tree if present/available
cmake --build out/build/x64-Debug --config Debug --parallel
ctest --test-dir out/build/x64-Debug -C Debug --output-on-failure

# Rust physics bridge
cargo test --manifest-path src/engine/physics/rapier_bridge/Cargo.toml --locked
cargo clippy --manifest-path src/engine/physics/rapier_bridge/Cargo.toml --locked -- -D warnings

# Useful focused CTest filters during iteration
ctest --test-dir build -C Debug -R "Biofuel(FarmState|Pipeline|TurnPipeline|HarvestPipeline)" --output-on-failure
ctest --test-dir build -C Debug -R "Biofuel(Engine|Architecture|Runtime|Registry|Boundary|Safety|Screen|Pause|Task)" --output-on-failure
```

If a command cannot run because a toolchain is missing, do not hide it. Report the exact missing tool and run the next-best validation.

## Review checklist before final report

- [ ] Starting dirty files were recorded and preserved.
- [ ] Worker file ownership conflicts were avoided.
- [ ] Engine/game boundary still holds.
- [ ] Tile metadata duplication is reduced.
- [ ] Runtime/service/event/asset/screen registries still compile and validate.
- [ ] No new dependency was added without a documented replacement target.
- [ ] Dead/redundant files were deleted only after tests proved replacement behavior.
- [ ] Windows/MSVC build path remains documented and verified or blocker documented.
- [ ] CMake target dependencies are scoped, not global.
- [ ] Tests pass or each failure has exact evidence and owner.
- [ ] Commits are small and use Lore trailers.
- [ ] Push completed or blocker documented.

## Final report format

Return this structure:

```markdown
# BiofuelGame Refactor Team Report

## Starting state
- Branch:
- Pre-existing dirty files:
- Toolchains available:

## What changed
- Game coder:
- Engine coder:
- Integration/docs/tests:

## Dependency decisions
| Library | Decision | Reason | Replacement/deletion target | License/MSVC notes |
|---|---|---|---|---|

## Verification evidence
- Build:
- CTest:
- Rust test/clippy:
- Windows/MSVC:

## Commits / push
- Commits:
- Push status:

## Remaining risks
- 

## Next recommended step
- 
```

## Desired end state

The engine backend should feel like a reusable modular framework: typed services/assets/events/screens, clear ownership, compile-time validation, plug-in module declarations, and minimal stringly typed or duplicated backend code. The game layer should consume those typed engine contracts while keeping Fuel Farm-specific rules in `src/game`. The repo should build cleanly on Windows, preserve existing behavior, and have clearer seams for future features without repeatedly touching engine internals.
