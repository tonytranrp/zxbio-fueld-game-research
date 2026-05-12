# engine/graphics/shaders

Compile-time shader module descriptors live here. A module is a small data-only
header that names a shader, points at embedded GLSL source, and exposes uniform
name constants used by consuming code.

## Current contents

```text
engine/graphics/shaders/
|-- ShaderModule.hpp
|-- TypedShaderModule.hpp
|-- BlurHModule.hpp
|-- BlurVModule.hpp
|-- BlurCompositeModule.hpp
|-- CrossfadeModule.hpp
|-- LoadingPreludeModule.hpp
|-- MainMenuBgModule.hpp
`-- MenuOptionModule.hpp
```

## Module pattern

Each shader module should expose:

- `NAME`
- `FRAGMENT_SOURCE`
- `VERTEX_SOURCE`
- `CONFIG`
- uniform-name constants used by consuming code

Modules stay lightweight on purpose: no virtual interfaces, no hidden runtime
registration, and no Raylib resource ownership.

## Relationship to components

Shader modules define what a shader is. Components in `../components/` define
runtime behavior that applies uniform values to shaders, such as the camera
component writing `uCameraYaw`.

Uniforms managed by a component should be documented in the module header, while
constant definitions can live in the component class.

## Current pipeline

1. Write GLSL in `assets/shaders/*.glsl`.
2. Add embedded screen shader names to `src/CMakeLists.txt`.
3. Expose the shader through a module header here.
4. Compile it in `LoadingScreen::buildTasks()`.
5. Consume it through `engine::runtime::typed::Shaders`.

## Coding standards

- Keep module headers data-only.
- Use `std::string_view` constants instead of raw string literals in consumers.
- Keep screen-shader registration explicit in the loading screen.
- Prefer current-state docs and real modules over placeholder examples.
