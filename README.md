# Biofuel Game - Fuel Farm

Research and implementation repo for a C++20 Raylib game about biofuel production, land use, and menu-driven simulation systems.

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
- a walkable first-person voxel world (`GamePlay`): streamed blocky terrain with
  a raymarched-voxel renderer plus a rasterized fallback, driven by a kinematic
  first-person controller
- typed model system with startup-preloaded model assets
- 2D/3D physics via an embedded Rust Rapier bridge
- global pause routing with a blur-backed pause popup
- event bus, input polling, animation manager, and small render/font/UI utilities

## Build

```bash
cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Release
ctest --test-dir build -C Debug
```

The root CMake project fetches its dependencies (Raylib, EnTT, nlohmann_json, Taskflow, spdlog, Pipeline-c, Corrosion) through CPM; see `THIRD-PARTY-NOTICES.md` for attribution and licensing status of each.

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

The research set under `Research/` is still the main source for balance inputs and domain language. In particular:

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
