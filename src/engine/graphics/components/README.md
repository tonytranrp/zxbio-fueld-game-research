# engine/graphics/components

Reusable shader components attach runtime state to GLSL shaders. Unlike
`shaders/` modules, which are compile-time data descriptors, components own live
animation state and apply uniforms through a polymorphic interface.

## Current contents

```text
engine/graphics/components/
|-- ComponentModule.hpp
|-- ComponentManager.hpp/.cpp
`-- Camera/
```

## Architecture

```text
ComponentModule
|-- CameraComponent
`-- future components

ComponentManager
|-- updateAll(dt)
|-- applyAll(shader)
`-- getAs<T>(name)
```

Components are called once per frame to set a small number of uniforms. The
polymorphism cost is acceptable here because the manager can own any mix of
components without templating every caller.

## How to add a component

1. Create `components/<Thing>/<Thing>Component.hpp`.
2. Implement `ComponentModule`.
3. Define GLSL uniform constants as `static constexpr` members.
4. Add corresponding uniforms to the GLSL shader.
5. Register with `ComponentManager::add()`.

```cpp
auto camera = std::make_unique<CameraComponent>();
components.add(std::move(camera));
components.updateAll(dt);
components.applyAll(shader);
```

## Coding standards

- Components do not own shaders; they write into a `Shader` passed to `apply()`.
- Prefer typed shader uniform APIs when the shader has a module tag.
- Keep component names stable for `getAs<T>(name)` lookups.
- Do not put screen-specific animation policy in reusable components.
