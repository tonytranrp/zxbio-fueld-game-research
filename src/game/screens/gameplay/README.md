# GamePlayScreen

Temporary gameplay entry screen for Biofuel Game — Fuel Farm. Hosts the Neko
cat companion as the primary interactive element with WASD-driven 8-directional
movement, a hand-model overlay, and placeholder UI text.

## Quick overview

```text
GamePlayScreen
├── NekoCat (presentation::sprites)    ← animated cat companion
├── HandModelOverlay (presentation)    ← mediapipe hand tracking overlay
└── Placeholder HUD text               ← "FUEL FARM" title + status message
```

The screen owns a `NekoCat` instance, a `HandModelOverlay`, and renders
centered placeholder text. All gameplay domain logic is deferred to
`src/game/gameplay/` for future implementation.

## Lifecycle

| Phase        | What happens                                                   |
|--------------|----------------------------------------------------------------|
| `onEnter()`  | Initialises hand tracking, positions NekoCat at screen center, calls `m_neko.load()`. |
| `onUpdate()` | Reads WASD input → drives NekoCat animation + movement. Updates hand overlay. |
| `onRender()` | Clears background (dark blue-grey `#12181C`), renders NekoCat, HUD text, hand overlay. |
| `onExit()`   | Calls `m_neko.unload()`, tears down hand overlay.              |

`onInput()` is currently a no-op — all input is polled synchronously in
`onUpdate()` via `readWASDDirection()`.

## NekoCat integration

`GamePlayScreen` is the primary consumer of `presentation::sprites::NekoCat`.
It owns the instance as a private member:

```cpp
presentation::sprites::NekoCat m_neko;
```

On enter, the cat is positioned at the screen center and its 41 PNG frames are
loaded into GPU textures. Each frame, `onUpdate()` calls:

```cpp
m_neko.update(dt, direction);   // advance animation, move, clamp to screen bounds
```

`onRender()` calls `m_neko.render()` after clearing the background, so the cat
draws on top of the solid fill. The cat renders at 3× scale (96×96 on-screen
pixels from 32×32 source art) with nearest-neighbour filtering for crisp edges.

When the screen exits, `m_neko.unload()` releases all GPU textures. The cat
supports move semantics, so ownership transfers cleanly if the screen instance
is moved.

## WASD controls

Input is read by a file-static helper in `GamePlayScreen.cpp`:

```cpp
static presentation::sprites::Direction readWASDDirection() noexcept;
```

This polls `IsKeyDown()` for W, A, S, D each frame and maps key combinations to
`Direction` enum values. Compound directions (diagonals) are checked first so
they take priority over single-key cardinals.

### Key mapping

| Keys held | Direction     | Movement                  |
|-----------|---------------|---------------------------|
| W         | `Up`          | North                     |
| S         | `Down`        | South                     |
| A         | `Left`        | West                      |
| D         | `Right`       | East                      |
| W + A     | `UpLeft`      | Northwest                 |
| W + D     | `UpRight`     | Northeast                 |
| S + A     | `DownLeft`    | Southwest                 |
| S + D     | `DownRight`   | Southeast                 |
| *(none)*  | `Idle`        | Stationary (triggers idle state cycling) |

### Movement properties

- **Speed:** 200 pixels per second (cardinal).
- **Diagonal speed:** Normalised by 1/√2 (~0.707) so diagonal movement speed
  matches cardinal speed.
- **Bounds clamping:** The cat is clamped to `[0, screenWidth - spriteSize]` ×
  `[0, screenHeight - spriteSize]` each frame. It cannot leave the window.

### Idle behaviour

When no keys are held, `readWASDDirection()` returns `Direction::Idle`. The cat
exits `Walking` state and enters the idle cycle, which auto-advances through
poses every 2.5 seconds:

```
Awake → Scratching → Washing → Yawning → Sleeping → Awake → ...
```

The cat retains its last facing direction while idle, so it renders facing the
direction it was last moving.

## Hand tracking overlay

`HandModelOverlay` provides a mediapipe-based hand skeleton rendered on top of
the gameplay view. It is initialised in `onEnter()` via
`ensureModelOnlyHandTracking()` and updated each frame. The overlay is
independent of the NekoCat — it does not interact with or control the cat.

## Rendering order

1. `ClearBackground(Color{18, 24, 28, 255})` — dark blue-grey fill.
2. `m_neko.render()` — cat at its current position and frame.
3. Placeholder HUD text — "FUEL FARM" title (gold, 34 px) and status message
   (light grey, 20 px), both horizontally centered.
4. `m_handOverlay.render()` — hand skeleton on top.

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
| `GamePlayScreen.cpp`        | Lifecycle, input polling, render order     |
| `GamePlayScreenModule.hpp`  | Screen catalog registration + transition   |
| `README.md`                 | This file                                  |

## Future direction

Keep farm simulation and domain state in `src/game/gameplay/`. When gameplay
features are implemented, connect them to `GamePlayScreen` deliberately — the
screen should remain a thin presentation layer that delegates to domain
systems.

## See also

- [NekoCat documentation](../../presentation/sprites/README.md) — animation
  system architecture, sprite loading, and how to add new poses.
- [Screens overview](../README.md) — screen catalog conventions and coding
  standards.
- [Hand overlay documentation](../../presentation/hands/README.md) — hand
  tracking model and rendering.
