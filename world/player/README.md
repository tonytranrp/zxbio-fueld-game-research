# world/player — the player controller

The body's own physics (`docs/goals.md` Group AD, `Prompts/001-...`): a fixed-timestep simulation
of stance, gravity, jumping, swimming and the render-only "feel" layer, swept against whatever
world answers `world::collision::SolidQuery`.

## Why this lives in `world/`, not `app/`

The brief suggested an `app`-level struct. `app/` is only built when `VOXEL_BUILD_RENDERER=ON`, so
its tests never run in CI's gating no-GPU `core` job — and a jump/coyote/buffer state machine whose
determinism is the whole point is exactly the code that must be covered there. Nothing here needs a
window, a device, or GLFW, so nothing here belongs behind the renderer flag.

## Rules of this folder

- **The world is a concept.** `step_player` is a template over `SolidQuery`, like the sweep it
  calls. Tests drive a flat-ground/wall/pool fake; the app drives `TerrainCollider`.
- **The feet are exact; only the view is smoothed.** Eye smoothing (A3) and view polish (A6) return
  an *offset the renderer adds*. `PlayerState::eye_smooth_offset` and the polish terms never touch
  `position`, so the walk-violation counter, `--autofly` and `--verify-frame` measure the same body
  whether they are on or off.
- **Every tick is the same length.** `FixedStepper` hands out a count; the simulation is a pure
  function of that count, never of render cadence. Anything reading wall-clock `dt` inside a tick
  is a bug.
- **`PlayerIntent::jump_pressed` is an edge**, set on exactly one tick per key press. Holding Space
  must not re-arm the buffer every tick.
- **Numbers live in `PlayerTuning`**, one struct, defaults = the shipped feel. No magic constants
  at a use site.

## Files

| file | what |
|---|---|
| `tuning.hpp` | every tunable number; `kSvoStepHeight`, sea level, the Water material's swim physics |
| `player_state.hpp` | `MoveMode`, `Stance`, `PlayerState`, `PlayerIntent`, `WorldSense`, `StepResult`; the pure pieces |
| `fixed_step.hpp` | `FixedStepper` — accumulator, tick count, `alpha()` |
| `controller.hpp` | `step_player<SolidQuery>` — one tick, in order, with the reasons |
| `view_polish.hpp` | head-bob, landing dip, boost FOV kick (render-only, clamped) |
