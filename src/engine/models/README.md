# engine/models

3D model asset loading, instancing, and skeletal/keyframe animation. This folder
owns the `ModelSystem` engine service: it loads model prototypes once, hands out
shared instances, applies model shaders, and drives per-instance animation.

## Current contents

```text
engine/models/
|-- ModelSystem.hpp           ModelSystem, ModelInstance, ModelAnimator, asset specs
|-- ModelSystem.cpp           asset loading, instance pool, shader/normalization
|-- ModelAnimator.cpp         animation state machine implementation
`-- ModelServiceModule.hpp    registers ModelSystem as the "service.model" runtime service
```

## Key types

| Type             | Role                                                          |
|------------------|--------------------------------------------------------------|
| `ModelSystem`    | Singleton service. `init`/`shutdown`/`update`, `createInstance`, `preload`, registry queries. Owns shared `SharedAssetData` per `ModelAssetId` and tracks live instances via weak pointers. |
| `ModelInstance`  | One renderable instance of an asset. `draw(ModelRenderState)`, `setAnimationState`/`playAction`, per-bone translation offsets. Shares the prototype unless an independent model is needed. |
| `ModelAnimator`  | Animation state machine: named states with clip index, loop flag, return state, and duration; supports transitions between states. |
| `ModelAssetSpec` | Static description of an asset (path, shader, preload/residency flags, animation states + keyframe-clip factory). |

## How other code uses it

Reach the system via the runtime service registered by `ModelServiceModule`
(`service.model`). Create instances with `ModelSystem::createInstance(assetId)`,
advance them through `ModelSystem::update(dt)`, and render via
`ModelInstance::draw()`. Animation can also be driven by typed model events
(`ModelSetStateEvent` / `ModelPlayActionEvent`), which the system subscribes to.

## Invariants

- Raylib `Model`/`ModelAnimation`/`Shader` lifetime is owned here; callers hold
  `shared_ptr<ModelInstance>` and never free GPU resources directly.
- The `ModelAssetId` enum is currently empty — no built-in assets are registered;
  the registry is populated by spec entries when assets are added.
