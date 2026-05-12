# engine/custom/procedural/resources

Reserved home for procedural resource descriptors.

## Current status

This folder is intentionally empty except for this README. It exists as a place
for reusable typed descriptors that are not themselves meshes, materials, rigs,
or configs.

## Expected use

Good candidates:

- typed IDs for generated procedural resource families
- resource manifest structs shared by multiple procedural modules
- import/export metadata for procedural tooling

Not good candidates:

- binary assets
- generated output
- screen-specific state

## Coding standards

- Keep descriptors plain and serializable.
- Use stable names and typed IDs instead of ad hoc strings.
- Resource lifetime should still be owned by caches or managers.
