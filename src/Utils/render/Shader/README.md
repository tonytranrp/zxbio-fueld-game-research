# Utils/render/Shader

This folder contains compile-time shader module descriptors. A module is a small class with `constexpr` metadata that names a shader, points at the embedded GLSL source, and exposes uniform-name constants.

## Current files

```text
Utils/render/Shader/
|-- ShaderModule.hpp
|-- BlurHModule.hpp
|-- BlurVModule.hpp
|-- CrossfadeModule.hpp
|-- MainMenuBgModule.hpp
`-- README.md
```

## Module pattern

Each shader module should expose:

- `NAME`
- `FRAGMENT_SOURCE`
- `VERTEX_SOURCE`
- `CONFIG`
- any uniform-name constants used by consuming code

Modules stay lightweight on purpose: no virtual interfaces, no hidden registration, and no Raylib ownership logic.

## Current pipeline

1. write GLSL in `assets/shaders/*.glsl`
2. add the shader name to `src/CMakeLists.txt`
3. expose it through a module header here
4. compile it in `LoadingScreen::buildTasks()`
5. consume it through `ShaderManager`

## Rules

- keep module headers data-only
- use `std::string_view` constants instead of raw string literals in consumer code
- keep registration explicit in the loading screen
- prefer current-state docs and real modules over placeholder examples
