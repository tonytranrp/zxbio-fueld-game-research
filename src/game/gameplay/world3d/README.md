# game/gameplay/world3d

First-person movement for the walkable voxel world. This folder holds the
kinematic character controller that `GamePlayScreen` drives to let the player
walk, sprint, look, and jump through the streaming block terrain.

## Current contents

```text
game/gameplay/world3d/
`-- FirstPersonController.hpp/.cpp   WASD + mouse-look + jump kinematic body
```

## FirstPersonController

A reusable, resource-free character: pure state + math. It reads Raylib input
each frame and integrates a simple kinematic body against a **ground-height
callback** (`HeightFn = f32(worldX, worldZ)`), so it works against any terrain
source.

| Member            | Role                                                         |
|-------------------|--------------------------------------------------------------|
| `setConfig(Config)` | eye height, walk/sprint speed, jump speed, gravity, mouse sensitivity, pitch clamp |
| `reset(feetPos)`  | place the player's feet and clear look/velocity state        |
| `update(dt, groundHeightAt)` | mouse-look, horizontal move, gravity, jump, ground snap |
| `camera()`        | the `Camera3D` for rendering                                  |
| `feetPosition()` / `eyePosition()` / `grounded()` / `yaw()` / `speed()` | queries for HUD/render |

## How it is used

`GamePlayScreen` owns one controller and feeds it
`VoxelWorld::groundHeight(x, z)` as the height callback. Because collision uses
the noise height function rather than built geometry, the player never falls
through the world even before any chunk mesh has been built. The controller is
updated at the render rate (in the screen's input step) so look and jump input is
never dropped.
