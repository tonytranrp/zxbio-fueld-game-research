# engine/graphics

Render utilities wrap the common Raylib drawing and shader operations used by
the current UI, screen flow, and model presentation.

## Current contents

```text
engine/graphics/
|-- Render.hpp/.cpp
|-- RenderServiceModule.hpp
|-- RenderSurface.hpp
|-- ShaderManager.hpp/.cpp
|-- TransientResourceCache.hpp/.cpp
|-- shaders/
`-- components/
```

## Renderer

`Renderer` is the shared boundary for common drawing tasks:

- frame begin/end helpers
- text drawing and measurement
- rectangle and sprite drawing
- full-screen fills and texture draws
- render-texture-to-screen drawing
- screen width and height queries
- small RAII helpers for shader mode and texture mode pairing

Raw Raylib drawing is still acceptable inside low-level rendering helpers.
Outside this boundary, prefer `Renderer` when a helper already exists.

## ShaderManager

`ShaderManager` owns Raylib shader resources. Public screen/render code should
go through typed shader modules where a shader has a module tag.

```cpp
auto& shaderManager = Runtime::shader();
Shaders::load<typed::shader::MenuOption>(shaderManager);
Shaders::set<typed::shader::MenuOption, typed::shader::uniform::Time>(shader, t);
```

## Components

Runtime shader components own per-frame animation state and apply uniforms
through `ComponentModule`. Unlike `shaders/` modules, components carry live
behavior and state.

See [components/README.md](./components/README.md) for architecture and
extension guidance.

## Shader source pipeline

1. Author GLSL in `assets/shaders/`.
2. Add embedded screen shader names to `src/CMakeLists.txt`.
3. Expose the shader through a module header in `engine/graphics/shaders/`.
4. Compile it during loading-screen startup.
5. Consume it through typed shader APIs.

File-backed model shaders can also live in `assets/shaders/` and be paired at
runtime through `ModelSystem`.

## Coding standards

- Keep shader resource ownership in `ShaderManager`.
- Keep render texture ownership in `RenderSurface` or transient caches.
- Prefer typed shader modules over ad hoc uniform strings.
- Do not put screen-specific layout in this folder.
