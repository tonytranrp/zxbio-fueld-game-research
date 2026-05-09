# Utils/render — Rendering Wrapper

Thin C++ wrapper around Raylib draw calls. Provides `std::string` support, centering helpers, shader management, and embedded GLSL shaders.

## Architecture

```
Utils/render/
├── Render.hpp
├── Render.cpp
├── ShaderManager.hpp      ← Singleton: load, cache, and provide Shader objects
├── ShaderManager.cpp
├── EmbeddedShaders.hpp    ← Backward-compatible aliases (delegates to modules)
├── Shader/                ← Shader Module system
│   ├── ShaderModule.hpp   ← Base config struct + convention docs
│   ├── BlurHModule.hpp    ← Horizontal blur module
│   ├── BlurVModule.hpp    ← Vertical blur module
│   └── README.md          ← Module creation guide
└── README.md              ← This file
```

## Renderer

 Stateless static class wrapping Raylib draw functions. All coordinates use project types (`i32`, `f32`).

### Coding Standards

#### 1. All Drawing Goes Through Renderer

Never call Raylib `DrawText()`, `DrawRectangle()`, etc. directly from game code:

```cpp
// ❌ Bypassing the wrapper
DrawText("Hello", 10, 10, 20, WHITE);

// ✅ Using Renderer
Renderer::drawText("Hello", 10, 10, 20, WHITE);
```

The Renderer wraps Raylib so we can add batching, screen-space transforms, or swap backends later without touching game code.

#### 2. Project Types Only

```cpp
// ❌ Raw C++ types
static void drawRect(int x, int y, int width, int height, Color color);

// ✅ Project types from Core/Types.hpp
static void drawRect(i32 x, i32 y, i32 width, i32 height, Color color);
```

#### 3. std::string Parameters — Accept by const&

```cpp
static void drawText(const std::string& text, i32 x, i32 y, i32 fontSize, Color color);
```

Callers with `std::string_view` or `const char*` should construct a temporary:
```cpp
Renderer::drawText(std::string{myStringView}, x, y, size, color);
```

#### 4. beginFrame / endFrame — Always Paired

Every frame must call `beginFrame()` before any drawing and `endFrame()` after. This is handled by `App::render()` — screens just implement `onRender()`.

#### 5. screenWidth() / screenHeight() — Use Instead of Raylib Directly

```cpp
// ✅
const i32 sw = Renderer::screenWidth();

// ❌
const int sw = GetScreenWidth();
```

#### 6. No State in Renderer

Renderer is a **stateless static class**. No instance variables, no constructor. If you need render state (current camera, transform stack, etc.), create a separate `RenderState` struct.

## ShaderManager

`ShaderManager` is a singleton that loads, caches, and provides Raylib `Shader` objects by name. It wraps `LoadShader` / `UnloadShader` and tracks loaded shaders in a map.

### Loading Shaders

Shaders can be loaded from **embedded source strings** (compiled into the binary) or from files.

**Preferred: embedded (no external files needed)**
```cpp
#include "Utils/render/ShaderManager.hpp"
#include "Utils/render/EmbeddedShaders.hpp"

// In App::init()
auto& shaders = utils::render::ShaderManager::instance();
shaders.init();

// Compile shaders from memory — source is in EmbeddedShaders.hpp
using namespace utils::render::embedded;
shaders.loadFromMemory("blur_h", nullptr, BLUR_H_FS.data());
shaders.loadFromMemory("blur_v", nullptr, BLUR_V_FS.data());
```

**Legacy: from files**
```cpp
// Only use this if you need hot-reload during development
shaders.load("blur_h", "", "assets/shaders/blur_h.fs");
```

### Using Shaders

```cpp
Shader blur = utils::render::ShaderManager::instance().get("blur_h");

// Get uniform location
i32 loc = utils::render::ShaderManager::getLocation(blur, "myUniform");

// Set uniform value
f32 radius = 3.0f;
utils::render::ShaderManager::setValue(blur, loc, &radius, SHADER_UNIFORM_FLOAT);

// Draw with shader
BeginShaderMode(blur);
    Renderer::drawRect(0, 0, sw, sh, WHITE);
EndShaderMode();
```

### Rules

- **Load once, reuse everywhere** — never call `LoadShader()` directly; always go through `ShaderManager`
- **Load in `App::init()`** — shader compilation is slow; do it at startup
- **Unload in `App::shutdown()`** — call `ShaderManager::instance().shutdown()` to free GPU resources
- **Check validity** — `IsShaderValid(shader)` before using; `ShaderManager::get()` returns an invalid shader if the name isn't found

## EmbeddedShaders

`EmbeddedShaders.hpp` contains GLSL shader source code as `constexpr std::string_view` raw string literals. These are compiled into the binary at build time — no external `.fs` files are needed at runtime.

### Why Embedded?

