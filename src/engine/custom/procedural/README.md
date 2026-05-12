# engine/custom/procedural

Reusable procedural content systems live here. These modules are engine-owned
and should be usable by debug tools, future gameplay, and tests without knowing
about concrete screens.

## Folder map

```text
engine/custom/procedural/
|-- core/       shared procedural types
|-- animation/  procedural rig animation helpers
|-- ik/         FABRIK and joint-limit helpers
|-- hand/       robot hand rig, retargeting, rendering, presets
|-- physics/    pose smoothing, fitting, separation, calibration mapping
|-- materials/  generated texture/material cache
|-- mesh/       generated mesh/model cache
|-- config/     reserved shared config loaders
|-- render/     reserved shared procedural render helpers
|-- resources/  reserved resource descriptors
`-- rig/        reserved shared rig descriptors
```

## Current production module

The robot-hand module owns rig dimensions, IK solving, typed animation clips,
material palettes, JSON presets, procedural/PNG texture support, cached
generated meshes, rendering, and hand-tracking retargeting.

Shared pose math, such as camera-frame mapping, two-hand calibration, smoothing,
pose separation, and visible-volume fitting, lives under `physics/`. Screens
should display controls and overlays; they should not own this math.

Preset defaults live in C++ specs and can be overridden by JSON files under
`assets/custom/procedural/hand/presets/`.

## Coding standards

- Use typed specs for defaults and JSON only for runtime tuning.
- Keep generated Raylib resources in caches.
- Keep coordinate spaces explicit in type and function names.
- Avoid game-screen dependencies inside procedural modules.
