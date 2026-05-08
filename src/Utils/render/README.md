# Utils/render — Rendering Wrapper

Thin C++ wrapper around Raylib draw calls. Provides `std::string` support, centering helpers, and a consistent coordinate system.

## Architecture

```
Utils/render/
├── Render.hpp   ← Renderer class: beginFrame, endFrame, draw* helpers
└── Render.cpp   ← Implementation
```

## Coding Standards

### 1. All Drawing Goes Through Renderer

Never call Raylib `DrawText()`, `DrawRectangle()`, etc. directly from game code:

```cpp
// ❌ Bypassing the wrapper
DrawText("Hello", 10, 10, 20, WHITE);

// ✅ Using Renderer
Renderer::drawText("Hello", 10, 10, 20, WHITE);
```

The Renderer wraps Raylib so we can add batching, screen-space transforms, or swap backends later without touching game code.

### 2. std::string Parameters — Accept by const&

```cpp
static void drawText(const std::string& text, i32 x, i32 y, i32 fontSize, Color color);
```

Callers with `std::string_view` or `const char*` should construct a temporary:
```cpp
Renderer::drawText(std::string{myStringView}, x, y, size, color);
```

### 3. beginFrame / endFrame — Always Paired

Every frame must call `beginFrame()` before any drawing and `endFrame()` after. This is handled by `App::render()` — screens just implement `onRender()`.

### 4. screenWidth() / screenHeight() — Use Instead of Raylib Directly

```cpp
// ✅
const i32 sw = Renderer::screenWidth();

// ❌
const int sw = GetScreenWidth();
```

### 5. No State in Renderer

Renderer is a **stateless static class**. No instance variables, no constructor. If you need render state (current camera, transform stack, etc.), create a separate `RenderState` struct.

## Types

- All coordinates and sizes: `i32` (pixel coordinates)
- Colors: Raylib `Color` struct (`{r, g, b, a}` with `u8` channels)
- Textures: Raylib `Texture2D`

## Adding a New Draw Function

1. Add a `static void` method to `Renderer`
2. Use project types (`i32`, `f32`, etc.)
3. Forward to the Raylib function internally
4. If it needs `std::string`, convert with `.c_str()`

```cpp
static void drawCircle(i32 centerX, i32 centerY, f32 radius, Color color) {
    DrawCircle(centerX, centerY, radius, color);
}
```

## Templates

None. This is a concrete wrapper layer. If you need generic draw helpers (e.g., `drawArray<T>`), put them in a separate utility header.
