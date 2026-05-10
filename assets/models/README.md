# assets/models

This folder stores authored runtime model assets for the game.

## Runtime format policy

- `.glb` is the standard runtime format for this repo
- other Raylib-supported model formats are allowed when needed
- only `.glb` is first-class in the current docs, examples, and testing

## Folder rules

- each runtime model lives in its own subfolder
- that subfolder should contain the model file and a local `README.md`
- the local `README.md` should record attribution, source URL, license, and any preprocessing notes

Example:

```text
assets/models/
`-- menu_transition_hands/
    |-- low_poly_hands.glb
    `-- README.md
```

## Registration rule

If a model is used by the runtime, especially in startup-preloaded UI or screen flows, it must be registered in `src/Systems/Model/ModelSystem.cpp` through the typed model registry.

That registry is the source of truth for:

- debug/display name
- model path
- optional shader pairing
- preload-on-startup behavior
- optional animation-state metadata

## Shaders

Model-local shaders still live in `assets/shaders/`, not inside the model folder. The model registry pairs them with the correct asset.
