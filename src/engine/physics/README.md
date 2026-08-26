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

For a player character, use `PhysicsWorld3D::moveCharacter(...)` (a kinematic
capsule + Rapier's `KinematicCharacterController`) rather than a dynamic
rigidbody: `PhysicsBodyDesc3D` does not carry rotation-lock fields all the way
to Rapier (see the next limitation), so a dynamic capsule would tip over.
`moveCharacter` takes the caller's authoritative position each call rather
than reading the collider's own cached pose, since `PhysicsSystem::stepFixed`
runs before screen update each frame and a same-frame `setBodyPosition` isn't
reflected in that cache until the next step. See `engine/character/README.md`
for the higher-level controller built on top of this.

**Known limitation:** `PhysicsBodyDesc3D` only carries `{kind, position,
linearVelocity, canSleep}` to the Rust bridge — `linearDamping`,
`angularDamping`, `gravityScale`, `enableCcd`, and the `lockTranslation*`/
`lockRotation` flags are accepted by the C++ struct but never reach Rapier.
Don't rely on the lock flags to keep a dynamic body upright.

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
