# world/wind — one wind field

Everything that moves reads this (`docs/goals.md` Group AE, `Prompts/001-...` Group B): grass
blades, tree canopies, the water's wave directions. Nothing invents its own wind again — the mesh
path's ad-hoc sine wobble in `terrain.vsh.hlsl` is what that looked like, and it could never agree
with anything else because it was not a field, just a wiggle.

## Why `world/`, not `engine/`

`engine/` is infrastructure — clock, ECS, events, input, jobs; things a different game would reuse
unchanged. Wind is *this world's weather*: it has a direction because this world has one, its gusts
are sized for terrain a player walks across, and every consumer is world or render content. It sits
beside `world/generation` and `world/materials` for the same reason those do.

## Rules of this folder

- **Pure and allocation-free.** `sample_wind(params, position, time)` is a function of its
  arguments and nothing else — evaluate it in any order, on any thread, from the CPU marcher or a
  build job, and get the same answer.
- **Sines, not FastNoise2**, despite the rest of the project using it. This function has to exist
  twice — here and in `render/diligent/shaders/wind.fxh` — and produce the same numbers in both. A
  hash-based noise cannot promise that across two compilers and two float pipelines; a short sum of
  directional sines can, exactly, because it is only `sin` and `dot`.
- **The constants are not duplicated at all.** `detail::` in `wind_field.hpp` holds the wave shape,
  and `render/diligent/detail/wind_macros.hpp` compiles those same values into every wind-aware
  shader as `WIND_*` macros. The HLSL mirror contains no numbers of its own, so it cannot drift.
  Per-run tuning (speed, direction, gust amplitude) goes through a constant buffer instead, because
  `--wind-speed` has to change it without recompiling a shader.
- **Gusts travel.** The sampling frame scrolls along the wind direction, so a gust is a patch of
  fast air moving downwind — not a pulse in place. There is a test for exactly that.
- **`still_wind()` is how `--no-wind` works**: the field itself goes to zero, rather than every
  consumer checking a flag somewhere it might be forgotten.

## Files

| file | what |
|---|---|
| `wind_field.hpp` | `WindParams`, `WindSample`, `sample_wind`/`wind_gust`/`wind_flutter`; `detail::` = the wave shape both sides compile |
| `src/wind_field.cpp` | the reference implementation `wind.fxh` mirrors |
