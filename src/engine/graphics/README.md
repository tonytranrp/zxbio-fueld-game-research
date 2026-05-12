# engine/graphics

Render utilities wrap the common Raylib drawing and shader operations used by the current UI and screen flow.

## Current contents

```text
engine/graphics/
|-- Render.hpp / .cpp          <- Shared drawing boundary
|-- RenderSurface.hpp          <- RAII render texture helper
|-- ShaderManager.hpp / .cpp   <- Runtime shader compilation and cache backend
|-- Shader/                    <- Compile-time shader module descriptors
|   |-- ShaderModule.hpp       <- Convention docs + ShaderModuleConfig
|   |-- TypedShaderModule.hpp  <- Typed shader load/lookup/uniform API
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
- full-screen regular texture drawing
- render-texture-to-screen drawing
- screen width and height queries
- small RAII helpers for shader mode and texture mode pairing

Raw Raylib drawing is still acceptable inside this utility layer and tightly scoped low-level rendering helpers. Outside this boundary, prefer `Renderer` when a helper already exists.

## ShaderManager

`ShaderManager` owns Raylib shader resources. Public screen/render code should go through typed shader modules where a shader has a module tag.

Rules:

- compile production screen shaders during loading-screen startup
- each shader module owns its typed shader asset tag and uniform tags
- use `engine::runtime::typed::Shaders::ensure<TShader>()`, `load<TShader>()`, and `get<TShader>()` for typed shaders
- use `engine::runtime::typed::Shaders::loc<TShader, TUniform>()` and `set<TShader, TUniform>()` for typed uniform lookup and updates
- direct `ShaderManager` calls are backend/fallback code only

## Components

Runtime shader components own per-frame animation state and apply uniforms through a polymorphic `ComponentModule` interface. Unlike `Shader/` modules, components carry live behavior and state.

See [Components/README.md](./Components/README.md) for architecture and extension guidance.

## Shader source pipeline

- authored GLSL lives in `assets/shaders/`
- embedded screen shaders use `cmake/EmbedShaders.cmake` to generate `build/generated/ShaderSources.hpp`
- module headers in `engine/graphics/shaders/` reference those generated embedded sources
- file-based model shaders can also live in `assets/shaders/` and be paired at runtime through `ModelSystem`

Current typed shader modules include blur passes, the crossfade shader, the loading prelude, the main-menu background shader, the menu option shader, and the file-backed menu hands shader.
