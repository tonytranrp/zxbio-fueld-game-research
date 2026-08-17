# engine/physics

Rapier-backed rigid-body physics lives here. This folder owns real collision,
rigid body simulation, raycasts, shape roles, and contact events for both 2D
and 3D worlds.

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
`engine/core/units` wrappers plus `PixelToMeterScale` at the boundary instead
of tuning Rapier in raw pixels.
Reusable interaction systems can tag bodies with `PhysicsShapeRole` when they
publish higher-level shape lifecycle or grab events.
For input-driven props, keep higher-level interaction logic outside the physics
service and feed Rapier bounded kinematic interactors plus dynamic props.

**Known limitation:** `CollisionGroup` currently filters C++-side contact-event
*reporting* only — the bridge's collider descriptors have no group field, so
Rapier itself never sees the group and colliders in "disjoint" groups still
physically collide. Do not rely on `CollisionGroup` to keep two shapes apart;
it only controls whether you get notified about a contact, not whether one
happens.

## Coding standards

- Keep Rapier internals inside the Rust bridge.
- Expose typed C++ handles and descriptors, not raw Rust or Rapier types.
- Step physics once from the fixed update path.
- Poll contacts after stepping; do not query per entity during rendering.
- Use kinematic bodies for player/tool-driven interactors and dynamic bodies
  for props that Rapier should move.
- Add new shapes deliberately and keep 2D/3D APIs dimension-specific.
