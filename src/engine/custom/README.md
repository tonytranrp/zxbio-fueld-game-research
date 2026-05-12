# engine/custom

Production-facing custom engine modules live here. These systems are engine-owned, typed by default, and data-overridable where runtime tuning is useful.

The first module is the procedural robot-hand engine used by the Debug hand lab. It keeps reusable rig, IK, animation, material, texture, mesh-cache, and render code out of game screens while still allowing game code to compose the tool experience.

Patterns:
- Typed specs provide fast defaults and compile-time extension points.
- JSON presets override those defaults for tuning and export from tools.
- Runtime resources use handles and caches, with Raylib resources owned by engine backends.
- Game screens should call module APIs instead of owning procedural generation internals.
