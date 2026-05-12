# engine/custom/procedural/render

Reserved home for shared procedural render helpers.

## Current status

This folder is intentionally empty except for this README. The robot-hand
renderer currently lives in `engine/custom/procedural/hand/` because it is
specific to that rig. Move code here only when multiple procedural modules need
the same render primitive or draw policy.

## Expected use

Good candidates:

- common debug draw helpers for procedural rigs
- reusable stage/grid render policies
- shared material binding helpers

Not good candidates:

- game screen layout
- one-off hand renderer internals
- raw Raylib resource ownership that belongs in `mesh/` or `materials/`

## Coding standards

- Keep rendering helpers stateless when possible.
- Accept explicit camera, shader, and resource handles.
- Do not read global runtime services from low-level render helpers.
