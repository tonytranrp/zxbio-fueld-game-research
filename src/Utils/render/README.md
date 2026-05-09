# Utils/render

Render utilities wrap the most common Raylib drawing and shader operations used by the current UI flow.

## Current contents

```text
Utils/render/
|-- Render.hpp
|-- Render.cpp
|-- ShaderManager.hpp
|-- ShaderManager.cpp
`-- Shader/
```

## Renderer

`Renderer` is the shared boundary for common drawing tasks. It currently provides:

- frame begin/end helpers
- text drawing and text measurement
- rectangle and sprite drawing
- full-screen fill drawing
- render-texture-to-screen drawing
- screen width and height queries
- small RAII helpers for shader mode and texture mode pairing

Raw Raylib drawing is still acceptable inside this utility layer and tightly scoped low-level rendering helpers. Outside this boundary, prefer `Renderer` when a helper already exists.

## ShaderManager

`ShaderManager` owns shader compilation, caching, uniform lookup, and uniform updates.

Rules:

- compile production shaders during loading-screen startup
- use shader module constants for names and uniform identifiers
- prefer `ShaderManager::getLocation`, `setValue`, and `setValueTexture` over scattered direct Raylib shader calls

## Shader source pipeline

- authored GLSL lives in `assets/shaders/*.glsl`
- `cmake/EmbedShaders.cmake` generates `build/generated/ShaderSources.hpp`
- shader modules in `Utils/render/Shader/` reference those generated source constants

Current authored shader modules include blur passes, the crossfade shader, and the main-menu background shader.
