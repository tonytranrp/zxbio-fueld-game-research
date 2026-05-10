# Systems/Model

`Systems/Model/` owns typed runtime model assets, model instances, and the first pass of model animation state flow.

## Current contents

```text
Systems/Model/
|-- ModelSystem.hpp
|-- ModelSystem.cpp
`-- README.md
```

## Responsibilities

- register runtime model assets through `ModelAssetId` and `ModelAssetSpec`
- preload startup-critical models during the loading screen
- centralize model lifetime and error handling
- create typed `ModelInstance` objects instead of open-coding `LoadModel()`
- attach model shaders
- expose per-instance animator and render state
- support the first conservative animation graph: `idle -> action -> return`
- let clipless assets use the animator as a timing/state controller for effect behavior

## Format policy

- `.glb` is the standard runtime format
- other Raylib-supported formats may be accepted by the registry
- only `.glb` is first-class in this pass

## Ownership split

- `ModelSystem` owns asset registration, shared loaded asset data, and event hooks
- `ModelInstance` owns the concrete runtime model view used by a caller
- screens and screen helpers do not own raw model loading or unloading

## Trigger boundary

- local screen effects use direct instance calls such as `setAnimationState()` and `playAction()`
- broader cross-system animation triggers are designed to route through model events

## Adding a new model

1. Add the asset under `assets/models/<asset_name>/`
2. Add a local attribution `README.md`
3. If the asset needs a shader, place that shader under `assets/shaders/`
4. Register the asset in the built-in registry in `ModelSystem.cpp`
5. If it should preload during startup, set `preloadOnStartup = true`
6. Use `Data::models().createInstance(...)` from the caller instead of raw Raylib model APIs
