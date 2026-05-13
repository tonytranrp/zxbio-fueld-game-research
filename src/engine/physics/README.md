# engine/physics

Rapier-backed rigid-body physics lives here. This folder owns real collision,
rigid body simulation, raycasts, and contact events for both 2D and 3D worlds.

## Current contents

```text
engine/physics/
|-- PhysicsServiceModule.hpp
|-- PhysicsSystem.cpp
|-- PhysicsSystem.hpp
|-- PhysicsTypes.hpp
|-- README.md
`-- rapier_bridge/
```

## How to use it

The service exposes separate typed worlds:

```cpp
auto& physics = biofuel::engine::runtime::Runtime::physics();

auto body = physics.world3D().createBody({
    .kind = biofuel::engine::physics::PhysicsBodyKind::Dynamic,
    .position = Vector3{0.0f, 2.0f, 0.0f},
});

physics.world3D().attachCuboid(body, {
    .halfExtents = Vector3{0.5f, 0.5f, 0.5f},
});
```

Physics uses meter-style world units. Pixel screens should use
`PixelToMeterScale` at the boundary instead of tuning Rapier in raw pixels.

## Coding standards

- Keep Rapier internals inside the Rust bridge.
- Expose typed C++ handles and descriptors, not raw Rust or Rapier types.
- Step physics once from the fixed update path.
- Poll contacts after stepping; do not query per entity during rendering.
- Add new shapes deliberately and keep 2D/3D APIs dimension-specific.
