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

Fast path (default since 2026-08-25 — Ninja, `BIOFUEL_ENABLE_BEVY_BRIDGE` off, PCH, persistent CPM
cache, raylib vendored as a prebuilt binary on Windows; well under 5 minutes even CPU-only — ~100s
for a from-scratch `build-ninja/` rebuild with warm caches, ~15s incremental, measured on real
hardware). Install [sccache](https://github.com/mozilla/sccache) (`winget install Mozilla.sccache`)
for a further speedup after wiping `build-ninja/` — CMake auto-detects and wraps every compile in
it; purely additive, not required:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Plain/IDE path (any generator, e.g. opening the folder directly in Visual Studio):

```bash
cmake -S . -B build
cmake --build build --config Debug
```

An existing Visual-Studio-integrated build directory (e.g. `out/build/x64-Debug`) works the same way
— just point `cmake --build` at it instead of `build`. See `README.md`'s Build section for the other
presets (`dev-full`, `release`, `release-debug` — the last is RelWithDebInfo: full `/O2` with
complete C++ *and* Rust debug symbols, the "fast but still debuggable" config) and what
`BIOFUEL_ENABLE_BEVY_BRIDGE` gates.

**Windows gotcha:** both `cmake --preset dev`/`cmake --build --preset dev` (Ninja) and, for the
plain path, `cmake --build` (Visual Studio generator) must run inside a Visual Studio Developer
environment (`INCLUDE`/`LIB`/`WindowsSdkDir` set) — Ninja doesn't auto-detect it the way the Visual
Studio generator does, so with Ninja even the *configure* step needs it, not just the build step. A
plain shell (Git Bash, a fresh PowerShell) will fail deep into the build with misleading errors like
`cannot open include file: 'concepts'` or cxx's `algorithm: no include path set` — that's a missing
dev environment, not a real compile error. Fix: run through `vcvarsall.bat x64` first, or launch from
a Developer Command Prompt / Visual Studio itself. Chaining it inline through Git Bash's
`cmd.exe /c '...'` is fragile with quoted paths **and can silently no-op** (exits 0 having run
nothing) rather than erroring — don't trust a bare exit-0 from that path. Both a `.bat` wrapper
invoked via Git Bash's `cmd.exe /c "<path>"` and a single-quoted `cmd.exe /c '<cmd> && <cmd>'`
passed through the PowerShell tool have been observed; only the latter has actually worked in
practice here.

Most dependencies (EnTT, nlohmann_json, Taskflow, spdlog, Corrosion) are fetched via CPM.cmake,
every one pinned to an immutable tag or commit hash with `URL_HASH` — never relax a pin to a moving
branch reference (Pipeline-c- was bitten by exactly that, twice, historically, back when it was
CPM-fetched). raylib is vendored as a prebuilt binary on Windows (`third_party/raylib-5.5-win64-
msvc16/`, CPM-built from source on other platforms), and Pipeline-c- is vendored as source under
`third_party/pipeline-c/` — it's this project's own library, meant to be edited in-tree, not an
external dependency to re-fetch. See `THIRD-PARTY-NOTICES.md` and each vendored dir's
`PROVENANCE.md`.

The Rust crates (`rust/rapier_bridge/`, `rust/bevy_bridge/`, one Cargo workspace at `rust/`) build
automatically as part of the CMake build via Corrosion — no separate `cargo build` step. Running
`cargo build` directly pollutes `rust/target/`, which is gitignored for exactly this reason.

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

**There is a first-person exploration gameplay screen.** `ExplorationScreen` (`src/game/screens/
exploration/`, added 2026-08-25, `ScreenId::Slot3`) provides WASD movement, mouse-look, jumping, and
sprinting over a small Rapier-collision test level (`ExplorationLevel` — ground, boundary walls, a
barn shell, scattered prop boxes, one placeholder landmark box; all stand-in geometry, not final art).
`MainMenuScreen`'s New Game/Continue route into it once the post-dismiss dimension-shift shader
completes. Movement is driven by two plain classes in `engine/character/` — `CharacterController3D`
(kinematic body + Rapier's `KinematicCharacterController`, via the new `PhysicsWorld3D::moveCharacter`
FFI call) and `FirstPersonCamera` (yaw/pitch + view bob) — owned by the screen, not typed services,
since their Rapier body handles are screen-lifetime and incompatible with `BIOFUEL_STATIC_SERVICE`'s
process-lifetime singleton semantics. This replaced the previous `GamePlayScreen` (a walkable
first-person voxel world — `VoxelWorld` chunked block storage plus `VoxelVolume` SDF raymarcher),
removed 2026-08-19 along with the entire `engine/world/` folder it depended on — see `Bug/bug.md`'s
2026-08-19 and 2026-08-25 entries. `engine/models/ModelSystem`'s built-in registry now has one entry:
`ModelAssetId::ViewmodelHands` (Meshy-generated, Blender-rigged, 9702 triangles, 34 bones, textured,
`idle`/`walk` clips), rendered every frame by `ExplorationScreen` through `engine::graphics::
ViewmodelPass` (`src/engine/graphics/ViewmodelPass.hpp`) — an offscreen `RenderSurface` with its own
depth buffer, composited over the world+HUD, so the hands can never clip into or be clipped by world
geometry. The rig's rest pose reaches along local +Y; the render applies a fixed +90-degree rotation
about local X (verified both numerically from the file's own node transforms and visually via a
Blender reproduction) to point the reach direction along +Z instead. This first pass deliberately uses
a fixed (non-yaw/pitch-tracking) viewmodel camera — weapon-sway/full look-tracking is a real follow-up,
not yet implemented. See `assets/models/README.md` and `src/game/models/README.md`.

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
