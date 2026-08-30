# Biofuel Game - Fuel Farm

Research and implementation repo for a C++20 Raylib engine paired with an embedded Bevy/Vulkan
gameplay session. The concept has changed direction several times during development — most recently
(2026-08-19), the voxel-world and biofuel-farm-simulation gameplay were removed entirely, replaced by
gameplay built around imported 3D models (Meshy AI generation). Raylib is now menu-only
(`MainMenuScreen`, loading screen, pause popup); actual gameplay runs inside a separate, reentrant
Bevy 0.19 / Vulkan ECS session (`src/World/`), reached from the main menu's New Game/Continue. It is a
real, citation-grounded biofuel-farm simulation — three crop species (Liebig's-law growth), pond-water
chemistry, a hydrogen/solar energy pathway, and a day/night cycle, all sharing one carbon-budget meter
and a live HUD. See CLAUDE.md's "The World session" section for the architecture, and each module
under `src/World/src/` for its own doc comment (every constant is grounded in cited real research).

## What is in this repo

- `Project Hub.md` - Obsidian vault entry point for linked project notes
- `Notes/` - Obsidian project maps, templates, and implementation journal
- `Agents.md` - master project plan and long-form design notes
- `Research/` - subject-matter research used to shape the game
- `assets/` - authored runtime assets: shaders, audio, and models
- `src/` - current game source code
- `tests/` - architecture guards and smoke tests (run via `ctest`, see Build below)
- `cmake/` - authored CMake helper scripts, including shader embedding
- `THIRD-PARTY-NOTICES.md` - dependency attribution and licensing status

Generated build output under `build/`, `Build/`, and `src/build/` is not part of the maintained source of truth.

## Current implementation status

**The engine shell** (raylib-side, menu-only as of the Bevy/Vulkan World pivot):

- application bootstrap and fixed-timestep loop
- loading screen with deferred startup tasks plus async-safe preflight support
- screen stack with crossfade transitions
- animated main menu with an embedded, endlessly-looping ambient background shader
- 2D/3D physics via an embedded Rust Rapier bridge, including a kinematic character controller
  (`PhysicsWorld3D::moveCharacter`, wrapping Rapier's `KinematicCharacterController`) — still used by
  `ExplorationScreen` (see below) and available to `src/World/`'s own Rust code
- global pause routing with a blur-backed pause popup
- event bus, input polling, animation manager, and small render/font/UI utilities
- `ExplorationScreen` (`src/game/screens/exploration/`, added 2026-08-25): still compiled and
  registered, but no longer reached from the main menu — superseded by the World session below. Its
  own viewmodel-hands asset and depth-isolated `ViewmodelPass` compositing approach were carried
  forward into `src/World/`'s own `viewmodel.rs` rather than rebuilt from scratch.

**The World session** (`src/World/`, a Bevy 0.19 / Vulkan ECS crate reached from the main menu's New
Game/Continue) is where actual gameplay lives — a real, citation-grounded biofuel-farm simulation, not
placeholder content:

- three real crop species (corn, switchgrass, miscanthus) with Liebig's-law limiting-factor growth,
  a genuine first-gen-vs-advanced-gen biofuel carbon-lifecycle comparison, wind-sway animation, and a
  click-to-harvest interaction with a VFX flourish
- a farm pond modeling real ocean-acidification pH chemistry, coupled back into irrigation quality
  (and crop growth), with an in-HUD warning once the pond overshoots its optimal pH
- a second energy pathway — a hydrogen electrolyzer and a solar array, both grounded in real DOE/IEA/
  Ember/NREL figures, including the solar array's own real embodied-manufacturing-carbon payback
  period and its capacity factor genuinely tied to the day/night cycle below (not a flat average)
- a real day/night cycle (a sweeping sun, animated sky color) that measurably slows crop growth at
  night, the same way real photosynthesis does
- a shared `CarbonBudget` meter every system above feeds (mirroring Anno 2070's CO2 Reservoir design),
  surfaced through a live HUD: current readings, per-species crop counts, and a scrolling history graph

Real player movement/look (WASD, mouse-look) inside the World session is currently unverified in this
project's own automation environment — Windows-MCP can deliver discrete mouse clicks but not synthetic
keyboard input or raw mouse motion to this crate's own winit window, so every system above was
deliberately designed to be exercisable via clicks alone from the fixed player-spawn point. See
CLAUDE.md's "The World session" section for the architecture, and each module's own doc comment under
`src/World/src/` for citations and design rationale. See `Bug/bug.md` for the 2026-08-19 removal this
all eventually replaced.

## Build

**New machine with nothing installed?** Double-click `scripts\setup.bat` (Windows). It installs
every prerequisite (Visual Studio C++ Build Tools, CMake, Ninja, Rust, Git, sccache) via winget,
then builds the game -- see `scripts\setup.ps1`'s header comment for details. Needs administrator
rights (it asks for them). Once set up, `scripts\build.bat` alone repeats just the build.

Fast path (default, recommended -- Ninja, no Bevy tech-demo bridge, CPU-only-friendly).
Run from a Visual Studio Developer Command Prompt (or after `vcvarsall.bat x64`) so
`cl.exe`/`link.exe` are on `PATH` -- Ninja doesn't auto-detect the MSVC environment the way
the Visual Studio generator does:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

This lands in `Build/dev/` and builds in well under 5 minutes even on a weak CPU-only
laptop -- around 100s for a from-scratch `Build/dev/` rebuild with warm caches, ~15s for an
ordinary incremental rebuild, measured on real hardware. It skips `biofuel_bevy_bridge` (see
below), uses precompiled headers, vendors raylib as a prebuilt binary on Windows instead of
compiling it from source (`third_party/raylib-5.5-win64-msvc16/`), and keeps a persistent CPM
dependency cache (`.cpm-cache/`, safe to delete, survives a `Build/` wipe).

**Optional: install [sccache](https://github.com/mozilla/sccache)** (`winget install
Mozilla.sccache`, or see its releases page on other platforms) for a further speedup, especially
after wiping `Build/dev/`. CMake auto-detects it at configure time and wraps every C/C++ and
Rust compile in it; a cache hit skips the actual compile entirely. Purely additive -- the build
works identically without it, just slower on a from-scratch `Build/dev/` rebuild.

Other presets: `dev-full` (adds the Bevy bridge, lands in `Build/dev-full/`), `release`
(Ninja, Release config, lands in `Build/release/`), and `release-debug` (Ninja,
RelWithDebInfo config, lands in `Build/release-debug/`). List them with `cmake --list-presets`.

**`release-debug` is the "fast but still debuggable" config** — the one to play or profile
at full speed in and then step into when something misbehaves. It is optimized like
`release` (full `/O2`, plus MSVC `/Zo` so locals and line-stepping survive inlining, and
full Rust debug info for the physics bridge) and produces complete `.pdb` symbols, but
skips the Release-only LTCG/ICF link passes — those buy a few more percent of perf and a
smaller exe at the cost of much slower links and mangled stack traces. The debug overlay
starts hidden in optimized builds (`NDEBUG`); it can still be toggled at runtime with its
usual hotkey.

Plain/IDE path (no presets -- e.g. opening the folder directly in Visual Studio, or any
generator you prefer):

```bash
cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Release
ctest --test-dir build -C Debug
```

The root CMake project fetches most dependencies (EnTT, nlohmann_json, Taskflow, spdlog,
Corrosion) through CPM; raylib is vendored as a prebuilt binary on Windows (source-built via CPM
on other platforms) and Pipeline-c- is vendored as source under `third_party/pipeline-c/` (it's
this project's own library, meant to be customized in-tree). See `THIRD-PARTY-NOTICES.md` for
attribution and licensing status of each, and `third_party/*/PROVENANCE.md` for update
instructions on the vendored ones.

**`BIOFUEL_ENABLE_BEVY_BRIDGE`** (default `OFF`): builds `biofuel_bevy_bridge`, the embedded
headless-Bevy renderer behind the F6 debug-overlay tech-demo screen (`BevyDemoScreen`). It's a
~430-crate Rust dependency graph (`bevy_render`, `wgpu`, `naga`, `gltf`...) and by far the
largest build-time cost in this repo for something that isn't on any gameplay path. Pass
`-DBIOFUEL_ENABLE_BEVY_BRIDGE=ON` (or use the `dev-full` preset) when actually working on
`engine/bevy/` or `game/screens/bevy_demo/`; the C++ side of that code is compiled out
entirely (guarded by the `BIOFUEL_WITH_BEVY_BRIDGE` compile definition) when it's off.

MP4 idle-video playback uses a local `ffmpeg.exe` install on Windows. CMake records the executable when it is available, and runtime falls back to searching `PATH`. Local MP4 files under `assets/video/` are ignored by default unless a clip is project-owned and safe to redistribute.

## Coding direction

This repo prefers conservative modern C++:

- project aliases from `src/engine/core/Types.hpp` for numeric types
- `std::string_view`, `std::span`, `constexpr`, `[[nodiscard]]`, and `noexcept` where they clarify intent
- concrete helpers and RAII wrappers before generic/template-heavy abstractions
- existing utility boundaries such as `Renderer`, `ShaderManager`, `ModelSystem`, and `TaskManager` instead of scattered raw Raylib or scheduler calls

Templates are allowed when they remove real shared duplication or belong to an existing generic subsystem such as `Animation<T>`. They are not the default style for screens, systems, or utilities.

## Where to start

- Obsidian vault hub: [Project Hub.md](./Project%20Hub.md)
- high-level project intent: [Agents.md](./Agents.md)
- source layout and local coding rules: [src/README.md](./src/README.md)
- screen system: [src/engine/ui/README.md](./src/engine/ui/README.md)
- game screens: [src/game/screens/README.md](./src/game/screens/README.md)
- render and shader utilities: [src/engine/graphics/README.md](./src/engine/graphics/README.md)
- event system: [src/engine/events/README.md](./src/engine/events/README.md)

## Research reference

The research set under `Research/` backed the balance data for the biofuel farm simulation that was
removed 2026-08-19. Kept for reference in case gameplay returns to a biofuel-economics direction; not
currently wired to any code. In particular:

- `01-biofuel-fundamentals.md` - fuel categories and generations
- `03-feedstock-and-crops.md` - feedstocks, yields, and crop traits
- `04-energy-and-emissions.md` - BTU, GHG, water, and carbon tradeoffs
- `05-economics-and-policy.md` - pricing, policy, and market context

## Model pipeline notes

- `.glb` is the standard runtime model format for this repo
- other Raylib-supported model formats are allowed, but not first-class in the current pipeline
- runtime model loading stays Raylib-first
- if custom glTF import control is needed later, the preferred future parser direction is `fastgltf`
- for offline asset optimization, `meshoptimizer` / `gltfpack` is the preferred path
