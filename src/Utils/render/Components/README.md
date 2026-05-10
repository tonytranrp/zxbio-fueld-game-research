# Utils/render/Components

Reusable shader components that attach runtime state to GLSL shaders. Unlike
`Shader/` modules (compile-time data-only descriptors), components own
animation state and apply uniforms through a polymorphic interface.

## Architecture

```
ComponentModule (interface)
    │
    ├── CameraComponent      ← camera yaw/offset animation
    └── (future components)   ← fog, lighting, etc.

ComponentManager
    └── owns vector<unique_ptr<ComponentModule>>
        ├── updateAll(dt)     ← advances all animations
        ├── applyAll(shader)  ← writes all uniforms to GPU
        └── getAs<T>(name)    ← type-safe retrieval
```

## Files

```text
Components/
├── ComponentModule.hpp     ← Virtual base interface
├── ComponentManager.hpp    ← Bulk owner and orchestrator
├── ComponentManager.cpp
├── README.md               ← This file
└── Camera/
    ├── CameraComponent.hpp ← Camera ComponentModule implementation
    ├── ShaderCamera.hpp    ← ShaderCameraState + ShaderCameraController
    ├── ShaderCamera.cpp    ← Controller animation logic
    └── README.md           ← Camera-specific docs
```

## Design Decisions

### Why virtual dispatch here?

Shader modules in `Shader/` are called on the hot render path with tight
loops and must be zero-overhead — hence `constexpr` data and no virtual.

Components are called once per frame to set a handful of uniforms. The
polymorphism cost is negligible, and it enables `ComponentManager` to own
any mix of components without templating everything.

### How components connect to ShaderManager

Components use `ShaderManager::getLocation()` and `ShaderManager::setValue()`
to write uniforms. They do NOT own shaders — they write into whatever
`Shader` handle is passed to their `apply()` method.

## Adding a New Component

1. Create a folder: `Components/YourThing/`
2. Create `YourThingComponent.hpp` implementing `ComponentModule`
3. Define GLSL uniform constants as `static constexpr` members
4. Add the corresponding uniforms to your GLSL shader
5. Register via `ComponentManager::add(std::make_unique<YourThingComponent>())`
