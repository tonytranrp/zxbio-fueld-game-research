# Core

The `Core/` folder owns application startup, shutdown, the fixed-timestep loop, project type aliases, and the loading task queue used by the loading screen.

## Files

```text
Core/
|-- App.hpp
|-- App.cpp
|-- Types.hpp
`-- LoadingTask.hpp
```

## Responsibilities

- `Application` is the single owner of the Raylib window and the main loop
- `Types.hpp` defines project aliases such as `i32`, `f32`, and `u8`, plus shared utilities like `TransparentHash` for string-view-compatible container lookups
- `LoadingTaskQueue` provides sequential startup work with weighted progress for `LoadingScreen`

## Rules

- do not call `InitWindow()` or `CloseWindow()` outside `App.cpp`
- keep lifecycle ownership in `Application`
- prefer designated initialization for `Application::Config`
- use the loading screen for startup work that should happen after window creation
- keep this layer concrete and lightweight; templates are not the default here

## Current behavior

`Application::init()` configures the window, pushes `LoadingScreen`, and lets the UI/data/render systems finish their heavier startup work from there. `Application::run()` uses a fixed timestep for updates and renders every frame.
