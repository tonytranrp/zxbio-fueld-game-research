# src - Game Source Code

This directory contains the authored C++ source for the game. The maintained code lives in the top-level feature folders below; generated files inside `src/build/` are not authored source.

## Current layout

```text
src/
|-- main.cpp
|-- CMakeLists.txt
|-- Core/
|-- Data/
|-- Systems/
|-- UI/
|-- AnimationController/
`-- Utils/
```

## Ownership by folder

- `Core/` - app bootstrap, main loop, project types, loading task queue
- `Data/` - event manager, event definitions, global access bridge
- `Systems/` - runtime systems; right now this is mainly input polling
- `UI/` - screen stack and concrete screens
- `AnimationController/` - generic animation runtime and screen blur effect
- `Utils/` - concrete shared helpers for rendering, fonts, and UI menus

## Repo rules that matter most

### Types

Use project aliases from `Core/Types.hpp` for numeric values in project-owned APIs and state.

```cpp
i32 width = 1280;
f32 dt = 0.016f;
u8 alpha = 255;
```

Keep raw Raylib or platform-native types at the boundary when that is the real API shape, such as key codes, `Color`, `Vector2`, `Texture2D`, or `Shader`.

### Modern C++

Prefer:

- `std::string_view` for static text and non-owning string parameters
- `std::span` for read-only lists
- `constexpr` for fixed layout and timing values
- `[[nodiscard]]` on query-style functions
- `noexcept` on accessors and small state transitions when they truly cannot throw
- small concrete RAII wrappers when they remove repeated pairing logic

Do not introduce templates by default. Use them only when they remove real duplication across multiple concrete users or match an existing generic subsystem.

### Boundaries

- gameplay and UI code should prefer `Renderer` and `ShaderManager` instead of open-coding raw Raylib draw and shader calls
- screens should use `ScreenManager` for navigation and quitting
- low-level runtime code may still call raw Raylib APIs inside utility boundaries where wrapping would be artificial

### Generated content

Do not treat `build/`, `out/`, or `src/build/` as source folders. They can exist locally, but they are generated outputs and should not drive architecture or README content.
