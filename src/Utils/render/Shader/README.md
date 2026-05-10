# Utils/render/Shader

This folder contains compile-time shader module descriptors. A module is a small data-only header that names a shader, points at embedded GLSL source, and exposes uniform-name constants used by consuming code.

## Current files

```text
Utils/render/Shader/
|-- ShaderModule.hpp
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
- any uniform-name constants used by consuming code

Modules stay lightweight on purpose: no virtual interfaces, no hidden registration, and no Raylib ownership logic.

## Relationship to Components

Shader modules define what a shader is: its name, source, and uniform names. Components in `../Components/` define runtime behavior that applies uniform values to shaders, such as the camera component writing `uCameraYaw`.

Uniforms managed by a component should be documented in the module header, while the constant definitions can live in the component class.

## Current pipeline

1. Write GLSL in `assets/shaders/*.glsl`
2. Add embedded screen shader names to `src/CMakeLists.txt`
3. Expose the shader through a module header here
4. Compile it in `LoadingScreen::buildTasks()`
5. Consume it through `ShaderManager`

This pipeline is for embedded screen shaders. File-based model shaders are allowed too; the current example is the hands shader paired through `ModelSystem`.

## Rules

- keep module headers data-only
- use `std::string_view` constants instead of raw string literals in consumer code
- keep screen-shader registration explicit in the loading screen
- prefer current-state docs and real modules over placeholder examples
