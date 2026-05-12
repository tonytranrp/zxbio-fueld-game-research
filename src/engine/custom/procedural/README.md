# engine/custom/procedural

Reusable procedural content systems.

The robot-hand module is the first production-style system here. It owns rig dimensions, IK solving, typed animation clips, material palettes, JSON presets, procedural/PNG texture support, cached generated meshes, and rendering. Debug screens and future gameplay tools should consume this module instead of owning hand-generation code directly.

Shared procedural pose math, such as smoothing, pose separation, and visible-volume fitting, lives under `physics/` so screen code can stay focused on input, camera, and presentation.

Preset defaults live in C++ specs and can be overridden by JSON files under `assets/custom/procedural/hand/presets/`.
