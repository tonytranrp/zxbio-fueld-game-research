# engine/events/physics

Typed physics events live here. These events are emitted by the Rapier-backed
physics service after each fixed simulation step.

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
```

Physics events carry the world kind plus raw collider handles. Game systems that
need entity ownership should keep their own collider-to-entity table.

## Coding standards

- Publish events from the physics service after a fixed step, not during render.
- Keep event payloads small and copyable.
- Prefer collider handles in events; callers can resolve richer context through
  their own domain mapping.
