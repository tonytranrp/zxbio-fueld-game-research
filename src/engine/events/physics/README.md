# engine/events/physics

Typed physics events live here. Collision events are emitted by the
Rapier-backed physics service after each fixed simulation step. Shape lifecycle
and grab events are emitted by higher-level engine interaction systems that own
the body-to-domain mapping.

## Current contents

```text
engine/events/physics/
|-- PhysicsEventModule.hpp
|-- PhysicsEvents.hpp
`-- README.md
```

## How to use it

Subscribe through the typed event layer:

```cpp
using namespace biofuel::engine::runtime::typed;
Events::sink<physics::CollisionStarted>().connect<&onCollisionStarted>();
Events::sink<physics::ShapeGrabStarted>().connect<&onShapeGrabStarted>();
```

Physics events carry the world kind plus raw collider handles. Game systems that
need entity ownership should keep their own collider-to-entity table.

## Coding standards

- Publish events from the physics service after a fixed step, not during render.
- Publish shape lifecycle/grab events from the engine system that creates the
  shape.
- Keep event payloads small and copyable.
- Prefer collider handles in events; callers can resolve richer context through
  their own domain mapping.
