# NekoCat

Pixel-art 2D sprite animation system for the Neko cat companion, derived from
[crgimenes/neko](https://github.com/crgimenes/neko). Loads individual 32×32 PNG
frames (not a spritesheet) and drives a 2-frame directional animation cycle
controlled via WASD input.

## Quick start

```cpp
#include "game/presentation/sprites/NekoCat.hpp"
using namespace biofuel::game::presentation::sprites;

NekoCat cat;

// After raylib InitWindow():
cat.load();
cat.setPosition(400.0f, 300.0f);

// Each frame:
Direction dir = readWASDDirection();   // your input reader
cat.update(dt, dir);
cat.render();

// On shutdown:
cat.unload();
```

## How animation works

Every animation pose is a **32×32 PNG** stored as a separate file. There is no
spritesheet — each file is loaded into its own GPU texture. All textures use
`TEXTURE_FILTER_POINT` so pixels stay crisp at any scale.

### Frame cycle

Most poses have **2 frames**. The animation counter runs from 0 to
`frameDuration × 2`, selecting frame 1 while the counter is below
`frameDuration` and frame 2 while above. At the maximum value the counter
wraps to zero. This is the original crgimenes/neko model.

| Parameter         | Default  | Meaning                              |
|-------------------|----------|--------------------------------------|
| `frameDuration`   | 0.15 s   | Time each frame is displayed         |
| Full cycle        | 0.30 s   | `frameDuration × 2`; ~3.33 fps cycle |

`State::Awake` is the only single-frame pose (`awake.png`); the counter is
ignored and frame stays at 1.

### Walking

When `State::Walking` is active, the system selects frames by **direction**
(e.g. `up1.png` / `up2.png`, `downright1.png` / `downright2.png`). Each of the
8 cardinal/intercardinal directions has its own 2-frame set.

When input is `Direction::Idle` the cat switches to `State::Awake` but retains
its last facing direction for rendering.

### Idle behaviours

Non-walking states (`Awake`, `Scratching`, `Washing`, `Yawning`, `Sleeping`)
all use a single fixed sprite set regardless of direction. `Scratching`,
`Washing`, `Yawning`, and `Sleeping` each have 2 frames; `Awake` has 1.

## API

### Lifecycle

| Method          | Description                                                   |
|-----------------|---------------------------------------------------------------|
| `load()`        | Load all PNGs from `assets/sprites/neko/` into GPU textures. Must be called after `InitWindow()`. Idempotent. |
| `update(dt, dir)` | Advance animation, apply directional movement, clamp to screen. Call once per frame. |
| `render()`      | Draw current frame at `(x, y)` scaled by `m_scale`. No-op if `!isLoaded()`. |
| `unload()`      | Release all GPU textures. Safe to call multiple times.        |

### Setters

| Method                        | Description                              |
|-------------------------------|------------------------------------------|
| `setDirection(Direction)`     | Override facing direction.               |
| `setState(State)`             | Switch behavioural state (resets frame). |
| `setPosition(f32 x, f32 y)`   | Top-left corner in screen pixels.        |
| `setScale(f32)`               | Display scale (default 2.0 → 64×64 px).  |
| `setFrameDuration(f32)`       | Seconds per animation frame.             |

### Accessors

| Method             | Returns      |
|--------------------|--------------|
| `direction()`      | `Direction`  |
| `state()`          | `State`      |
| `x()` / `y()`      | `f32`        |
| `scale()`          | `f32`        |
| `frameDuration()`  | `f32`        |
| `currentFrame()`   | `i32` (1 or 2) |
| `isLoaded()`       | `bool`       |

### Move semantics

`NekoCat` is **move-only**. The copy constructor and copy-assignment operator
are deleted because `Texture2D` ownership must not be duplicated. The move
constructor and move-assignment transfer texture ownership and mark the source
as unloaded.

## Direction enum

9 values representing the compass bearing the cat faces or moves toward.

| Value         | WASD keys        | X delta | Y delta |
|---------------|------------------|---------|---------|
| `Up`          | W                | 0       | −1      |
| `Down`        | S                | 0       | +1      |
| `Left`        | A                | −1      | 0       |
| `Right`       | D                | +1      | 0       |
| `UpLeft`      | W + A            | −1      | −1      |
| `UpRight`     | W + D            | +1      | −1      |
| `DownLeft`    | S + A            | −1      | +1      |
| `DownRight`   | S + D            | +1      | +1      |
| `Idle`        | *(no keys held)* | 0       | 0       |

Diagonal directions are normalised by `1/√2` (≈ 0.707) so diagonal movement
speed matches cardinal speed. Movement speed is **200 px/s**.

## State enum

6 behavioural animation sets.

| Value        | Frames | Description                                   |
|--------------|--------|-----------------------------------------------|
| `Walking`    | 2      | Directional movement. Uses direction prefix.   |
| `Awake`      | 1      | Single idle frame. Default state after load.   |
| `Scratching` | 2      | Scratch animation (`scratch1.png`/`scratch2.png`). |
| `Washing`    | 2      | Wash animation (`wash1.png`/`wash2.png`).      |
| `Yawning`    | 2      | Yawn animation (`yawn1.png`/`yawn2.png`).      |
| `Sleeping`   | 2      | Sleep animation (`sleep1.png`/`sleep2.png`).   |

`setState()` resets the frame timer and frame index when the state actually
changes. Calling `setState()` with the current value is a no-op.

## WASD input mapping

Input is read outside `NekoCat` (the class is input-agnostic). The canonical
reader lives in `GamePlayScreen.cpp`:

```cpp
static Direction readWASDDirection() noexcept {
    const bool w = IsKeyDown(KEY_W);
    const bool a = IsKeyDown(KEY_A);
    const bool s = IsKeyDown(KEY_S);
    const bool d = IsKeyDown(KEY_D);

    // Compound directions checked first so they take priority
    if (w && d)  return Direction::UpRight;
    if (w && a)  return Direction::UpLeft;
    if (s && d)  return Direction::DownRight;
    if (s && a)  return Direction::DownLeft;
    if (w)       return Direction::Up;
    if (s)       return Direction::Down;
    if (a)       return Direction::Left;
    if (d)       return Direction::Right;

    return Direction::Idle;
}
```

WASD → direction mapping:

- **W** → Up
- **A** → Left
- **S** → Down
- **D** → Right
- **W+A** → UpLeft
- **W+D** → UpRight
- **S+A** → DownLeft
- **S+D** → DownRight
- *(none)* → Idle

## Asset layout

All sprite frames live under `assets/sprites/neko/`:

```
assets/sprites/neko/
├── awake.png              # Single idle frame
├── up1.png / up2.png      # Walking — up
├── down1.png / down2.png  # Walking — down
├── left1.png / left2.png  # Walking — left
├── right1.png / right2.png# Walking — right
├── upleft1.png / upleft2.png
├── upright1.png / upright2.png
├── downleft1.png / downleft2.png
├── downright1.png / downright2.png
├── scratch1.png / scratch2.png
├── wash1.png / wash2.png
├── yawn1.png / yawn2.png
├── sleep1.png / sleep2.png
├── upclaw1.png / upclaw2.png    # Present on disk but NOT loaded by NekoCat
├── leftclaw1.png / leftclaw2.png
├── rightclaw1.png / rightclaw2.png
├── downclaw1.png / downclaw2.png
└── fp_*.png                     # First-person view frames (not loaded)
```

**Naming convention:** `{prefix}{frame}.png` for 2-frame poses,
`{prefix}.png` for single-frame poses. The prefix is derived from
`directionPrefix()` (walking) or `statePrefix()` (all other states).

The `*claw*.png` and `fp_*.png` files are present on disk from the original
crgimenes/neko asset set but are **not** loaded by `NekoCat::load()`. The
`load()` function iterates only over the 8 cardinal/intercardinal
`Direction` values and the 5 non-walking `State` values (Awake, Scratching,
Washing, Yawning, Sleeping). The claw and first-person frames have no
corresponding enum values and are purely archival.

## Renderer::drawTextureRec extension

The `Renderer` utility class in `engine/graphics/Render.hpp` wraps common
raylib draw calls with C++ conveniences. One of its methods is
`drawTextureRec`:

```cpp
// Render.hpp — static method on the Renderer class
static void drawTextureRec(Texture2D texture, Rectangle source,
                           Vector2 position, Color tint);

// Render.cpp — delegates directly to raylib
void Renderer::drawTextureRec(Texture2D texture, Rectangle source,
                              Vector2 position, Color tint) {
    DrawTextureRec(texture, source, position, tint);
}
```

This wraps raylib's `DrawTextureRec`, which draws a sub-region (`source`
rectangle) of a texture at a screen `position`. It is the simplest
sub-region blit available — no rotation, no destination size override.

> **Note:** `NekoCat::render()` does **not** use `Renderer::drawTextureRec`.
> It calls raylib's `DrawTexturePro` directly because it needs destination
> scaling (`source` rect maps to a scaled `dest` rect). `drawTextureRec`
> draws the source region at its native pixel size, making it better suited
> for UI elements and debug overlays where no scaling is required.

`DrawTexturePro` is the more powerful counterpart — it accepts a destination
rectangle, an origin/rotation vector, and is what `NekoCat::render()` uses:

```cpp
DrawTexturePro(
    tex,
    Rectangle{0, 0, (f32)tex.width, (f32)tex.height},   // source
    Rectangle{m_x, m_y, spriteSize, spriteSize},          // dest (scaled)
    Vector2{0, 0}, 0.0f, WHITE);                          // origin, rotation, tint
```

## Reference

- Original project: [crgimenes/neko](https://github.com/crgimenes/neko)
- Header: `src/game/presentation/sprites/NekoCat.hpp`
- Implementation: `src/game/presentation/sprites/NekoCat.cpp`
- Consumer: `src/game/screens/gameplay/GamePlayScreen.cpp`
- Assets: `assets/sprites/neko/`
