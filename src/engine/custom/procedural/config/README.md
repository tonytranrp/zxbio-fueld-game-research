# engine/custom/procedural/config

Reserved home for typed configuration loaders used by procedural systems.

## Current status

This folder is intentionally empty except for this README. Existing procedural
hand presets currently keep their defaults in C++ specs and read JSON from the
hand module. If more procedural systems need shared config parsing, move the
reusable loader code here.

## Expected use

Good candidates:

- common JSON validation helpers for procedural presets
- strongly typed versioned config structs
- shared config error reporting

Not good candidates:

- one-off screen tuning constants
- raw asset files
- generated build output

## Coding standards

- Config structs should be versioned when stored on disk.
- Defaults belong in C++ types; files override them.
- Keep parsing errors recoverable so tools can fall back to defaults.
- Do not make config loading depend on concrete game screens.
