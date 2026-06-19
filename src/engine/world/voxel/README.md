# engine/world/voxel

The infinite, Minecraft-style block world. This folder owns two cooperating
pieces: `VoxelWorld`, the streaming chunk store and mesher, and `VoxelVolume`,
a bounded GPU voxel grid baked from the same terrain for the fullscreen
raymarcher. Both are engine-owned (raw Raylib mesh/texture lifetime lives in the
`.cpp` files).

## Current contents

```text
engine/world/voxel/
|-- VoxelWorld.hpp/.cpp    streaming chunked block world + chunk mesher
`-- VoxelVolume.hpp/.cpp   dense voxel grid baked to a GPU texture for raymarching
```

## VoxelWorld

An infinite world where terrain is a pure deterministic function of world
position (a noise heightmap), so chunks generate and mesh independently.

- `configure(Config)` — view radius, build budget/frame, seed, noise params,
  sea level, water wave settings.
- `update(playerPosition)` — streams chunks in/out around the player, building at
  most `maxBuildsPerFrame` meshes (anti-hitch). Each chunk is ONE GPU mesh with
  hidden-face culling.
- `render()` / `renderWater(timeSeconds)` — draw chunks and translucent water
  (between `BeginMode3D` / `EndMode3D`).
- `groundHeight(x, z)` — world Y the player stands on; usable for collision before
  any mesh exists.
- `blockAt(x, y, z)` / `surfaceHeight(x, z)` — pure terrain samplers, exposed so
  `VoxelVolume` can bake from the identical world definition.

`Block` (Air, Grass, Dirt, Stone, Sand, Snow, Wood, Leaves) is the shared
material enum. `kChunkSize` is 16 blocks on X and Z.

## VoxelVolume

A bounded W×H×D grid around the player, baked from `VoxelWorld` and uploaded for
the raymarched-voxel shader. GL 3.3 (raylib) has no 3D textures, so the grid is
flattened into a 2D R8 texture of size `W × (H*D)`: voxel `(x, y, z)` lives at
texel `(x, y + z*H)` with value = block id.

- `update(world, playerPosition)` — re-bakes and re-uploads when the player moves
  past `recenterThreshold` (or on first call); returns true if rebuilt.
- `texture()` / `originWorld()` / `width/height/depth()` — feed the shader.

## Invariant

`VoxelVolume` must bake from the same `VoxelWorld` instance the mesher uses, so
the raymarched and rasterized views agree.