- The executable is **self-contained** — no `assets/shaders/` directory to distribute
- Shaders are compiled into GPU memory at startup via `LoadShaderFromMemory()`
- No file I/O at runtime — faster loading, no missing file errors

### Current Shaders

| Name | Variable | Purpose |
|------|----------|---------|
| `blur_h` | `BLUR_H_FS` | Horizontal Gaussian blur (9-tap) |
| `blur_v` | `BLUR_V_FS` | Vertical Gaussian blur (9-tap) |

### Modifying Shaders

Edit the raw string literal in `EmbeddedShaders.hpp`, then rebuild:
```bash
cmake --build build --config Release
```

The shader source is baked into the `.exe` at compile time.

### Adding a New Shader

New shaders should be created as **shader modules** in the `Shader/` subfolder, not as raw string literals in `EmbeddedShaders.hpp`. See the [Shader Modules](#shader-modules) section and `Shader/README.md` for the full guide.

Quick steps:

1. Create a new module header in `Shader/` (e.g. `Shader/MyEffectModule.hpp`) following the convention in `ShaderModule.hpp`
2. Add a backward-compatible alias in `EmbeddedShaders.hpp` if existing code needs it
3. Register the module in `App::init()` via `ShaderManager::instance().loadFromMemory()`
4. Use it in render code via `ShaderManager::instance().get()`

```cpp
// In Shader/MyEffectModule.hpp:
namespace biofuel::utils::render::shader {
class MyEffectModule {
public:
    static constexpr std::string_view NAME = "my_effect";
    static constexpr std::string_view FRAGMENT_SOURCE = R"(#version 330
        // ... GLSL code ...
    )";
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };
};
} // namespace biofuel::utils::render::shader

// In App::init():
shaders.loadFromMemory(
    MyEffectModule::CONFIG.name.data(),
    MyEffectModule::CONFIG.vertexSource,
    MyEffectModule::CONFIG.fragmentSource.data()
);
```

## Shader Modules

The `Shader/` subfolder contains self-contained shader modules. Each module is a standalone class with `constexpr` config data, no inheritance, and no virtual methods.

### What They Are

A shader module bundles everything about one shader into a single header:

- **`NAME`** — the string key used to look up the shader in `ShaderManager`
- **`FRAGMENT_SOURCE`** — the GLSL fragment code as a `constexpr std::string_view`
- **`VERTEX_SOURCE`** — `nullptr` for Raylib's default vertex shader, or a custom vertex shader
- **`CONFIG`** — a `ShaderModuleConfig` struct aggregating the above fields
- **Uniform name constants** — e.g. `UNIFORM_TEXEL_SIZE`, `UNIFORM_BLUR_RADIUS`

All data is `constexpr`, so it lives in read-only memory. No allocations, no runtime cost.

### How They Relate to ShaderManager

Modules provide names and sources. `ShaderManager` loads them at startup:

```cpp
// In App::init()
auto& shaders = utils::render::ShaderManager::instance();
shaders.loadFromMemory(
    BlurHModule::CONFIG.name.data(),
    BlurHModule::CONFIG.vertexSource,
    BlurHModule::CONFIG.fragmentSource.data()
);
```

The module itself has no dependency on `ShaderManager` or Raylib. It only includes `Core/Types.hpp` and `<string_view>`. Registration is explicit, done by hand in `App::init()`.

### How They Relate to EmbeddedShaders

`EmbeddedShaders.hpp` now contains backward-compatible aliases that delegate to module sources:

```cpp
// EmbeddedShaders.hpp (backward compat)
inline constexpr std::string_view BLUR_H_FS = shader::BlurHModule::FRAGMENT_SOURCE;
inline constexpr std::string_view BLUR_V_FS = shader::BlurVModule::FRAGMENT_SOURCE;
```

Existing code using `BLUR_H_FS` still works. New code should use `BlurHModule::FRAGMENT_SOURCE` directly.

### Current Modules

| Module | Name | Purpose |
|--------|------|---------|
| `BlurHModule` | `blur_h` | Horizontal Gaussian blur (9-tap) |
| `BlurVModule` | `blur_v` | Vertical Gaussian blur (9-tap) |

### Adding a New Module

See `Shader/README.md` for the full module creation guide. The short version:

1. Create `Shader/YourModule.hpp` following the convention in `ShaderModule.hpp`
2. Add a backward-compatible alias in `EmbeddedShaders.hpp` if needed
3. Register it in `App::init()` via `ShaderManager::instance().loadFromMemory()`

## Types

- All coordinates and sizes: `i32` (pixel coordinates)
- Colors: Raylib `Color` struct (`{r, g, b, a}` with `u8` channels)
- Textures: Raylib `Texture2D`
- Shader handles: Raylib `Shader`

## Templates

None. This is a concrete wrapper layer. If you need generic draw helpers (e.g., `drawArray<T>`), put them in a separate utility header.
