# GamePlayScreen

Gameplay entry screen for Biofuel Game — Fuel Farm. It is a walkable,
infinite, Minecraft-style voxel world: blocky terrain streams in around the
player as they walk, and they can move, sprint, look, and jump through it in
first person.

## Quick overview

```text
GamePlayScreen
├── VoxelWorld (engine::world::voxel)            ← streaming blocky terrain (chunks)
├── VoxelVolume (engine::world::voxel)           ← 3D texture the raymarcher reads
├── FirstPersonController (game::gameplay::world3d) ← WASD + mouse-look + jump
└── HUD                                          ← crosshair, title, controls, debug stats
```

The screen owns the voxel world, a baked voxel volume for the raymarched
renderer, and a first-person controller. It can render the world two ways and
toggle between them at runtime with **F6**:

- **Raymarch mode** (default): a John Lin-style raymarched-voxel shader
  (`assets/shaders/raymarched_voxels.glsl`) sampling `VoxelVolume`'s 3D texture,
  rendered at half resolution and point-upscaled for a crisp pixel look.
- **Raster mode** (fallback): streamed chunk meshes from `VoxelWorld`, drawn in
  `BeginMode3D`/`EndMode3D` with an animated water pass. Used automatically if
  the raymarch shader fails to compile.

## Lifecycle

| Phase        | What happens                                                   |
|--------------|----------------------------------------------------------------|
| `onEnter()`  | Configures `VoxelWorld` (view radius, seed, sea level), spawns the player at ground height, bakes `VoxelVolume`, compiles the raymarch shader, and captures the cursor for mouse-look. |
| `onExit()`   | Releases the cursor, unloads all chunks, unloads the volume, releases the raymarch render target. |
| `onPause()`  | Releases the cursor so the player can click an overlay (e.g. the pause popup). |
| `onResume()` | Re-captures the cursor for mouse-look. |
| `onUpdate()` | Fixed 60 Hz step: in raymarch mode re-bakes the GPU volume around the player; in raster mode streams chunk meshes. |
| `onInput()`  | Runs once per rendered frame: drives the `FirstPersonController` with the real frame delta (keeping mouse-look smooth and never dropping a jump), and toggles raymarch/raster with F6. |
| `onRender()` | Renders the world (raymarch or raster) then the HUD. |

The player is driven in `onInput()` rather than `onUpdate()` so look and jump
input is sampled at the render rate and never dropped when the render rate runs
ahead of the fixed update.

## Controls

| Input        | Action                  |
|--------------|-------------------------|
| W / A / S / D | Move (relative to look) |
| Mouse        | Look                    |
| LEFT-SHIFT   | Sprint                  |
| SPACE        | Jump                    |
| F6           | Toggle raymarch / raster renderer (only if the raymarch shader compiled) |
| ESC          | Pause (routed globally by `PauseController`) |

## First-person controller

Movement, mouse-look, gravity, jump, and ground snapping live in
`game::gameplay::world3d::FirstPersonController` — a reusable, resource-free
kinematic body. It integrates against a ground-height callback, so the screen
passes `m_voxels.groundHeight(x, z)` and the controller works directly off the
terrain noise function. Because collision uses the noise function rather than
built geometry, the player never falls through the world even before any chunk
mesh has been built. It produces a `Camera3D` for both render paths.

## Rendering order

1. World — `renderRaymarch()` (default) or `renderRaster()` (sky gradient +
   chunk meshes + animated water).
2. `renderHud()` — centered crosshair, "FUEL FARM — Voxel World" title, the
   controls hint, and a bottom debug line (position, speed, grounded/airborne,
   loaded chunk count).

## Transition policy

The screen uses a 0.35-second crossfade transition with ease-out cubic easing
when pushed or popped via the screen catalog:

```cpp
template<>
struct TransitionPolicy<GamePlayScreen> {
    static constexpr TransitionPolicyData VALUE{
        .duration = 0.35f,
        .easing = Easing::easeOutCubic,
        .composer = TransitionComposer::Crossfade,
    };
};
```

## File listing

| File                        | Purpose                                    |
|-----------------------------|--------------------------------------------|
| `GamePlayScreen.hpp`        | Class declaration, members                 |
| `GamePlayScreen.cpp`        | Lifecycle, input, render paths, cursor     |
| `GamePlayScreenModule.hpp`  | Screen catalog registration + transition   |
| `README.md`                 | This file                                  |

## Future direction

Keep farm simulation and domain state in `src/game/gameplay/`. When gameplay
features are implemented, connect them to `GamePlayScreen` deliberately — the
screen should remain a thin presentation layer that delegates to domain and
world systems.

## See also

- Voxel world — `src/engine/world/voxel/` (chunk streaming via `VoxelWorld`, the
  raymarched 3D texture via `VoxelVolume`, and the block model).
- [Screens overview](../README.md) — screen catalog conventions and coding
  standards.
