# engine/graphics/components

Reusable shader components attach runtime state to GLSL shaders. Unlike
`shaders/` modules, which are compile-time data descriptors, components own live
animation state and apply uniforms through a polymorphic interface.

## Current contents

```text
engine/graphics/components/
|-- ComponentModule.hpp
`-- Camera/
```

## Architecture

```text
ComponentModule (interface)
|-- CameraComponent
`-- future components
```

There is no component manager. Screens own the components they need directly
as members and drive them each frame:

```text
screen.update(dt)  ->  component.update(dt)
screen.render()    ->  component.apply(shader)
```

Components are called once per frame to set a small number of uniforms. The
polymorphism cost is acceptable here because callers can hold any mix of
components without templating every call site.

## How to add a component

1. Create `components/<Thing>/<Thing>Component.hpp`.
2. Implement `ComponentModule`.
3. Define GLSL uniform constants as `static constexpr` members.
4. Add corresponding uniforms to the GLSL shader.
5. Own an instance in the screen that uses it and call `update(dt)` /
   `apply(shader)` from that screen.

```cpp
CameraComponent m_cameraComponent;

// per frame:
m_cameraComponent.update(dt);
m_cameraComponent.apply(shader);
```

## Coding standards

- Components do not own shaders; they write into a `Shader` passed to `apply()`.
- Prefer typed shader uniform APIs when the shader has a module tag.
- Do not put screen-specific animation policy in reusable components.
