# engine/world

Procedural terrain, world-state storage, and 3D world rendering live here. This
folder owns two distinct things: the engine `WorldSystem` service (tile/chunk
world state with multi-world support and JSON persistence) and a family of
standalone renderable terrain modules (heightmap noise, extruded 2.5D worlds,
and the streaming voxel world in `voxel/`).

## Current contents

```text
engine/world/
|-- WorldSystem.hpp/.cpp          service backend: tile/chunk state, multi-world
|-- WorldServiceModule.hpp        registers WorldSystem as a static engine service
|-- WorldManager.hpp/.cpp         create/destroy/load/save on top of WorldSystem (JSON)
|-- WorldTypes.hpp                world enums + coord/tile/chunk aggregates (trivially copyable)
|-- WorldEvents.hpp               physics-collider world events (engine::world namespace)
|-- WorldPhysicsEventModule.hpp   typed registration for the physics-collider events
|-- TerrainGenerator.hpp/.cpp     procedural heightmap + biome-map generation (sin/cos noise)
|-- Terrain3D.hpp/.cpp            single walkable noise terrain surface (one GPU mesh + height field)
|-- HeightmapWorld3D.hpp/.cpp     extruded 2.5D world: cuboid-per-cell + physics + orbit camera
|-- VoxelChunkRenderer.hpp/.cpp   chunked cuboid renderer with per-chunk dirty/visibility state
`-- voxel/                        infinite streaming block world (see voxel/README.md)
```

## How it fits

- `WorldSystem` is reached through `Runtime::world()` (registered via
  `WorldServiceModule`). It owns chunk/tile state for up to `kMaxWorlds`
  worlds, with lazy chunk creation and bounds-checked tile access. `WorldManager`
  wraps it for lifecycle + persistence and is a companion, not a separate service.
- `TerrainGenerator` produces a `HeightmapData` + `BiomeMap` that
  `HeightmapWorld3D` (via `VoxelChunkRenderer`) extrudes into a collidable 3D
  world. `Terrain3D` is the simpler self-contained walkable surface.
- The currently shipped gameplay uses the streaming `voxel/` world, not the
  extruded `HeightmapWorld3D` path.

## Invariants

- `WorldTypes` aggregates are kept trivially copyable (static-asserted) so they
  ride the entt dispatcher safely.
- Terrain/voxel render calls must run between `BeginMode3D` / `EndMode3D`.
- Raw Raylib mesh/model lifetime stays inside these classes; game code never
  touches it.
