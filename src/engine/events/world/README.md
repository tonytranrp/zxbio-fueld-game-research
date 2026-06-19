# engine/events/world

The typed event channel for general world lifecycle and terrain-modification
events. This folder defines the event structs and registers them with the
runtime's typed event dispatcher so any system can publish or subscribe.

## Current contents

```text
engine/events/world/
|-- WorldEvents.hpp        event structs (tile/chunk/world/heightmap/building) + stub domain types
`-- WorldEventModule.hpp   typed registration: tags, string specs, and the event module
```

## Events

`WorldEvents.hpp` (namespace `engine::events::world`) declares:

| Event                  | Fired when                                  |
|------------------------|---------------------------------------------|
| `TileChangedEvent`     | a tile's data changes (old/new `TileData`)  |
| `ChunkLoadedEvent` / `ChunkUnloadedEvent` | a chunk streams in/out    |
| `WorldCreatedEvent` / `WorldDestroyedEvent` / `WorldActivatedEvent` | world lifecycle |
| `HeightmapModifiedEvent` | a heightmap region is edited (bounds)     |
| `BuildingPlacedEvent` / `BuildingRemovedEvent` | a building is placed/removed |

`WorldEventModule.hpp` assigns each a `BIOFUEL_EVENT_TAG`, a stable string spec
(e.g. `"world.tile_changed"`), and bundles them into `WorldEventModule`.

## How other code uses it

Publish/subscribe through the typed event tags rather than raw struct names.
These are the *general* world events; the **physics-collider** world events live
separately in `engine/world/WorldEvents.hpp` and are registered with distinct
tags in `engine/world/WorldPhysicsEventModule.hpp` to avoid name collision.

## Invariant

Every event (and the stub domain types) must be trivially copyable so the entt
dispatcher can memcpy them — enforced by `static_assert` in `WorldEvents.hpp`.
