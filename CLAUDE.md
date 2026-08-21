# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

"Fuel Farm" — a C++20 / Raylib game built on a custom engine with a Rust physics backend (Rapier, via
a `cxx` FFI bridge). The game concept has changed direction repeatedly: originally 2D pixel art with
3D "pop-out" models (see `Agents.md` — now historical, do not treat it as current), then a fully 3D
first-person voxel world with a biofuel-farm-simulation gameplay loop (crops, seasons, harvest/fuel-
processing pipelines, tech tree). **Both the voxel world and the farm simulation were removed
entirely on 2026-08-19** — see `Bug/bug.md` for exactly what and why. The direction going forward is
gameplay built around imported hand-authored/AI-generated 3D models (Meshy AI + Blender cleanup), not
yet implemented. Current implementation status lives in `README.md`.

## Build

```bash
cmake -S . -B build
cmake --build build --config Debug
```

An existing Visual-Studio-integrated build directory (e.g. `out/build/x64-Debug`) works the same way
— just point `cmake --build` at it instead of `build`.

**Windows gotcha:** `cmake --build` must run inside a Visual Studio Developer environment
(`INCLUDE`/`LIB`/`WindowsSdkDir` set). A plain shell (Git Bash, a fresh PowerShell) will fail deep
into the build with misleading errors like `cannot open include file: 'concepts'` or cxx's
`algorithm: no include path set` — that's a missing dev environment, not a real compile error. Fix:
run through `vcvarsall.bat x64` first (e.g. from a small `.bat` wrapper — chaining it inline through
Git Bash's `cmd.exe /c '...'` is fragile with quoted paths), or launch from a Developer Command
Prompt / Visual Studio itself.

Dependencies (Raylib, EnTT, nlohmann_json, Taskflow, spdlog, Pipeline-c, Corrosion) are fetched via
CPM.cmake, every one pinned to an immutable tag or commit hash with `URL_HASH` — never relax a pin to
a moving branch reference (Pipeline-c- was bitten by exactly that, twice, historically).

The Rust physics crate (`src/engine/physics/rapier_bridge/`) builds automatically as part of the
CMake build via Corrosion — no separate `cargo build` step. Running `cargo build` directly there
pollutes `rapier_bridge/target/`, which is gitignored for exactly this reason.

## Test

```bash
ctest --test-dir build -C Debug             # all tests
ctest --test-dir build -C Debug -R Physics  # a single test by (partial) name match, e.g. BiofuelPhysicsSmoke
```

**Done-state rule:** a change to engine or game code is not done until `ctest --test-dir build
-C Debug` passes — run it (or the relevant `-R <name>` subset) before declaring success. A change
that "looks right" but is unverified is not done; report the actual test result, not an assumption.

Tests fall into two kinds, both run through the same `ctest` suite:
- **Smoke tests** (`tests/engine/`, `tests/physics/`) — ordinary runtime behavior checks. `tests/
  game/` and `tests/pipeline/` existed for the farm-simulation gameplay and were removed with it
  2026-08-19 — don't recreate that layout without checking it's still the right shape for whatever
  gameplay exists at the time.
- **Architecture guards** (`tests/architecture/*.cmake`) — CMake scripts that regex-scan actual
  source files for structural invariants (e.g. `EngineBoundaryGuard` fails if `game/` code leaks into
  the engine's include path; `RegistryManifestGuard` fails if typed-registry codegen reverts to
  `GLOB_RECURSE`). A guard failure means an architectural rule was violated, not a logic bug — read
  the guard's `.cmake` file to see exactly what it enforces before "fixing" the code that tripped it.

## Architecture

**The engine/game boundary is enforced at compile time, not just by convention.**
`biofuel_engine`'s `target_include_directories` never exposes `src/game/` — only a generated
include-root junction under `src/engine/` is visible to it. Game code may include engine headers;
engine code must never include game headers. When engine code seems to need something from `game/`,
invert the dependency (an interface/callback the game side implements) rather than relaxing the
boundary.

**The typed-registry pattern is the house style for anything registerable** — services, events,
shaders, assets, debug panels. A compile-time `Registry<Types...>` pack plus a `Spec<T>` trait
template per registered type, validated via `static_assert`-based validator structs (see
`DebugPanelRegistryValidator` in `engine/debug/DebugOverlayService.hpp` for the canonical shape).
Registration goes through macros (`BIOFUEL_EVENT_MODULE`, `BIOFUEL_SERVICE_MODULE`,
`BIOFUEL_SHADER_MODULE`, or the lower-level `BIOFUEL_TYPED_MODULE`) inside a header listed in `src/
CMakeLists.txt`'s `ENGINE_TYPED_MODULE_HEADERS` manifest (hand-maintained, never `GLOB_RECURSE`).
`cmake/GenerateTypedRegistries.cmake` regex-scans those headers at build time and writes the
concatenated `Generated*Registry.hpp` files. New registerable code needs both the macro invocation
*and* a manifest entry, or it silently won't register.

**All runtime service access goes through `biofuel::engine::runtime::Runtime`** — the single
service-locator facade (`Runtime::events()`, `Runtime::audio()`, `Runtime::model()`, etc.). Don't add
a second global facade.

**Events** are typed structs organized by domain (`engine/events/{animation,input,mouse,physics,
screen,window}/`), published/consumed via `Events::publish<T>()`/`Events::sink<T>()` (a thin typed
wrapper over an EnTT-dispatcher-backed `EventManager`). A type only needs `EventSpec<T>` to be
publishable — registry *membership* is a separate uniqueness check, not a requirement for dispatch to
work, so a mis-registered event can still silently "work" via the raw dispatcher. Grep for actual
callers before trusting a claim that some event type is unused.

**Screens** are managed by `ScreenManager` (a stack with crossfade transitions), pushed via typed
`ScreenSlot`s. `Screen::isTransitioning()` and similar-looking "internal" methods are read directly
by several concrete screens — don't assume something is dead just because its name suggests
framework-only use.

**Physics** is 2D+3D via an embedded Rust Rapier bridge (`src/engine/physics/rapier_bridge/`, a
`cxx`-bridge crate built into the C++ target by Corrosion). The bridge is handle-based: bodies/
colliders cross the FFI boundary as packed `u64` handles (index + generation, `0` reserved as an
always-invalid sentinel), and every lookup goes through `Option`-returning Rapier APIs, so a stale or
malformed handle from C++ can't panic the Rust side — the crate has zero `unsafe`/`unwrap`/`expect`/
`panic!` by design (a Rust panic can't unwind across the FFI boundary; `panic = "abort"` is set, so a
panic anywhere here kills the whole process). `CollisionGroup`/`SolverGroup` are bitmasks, not
integer IDs — two values only "collide" if `(mask & other.group) != 0 && (other.mask & group) != 0`;
test with genuinely disjoint bits, not just numerically-different values.

**The vendored Pipeline-c- library** (`pb::core::from<Input>::then<Stage>::...::to<Output>`) is still
a real dependency, but its only remaining consumer is the engine's own startup-task system
(`engine/tasks/TaskModule.hpp`'s `TaskModule` concept requires `pb::core::ValidPipeline`) — the
farming-gameplay pipelines that used to be its main consumer (`TurnPipeline`, `HarvestPipeline`,
`FuelProcessPipeline` in `game/gameplay/`) were removed 2026-08-19. Don't assume Pipeline-c- usage
elsewhere in the codebase; grep before reusing the pattern.

**There is currently no gameplay screen.** `GamePlayScreen` (a walkable first-person voxel world —
`VoxelWorld` chunked block storage plus `VoxelVolume` SDF raymarcher) was removed 2026-08-19 along
with the entire `engine/world/` folder it depended on. `engine/models/ModelSystem` (a typed asset
registry for imported `.glb` models) still has an empty built-in model list — this is now the actual
starting point for whatever gameplay gets built around imported models next (see
`assets/models/README.md` and `src/game/models/README.md`), not a mid-transition artifact.

**Shaders** follow the same typed-registry pattern (`ShaderAsset<Tag>` specializations via
`BIOFUEL_EMBEDDED_SHADER_ASSET`/`BIOFUEL_SHADER_MODULE`), with GLSL source embedded into a generated
`ShaderSources.hpp` at build time (`cmake/EmbedShaders.cmake`). The main menu's animated background
(`procedural_backdrop.glsl`) is a permanent looping ambient shader, not a timed transition to another
screen.

## Coding standards

From `src/README.md`, condensed:
- Root-relative includes from `src` (`engine/runtime/Runtime.hpp`, not a relative path).
- Fixed-width type aliases from `engine/core/Types.hpp` (`f32`, `u32`, `usize`, ...) instead of raw
  `int`/`float`.
- `constexpr` for layout/timing constants and stable labels.
- Raw Raylib resource lifetime (`Load*`/`Unload*`) stays inside managers/caches/RAII helpers —
  enforced by `RuntimeSafetyGuard.cmake`, which fails the build if a raw Raylib lifetime call shows
  up outside an approved file, and fails harder if it's inside `game/` code at all.

## Project docs

- `README.md` — build instructions, current implementation status, coding direction. Start here.
- `Agents.md` — the *original* project plan (2D pixel art, vcpkg, a `SwapEntity` 2D↔3D system).
  Explicitly marked historical at the top of the file; none of its architecture section describes the
  current codebase. Kept for reference only.
- `Bug/bug.md` — running log of real bugs found and fixed, plus known limitations left as deliberate
  design decisions. Worth checking before assuming an odd-looking piece of code is accidental.
- Per-module `README.md` files throughout `src/` — each folder's local rules and design rationale;
  check for one before assuming a folder's conventions from its siblings.
