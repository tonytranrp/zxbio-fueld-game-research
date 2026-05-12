# engine/custom

Production-facing custom engine modules live here. These systems are engine-owned, typed by default, and data-overridable where runtime tuning is useful.

The first module is the procedural robot-hand engine used by the Debug hand lab. It keeps reusable rig, IK, animation, material, texture, mesh-cache, and render code out of game screens while still allowing game code to compose the tool experience.

Patterns:
- Typed specs provide fast defaults and compile-time extension points.
- JSON presets override those defaults for tuning and export from tools.
- Runtime resources use handles and caches, with Raylib resources owned by engine backends.
- Game screens should call module APIs instead of owning procedural generation internals.

## Adding a custom module

Create a subfolder with its own `README.md`, typed public structs, and an
engine-facing API. Keep assets under `assets/custom/<module>/` when runtime data
is needed.

```text
engine/custom/my_module/
|-- MyModuleTypes.hpp
|-- MyModule.hpp
|-- MyModule.cpp
`-- README.md
```

## Coding standards

- Keep modules reusable and independent from concrete game screens.
- Use typed specs and enums at API boundaries.
- Keep Raylib resources in managers, renderers, or caches.
- Make data overrides optional; C++ defaults must still run.
