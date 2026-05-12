# game/models

`game/models/` owns typed runtime model assets, model instances, and the runtime bridge between model state machines and authored keyframed rig playback.

## Current contents

```text
game/models/
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
- expose per-instance animator, keyframe state, and render state
- support the first conservative animation graph: `idle -> action -> return`
- bind authored model keyframe clips onto rigged models
- apply per-instance bone poses without pushing raw Raylib calls into screens
- allow dev-only runtime bone translation offsets for controller overlays

## Format policy

- `.glb` is the standard runtime format
- other Raylib-supported formats may be accepted by the registry
- only `.glb` is first-class in this pass

## Ownership split

- `ModelSystem` owns asset registration, shared loaded asset data, and event hooks
- `ModelInstance` owns the concrete runtime model view used by a caller, including per-instance rig pose buffers when needed
- screens and screen helpers do not own raw model loading or unloading
- dev tools may apply temporary per-instance offsets through `ModelInstance`; those offsets are runtime-only and reset on launch

## Keyframed rig flow

- clip authoring lives in `engine/animation/`
- asset specs can register a typed keyframe clip factory
- `ModelAnimator` still owns the high-level state machine
- `ModelKeyframePlayer` samples the active authored clip and applies the resulting pose to the instance model
- screen helpers consume the resulting root offsets / scalar channels instead of manufacturing the motion from ad hoc pulse math
- `ModelControllerOverlay` can add temporary offsets after the sampled pose so hand/camera placement can be tuned visually before constants are copied into authored code

## Trigger boundary

- local screen effects use direct instance calls such as `setAnimationState()` and `playAction()`
- broader cross-system animation triggers are designed to route through model events

## Adding a new model

1. Add the asset under `assets/models/<asset_name>/`
2. Add a local attribution `README.md`
3. If the asset needs a shader, place that shader under `assets/shaders/`
4. If the asset is rigged and should use authored motion, add a keyframe clip factory under `engine/animation/`
5. Register the asset in the built-in registry in `ModelSystem.cpp`
6. If it should preload during startup, set `preloadOnStartup = true`
7. Use `Runtime::model().createInstance(...)` from the caller instead of raw Raylib model APIs

## Dev model controller

Configure with `-DBIOFUEL_DEV_MODEL_CONTROLLER=ON` to enable the runtime controller overlay. Keep `-DBIOFUEL_DEV_STARTUP_MENU_TRANSITION=OFF` when you want normal menu entry instead of auto-running the transition. The overlay lets callers expose points such as root position, camera target, or weighted hand/bone clusters; dragging the gizmo changes runtime offsets only, and pressing `C` copies the selected offset text for manual use in C++ keyframes or framing constants.
