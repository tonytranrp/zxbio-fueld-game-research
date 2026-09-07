# world/water — the wave field

The sea's surface motion (`docs/goals.md` Group AH, `Prompts/001-...` Group E): Gerstner waves
driven by `world/wind`, sampled by the marcher for shading and by the player for swimming.

## Scope, stated so it is not re-litigated

This is the wave **field** and the player's interaction with it. It is deliberately **not**
currents, shallow-water equations, foam advection, or breaking physics — the water research's
§5.3/§9 pipeline (shoaling, refraction, reef dissipation, radiation stress) is a future arc of its
own magnitude, named as the follow-up in `research/gameplay-pass-log.md`. The complaint that
motivated this was "the water is static"; a travelling surface that the swimmer rides answers it.

## Rules of this folder

- **Gerstner, not sines.** Water particles move in circles, not up and down. That is what sharpens
  crests and flattens troughs; a plain sine reads as jelly. It is also the exact nonlinear solution
  for deep-water gravity waves, so it is not a cheat — see the research's §8.2.
- **The steepness budget is a correctness constraint, not a taste one.** `Σ Q·k·A > 1` makes the
  surface self-intersect into visible loops. The field targets 0.6 and a test pins it at every wind
  speed, not just the default.
- **Normals come from the same sum**, analytically — never from a finite difference of the height.
  Gerstner moves points horizontally too, so a height difference disagrees with the crests at
  exactly the steepnesses that make crests worth having. There is a test comparing the analytic
  normal against the real geometric normal of the displaced surface.
- **Wind and waves share one field.** `make_wave_field(wind)` is the only way to build one, so
  `--no-wind` is glass by construction rather than by a flag each consumer checks.
- **CPU reference first, HLSL mirrors it** — the same rule the marcher lives by, for the same
  reason: it is only `sin`, `cos` and `sqrt`, so the two sides can agree exactly.

## Files

| file | what |
|---|---|
| `gerstner.hpp` | `GerstnerWave`, `WaveField`, `make_wave_field`, `wave_height` (physics), `wave_surface` (shading), `shore_fade` |
