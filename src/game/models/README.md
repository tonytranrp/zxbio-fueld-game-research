# game/models

`game/models/` owns typed runtime model assets, model instances, and the bridge between Raylib models and authored rig/keyframe playback.

The main-menu transition is shader-only and does not load any runtime models.

## Current contents

```text
game/models/
`-- README.md
```

This folder is currently just a placeholder for game-specific model notes. The
actual `ModelSystem` implementation (loading, registry, instances, animation
pairing) lives in `src/engine/models/` (`ModelSystem.hpp/.cpp`,
`ModelServiceModule.hpp`, `ModelAnimator.cpp`). See that folder's README for
the runtime model contract.

## Responsibilities

- Register future runtime model assets through `ModelAssetId` and `ModelAssetSpec`.
- Centralize model lifetime, shader pairing, animation states, and error handling.
- Create typed `ModelInstance` objects instead of open-coding `LoadModel()`.
- Keep raw Raylib model ownership out of screens and screen effects.

## Adding a new model

1. Add the asset under `assets/models/<asset_name>/`.
2. Add a local attribution `README.md`.
3. If the asset needs a shader, place that shader under `assets/shaders/`.
4. If the asset is rigged and should use authored motion, add a keyframe clip factory under `engine/animation/`.
5. Register the asset in the built-in registry in `ModelSystem.cpp`.
6. Use `Runtime::model().createInstance(...)` from the caller.

```cpp
auto& models = biofuel::engine::runtime::Runtime::model();
const auto registered = models.registry();
if (!registered.empty()) {
    auto instance = models.createInstance(registered.front().id);
    if (instance && instance->ready()) {
        instance->setAnimationState("idle");
    }
}
```

## Coding standards

- Keep raw `LoadModel()` and `UnloadModel()` calls inside `ModelSystem`.
- Use `ModelAssetSpec` for registration instead of ad hoc screen loading.
- Keep model IDs stable once gameplay or save data depends on them.
- Put reusable keyframe clip code in `engine/animation/`.
