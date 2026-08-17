# engine/world

The physics-collider bridge that connects farm tiles to Rapier 2D bodies. This
is a small, focused module — not the general-purpose world/voxel terrain (that
lives in `engine/world/voxel/`, see its own README).

## Current contents

```text
engine/world/
|-- WorldEvents.hpp
|-- WorldPhysicsEventModule.hpp
|-- README.md
`-- voxel/
```

`WorldEvents.hpp` defines the event payloads (`TileChangedEvent`,
`BuildingPlacedEvent`, `BuildingRemovedEvent`) that `WorldPhysicsIntegration`
(in `src/game/gameplay/`) publishes when it bakes or removes tile/building
colliders. `WorldPhysicsEventModule.hpp` registers them with the typed event
system under the `WorldPhysicsTileChanged` / `WorldPhysicsBuildingPlaced` /
`WorldPhysicsBuildingRemoved` tags.

## Important: two unrelated "world event" families share a namespace pattern

There is a second, general-purpose event family at
`engine/events/world/WorldEvents.hpp` (a **different file**, in a different
folder) with its own `WorldEventModule.hpp` and its own tag names
(`world.tile_changed`, `world.chunk_loaded`, etc., currently unpublished
placeholders for a not-yet-integrated world subsystem). The two files have the
same basename in different directories and both define a `TileChangedEvent`-
shaped struct in slightly different namespaces — this has already tripped up
a prior cleanup pass. If you need to reference either one, always use the
fully-qualified path or say explicitly which folder you mean:

- `engine/world/WorldEvents.hpp` (this folder) — physics-collider bridge,
  tags prefixed `WorldPhysics*`, real events with real publishers.
- `engine/events/world/WorldEvents.hpp` — general-purpose world domain events,
  tags un-prefixed (`ChunkLoaded`, `TileChanged`, etc.), currently unwired
  placeholders.

`WorldPhysicsEventModule.hpp`'s own doc comment calls out this exact split
and the deliberate distinct tag names chosen to avoid a registry collision.

## Coding standards

- Keep this folder's event payloads about physics-collider state only — general
  world/terrain domain events belong in `engine/events/world/`, not here.
- New events here need distinct `WorldPhysics*`-prefixed tag names, never the
  bare names used by the general-purpose world event family.
