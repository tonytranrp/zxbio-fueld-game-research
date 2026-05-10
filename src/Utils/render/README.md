# Utils/render

Render utilities wrap the common Raylib drawing and shader operations used by the current UI and screen flow.

## Current contents

```text
Utils/render/
|-- Render.hpp / .cpp          <- Shared drawing boundary
|-- RenderSurface.hpp          <- RAII render texture helper
|-- ShaderManager.hpp / .cpp   <- Shader compilation, caching, uniforms
|-- Shader/                    <- Compile-time shader module descriptors
|   |-- ShaderModule.hpp       <- Convention docs + ShaderModuleConfig
|   |-- BlurHModule.hpp
|   |-- BlurVModule.hpp
|   |-- BlurCompositeModule.hpp
|   |-- CrossfadeModule.hpp
|   |-- LoadingPreludeModule.hpp
|   |-- MainMenuBgModule.hpp
|   `-- MenuOptionModule.hpp
`-- Components/                <- Runtime shader components
    |-- ComponentModule.hpp
    |-- ComponentManager.hpp / .cpp
    `-- Camera/
        |-- CameraComponent.hpp
        |-- ShaderCamera.hpp / .cpp
        `-- README.md
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

- compile production screen shaders during loading-screen startup
- use shader module constants for names and uniform identifiers
- prefer `ShaderManager::getLocation`, `setValue`, and `setValueTexture` over scattered direct Raylib shader calls

## Components

Runtime shader components own per-frame animation state and apply uniforms through a polymorphic `ComponentModule` interface. Unlike `Shader/` modules, components carry live behavior and state.

See [Components/README.md](./Components/README.md) for architecture and extension guidance.

## Shader source pipeline

- authored GLSL lives in `assets/shaders/`
- embedded screen shaders use `cmake/EmbedShaders.cmake` to generate `build/generated/ShaderSources.hpp`
- module headers in `Utils/render/Shader/` reference those generated embedded sources
- file-based model shaders can also live in `assets/shaders/` and be paired at runtime through `ModelSystem`

Current embedded shader modules include blur passes, the crossfade shader, the loading prelude, the main-menu background shader, and the menu option shader. Model-specific shaders such as the hands shader are currently loaded from file rather than embedded modules.
