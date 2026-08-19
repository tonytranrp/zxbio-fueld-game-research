# engine

Reusable runtime code lives here: app startup, core types, typed registries,
events, physics, graphics, media, animation, input, window helpers, UI stack,
and model loading.

## Folder map

```text
engine/
|-- app/        Raylib application loop and service update order
|-- core/       common aliases, loading tasks, typed registry primitives
|-- runtime/    Runtime facade plus service/event/asset/shader registries
|-- events/     event payloads and dispatcher bridge
|-- physics/    Rapier-backed 2D/3D rigid-body simulation
|-- tasks/      task/job scheduling wrapper and async loading helpers
|-- graphics/   renderer, render surfaces, shaders, shader components
|-- models/     model loading, animation pairing, and typed model registry
|-- animation/  value animation and model keyframe playback
|-- audio/      audio asset and playback service
|-- video/      FFmpeg-backed video playback service
|-- fonts/      font loading helpers
|-- input/      shared input polling service
|-- window/     platform window helpers
|-- ui/         screen stack and typed render pipeline
`-- debug/      telemetry and debug helpers
```

## Engine boundary

Engine code must not depend on concrete game screens or Fuel Farm rules. It may
depend on typed registries, explicit template parameters, and generic data
contracts. If a debug screen proves a reusable idea, move the detector, mapping,
cache, or resource ownership here and leave only the tool UI in `game/`.

## How to use it

Game code normally enters engine systems through the runtime facade:

```cpp
auto& screens = biofuel::engine::runtime::Runtime::screen();
auto& renderer = biofuel::engine::runtime::Runtime::render();
auto& physics = biofuel::engine::runtime::Runtime::physics();
```

Lower-level engine code should include the specific subsystem it needs rather
than reaching through unrelated managers.

## Coding standards

- Prefer strongly typed tags, specs, and registries over string lookups.
- Keep raw Raylib resource lifetime inside managers, caches, or RAII wrappers.
- Optional integrations must be feature-flag safe.
- Avoid hidden global state outside established managers and `Runtime`.
- Keep headers dependency-light when they are included across subsystems.
