# Biofuel Game - Fuel Farm

Research and implementation repo for a C++20 Raylib game. The concept has changed direction several
times during development — most recently (2026-08-19), the voxel-world and biofuel-farm-simulation
gameplay were removed entirely in favor of building gameplay around imported 3D models (Meshy AI
generation + Blender cleanup). What replaces them is not yet built; the engine (screens, events,
services, physics, shaders, models) is otherwise intact and reusable.

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

Generated build output under `build/`, `out/`, and `src/build/` is not part of the maintained source of truth.

## Current implementation status

The playable codebase is still early-stage, but it already contains:

- application bootstrap and fixed-timestep loop
- loading screen with deferred startup tasks plus async-safe preflight support
- screen stack with crossfade transitions
- animated main menu with an embedded, endlessly-looping ambient background shader
- typed model system with startup-preloaded model assets (currently empty — no models imported yet)
- 2D/3D physics via an embedded Rust Rapier bridge (currently unused by any game-side system)
- global pause routing with a blur-backed pause popup
- event bus, input polling, animation manager, and small render/font/UI utilities

There is currently no gameplay screen — the previous voxel-world (`GamePlay`) and biofuel-farm
simulation (crops, seasons, harvest/fuel-processing pipelines, tech tree) were removed 2026-08-19.
The main menu's "New Game" button plays its shader transition and then holds indefinitely; nothing
currently follows it. See `Bug/bug.md` for what was removed and why.

## Build

Fast path (default, recommended -- Ninja, no Bevy tech-demo bridge, CPU-only-friendly).
Run from a Visual Studio Developer Command Prompt (or after `vcvarsall.bat x64`) so
`cl.exe`/`link.exe` are on `PATH` -- Ninja doesn't auto-detect the MSVC environment the way
the Visual Studio generator does:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

This lands in `build-ninja/` and builds in well under 5 minutes even on a weak CPU-only
laptop -- around 100s for a from-scratch `build-ninja/` rebuild with warm caches, ~15s for an
ordinary incremental rebuild, measured on real hardware. It skips `biofuel_bevy_bridge` (see
below), uses precompiled headers, vendors raylib as a prebuilt binary on Windows instead of
compiling it from source (`third_party/raylib-5.5-win64-msvc16/`), and keeps a persistent CPM
dependency cache (`.cpm-cache/`, safe to delete, survives a `build-ninja/` wipe).

**Optional: install [sccache](https://github.com/mozilla/sccache)** (`winget install
Mozilla.sccache`, or see its releases page on other platforms) for a further speedup, especially
after wiping `build-ninja/`. CMake auto-detects it at configure time and wraps every C/C++ and
Rust compile in it; a cache hit skips the actual compile entirely. Purely additive -- the build
works identically without it, just slower on a from-scratch `build-ninja/` rebuild.

Other presets: `dev-full` (adds the Bevy bridge, lands in `build-ninja-full/`), `release`
(Ninja, Release config, lands in `build-ninja-release/`). List them with `cmake --list-presets`.

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
