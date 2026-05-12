# engine/custom/procedural/rig

Reserved home for shared procedural rig descriptors and builders.

## Current status

This folder is intentionally empty except for this README. Robot-hand rig data
currently lives in `hand/` because it is hand-specific. Move reusable rig
building code here when a second procedural rig needs the same abstractions.

## Expected use

Good candidates:

- generic joint-chain descriptors
- skeleton construction helpers
- bind-pose generation shared across procedural models

Not good candidates:

- IK solvers, which belong in `ik/`
- mesh cache code, which belongs in `mesh/`
- hand-only dimensions, which belong in `hand/`

## Coding standards

- Rig structs should name coordinate spaces and units.
- Keep builders deterministic and independent from rendering.
- Avoid inheritance unless a real family of rigs needs it.
