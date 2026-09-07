# `dev/scenario` — what a verification IS, as data

## What belongs here

The *description* of a scenario, and nothing that executes one:

- `pose.hpp`, `input_frame.hpp` — the vocabulary. `InputFrame` carries
  `world::player::PlayerIntent` rather than a second input vocabulary, because that struct is
  already what the controller speaks and is already GLFW-free.
- `motion_script.hpp` / `src/motion_script.cpp` — segments (`hold`, `look`, `goto`, `wait`) and the
  driver that turns them into one `InputFrame` per fixed tick.
- `capture.hpp`, `assertion.hpp` — when to grab a frame, and what must be true at the end.
- `scenario.hpp` — the whole thing in one struct.
- `parser.hpp` / `src/parser.cpp` — the `.scn` text format, its diagnostics, and `emit()` for the
  round-trip test.
- `registry.hpp` / `src/registry.cpp` — the self-registering factory for built-in scenarios
  (`cpp-heavy-templates/references/modular-architecture.md` §4).

## What does not belong here

**Anything that runs one.** No Diligent, no GLFW, no FastNoise2 — the same firewall
`render/interface` keeps. `dev/harness` is the executor; this module is the noun. That is what
lets the whole module build and test under `-DVOXEL_BUILD_RENDERER=OFF`, in the gating CI job.

## The rules of this folder

1. **A scenario is data.** Adding one is adding a `.scn` file under `dev/scenarios/` — it must not
   require a rebuild. A built-in (code) scenario exists only for the cases that genuinely need
   computation, and registers itself from its own translation unit.
2. **The input a scenario produces is the input the game takes.** `InputFrame::intent` is
   `world::player::PlayerIntent`. If a scenario can express an input the player cannot, the harness
   is testing something the game does not do — which is the `--autofly` bug of the previous pass,
   in a new costume.
3. **`jump_pressed` is an EDGE.** `world/player/README.md` says a jump press reaches exactly one
   fixed tick; a `hold` segment naming `jump` therefore sets it on its first tick only. A test
   asserts the exact tick sequence.
4. **Every parse failure names file, line and what was expected.** A `.scn` is written by a human at
   2am; "parse error" is not an acceptable message.
5. **The format round-trips.** `parse(emit(s)) == s` for every checked-in scenario, asserted by a
   test. That is what keeps `emit()` honest enough to be used for a report.
6. **`include` resolves relative to the including file and rejects cycles.** This is the one place
   this repo pays for nesting; `@response-file` in `engine/cli` deliberately does not.
