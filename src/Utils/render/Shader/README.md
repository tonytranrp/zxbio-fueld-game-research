# Utils/render/Shader — Shader Module System

Convention-based, zero-overhead shader definitions. Each shader module is a standalone class with `constexpr` metadata and GLSL source baked into the binary. No virtual dispatch, no inheritance, no runtime registration magic.

---

## Architecture

```
Utils/render/Shader/
├── ShaderModule.hpp   ← ShaderModuleConfig struct + convention docs
├── BlurHModule.hpp    ← Horizontal Gaussian blur (9-tap)
├── BlurVModule.hpp    ← Vertical Gaussian blur (9-tap)
└── README.md          ← This file
```

Every module header includes **only** `ShaderModule.hpp` (which pulls in `Core/Types.hpp` and `<string_view>`). No raylib headers, no `ShaderManager.hpp`. This keeps compile times fast and dependencies minimal.

---

## Overview

The shader module system replaces the old approach of putting all GLSL source strings in a single `EmbeddedShaders.hpp` file. Instead, each shader lives in its own module header with its metadata, uniform names, and GLSL source all in one place.

**Why convention over inheritance?** Shaders run on the hot render path, potentially multiple times per frame. Virtual dispatch (vtable indirection) adds overhead and prevents inlining. A convention-based approach gives us:

- Zero per-frame overhead (all data is `constexpr`)
- Fast compiles (module headers include almost nothing)
- Easy auditing (every module follows the same shape)
- No hidden registration (explicit `loadFromMemory()` calls in `App::init()`)

---

## Creating a New Shader Module

Follow these steps to add a new shader module. This example creates a hypothetical vignette shader.

### Step 1: Create the module header

Create a new `.hpp` file in `src/Utils/render/Shader/`. Name it after the effect: `VignetteModule.hpp`.

```cpp
// VignetteModule.hpp
#pragma once

#include "Utils/render/Shader/ShaderModule.hpp"

namespace biofuel::utils::render::shader {

class VignetteModule {
public:
    static constexpr std::string_view NAME = "vignette";

    static constexpr std::string_view FRAGMENT_SOURCE = R"(#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float vignetteRadius;
uniform float vignetteSoftness;

out vec4 finalColor;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    float dist = distance(fragTexCoord, vec2(0.5, 0.5));
    float vignette = smoothstep(vignetteRadius, vignetteRadius - vignetteSoftness, dist);
    finalColor = mix(vec4(0.0, 0.0, 0.0, 1.0), texelColor, vignette) * colDiffuse;
}
)";

    static constexpr const char* VERTEX_SOURCE = nullptr;

    static constexpr ShaderModuleConfig CONFIG{
        .name           = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource   = VERTEX_SOURCE,
    };

    // Uniform name constants — use these instead of raw strings
    static constexpr std::string_view UNIFORM_RADIUS   = "vignetteRadius";
    static constexpr std::string_view UNIFORM_SOFTNESS = "vignetteSoftness";
};

} // namespace biofuel::utils::render::shader
```

### Step 2: Register in App::init()

Open `App::init()` and add a `loadFromMemory()` call for the new module:

```cpp
#include "Utils/render/Shader/VignetteModule.hpp"

// In App::init(), alongside existing shader registrations:
auto& shaders = utils::render::ShaderManager::instance();
shaders.loadFromMemory(
    VignetteModule::CONFIG.name.data(),
    VignetteModule::CONFIG.vertexSource,
    VignetteModule::CONFIG.fragmentSource.data()
);
```

### Step 3: Use in consumer code

Reference the module's constants when setting uniforms:

```cpp
#include "Utils/render/Shader/VignetteModule.hpp"

Shader vignette = utils::render::ShaderManager::instance().get(VignetteModule::NAME.data());

i32 radiusLoc = utils::render::ShaderManager::getLocation(vignette, VignetteModule::UNIFORM_RADIUS.data());
f32 radius = 0.7f;
utils::render::ShaderManager::setValue(vignette, radiusLoc, &radius, SHADER_UNIFORM_FLOAT);
```

That is the full lifecycle. No factory, no macro, no auto-registration. Three steps: create the header, register it, use it.

---

## Using a Shader Module

Consumer code (like `ScreenBlurEffect`) references shader modules through their constants, never through raw string literals.

### Example: ScreenBlurEffect using BlurHModule and BlurVModule

```cpp
#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"

void ScreenBlurEffect::init(i32 width, i32 height) {
    auto& shaders = utils::render::ShaderManager::instance();

    // Load both blur passes
    shaders.loadFromMemory(
        BlurHModule::CONFIG.name.data(),
        BlurHModule::CONFIG.vertexSource,
        BlurHModule::CONFIG.fragmentSource.data()
    );
    shaders.loadFromMemory(
        BlurVModule::CONFIG.name.data(),
        BlurVModule::CONFIG.vertexSource,
        BlurVModule::CONFIG.fragmentSource.data()
    );

    // Get shader handles
    m_blurH = shaders.get(BlurHModule::NAME.data());
    m_blurV = shaders.get(BlurVModule::NAME.data());

    // Cache uniform locations using module constants
    m_blurHTexelLoc = shaders.getLocation(m_blurH, BlurHModule::UNIFORM_TEXEL_SIZE.data());
    m_blurHRadiusLoc = shaders.getLocation(m_blurH, BlurHModule::UNIFORM_BLUR_RADIUS.data());
    m_blurVTexelLoc = shaders.getLocation(m_blurV, BlurVModule::UNIFORM_TEXEL_SIZE.data());
    m_blurVRadiusLoc = shaders.getLocation(m_blurV, BlurVModule::UNIFORM_BLUR_RADIUS.data());
}
```

### Key rules for consumers

1. **Use module constants, not raw strings.** Write `BlurHModule::NAME` instead of `"blur_h"`. Write `BlurHModule::UNIFORM_TEXEL_SIZE` instead of `"texelSize"`. This prevents typos and keeps names in sync.

2. **Include the module header, not EmbeddedShaders.hpp.** Module headers are the single source of truth for their shader. The old `EmbeddedShaders.hpp` is being phased out in favor of per-module headers.

3. **Call `.data()` on `string_view` when passing to C APIs.** Raylib functions expect `const char*`. `std::string_view::data()` returns exactly that.

---

## Initializing Shader Modules

All shader modules are registered in `App::init()`. This is the only place where `loadFromMemory()` should be called for production shaders.

### Current registration code

```cpp
#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"

void App::init() {
    auto& shaders = utils::render::ShaderManager::instance();
    shaders.init();

    // Shader modules — explicit registration, no auto-registration
    shaders.loadFromMemory(
        BlurHModule::CONFIG.name.data(),
        BlurHModule::CONFIG.vertexSource,
        BlurHModule::CONFIG.fragmentSource.data()
    );
    shaders.loadFromMemory(
        BlurVModule::CONFIG.name.data(),
        BlurVModule::CONFIG.vertexSource,
        BlurVModule::CONFIG.fragmentSource.data()
    );
}
```

### Adding a new module to the registration block

When you create a new module, add its `loadFromMemory()` call right after the existing ones. Keep them alphabetically sorted by module name for easy scanning:

```cpp
// Shader modules — explicit registration, no auto-registration
shaders.loadFromMemory(BlurHModule::CONFIG.name.data(),      BlurHModule::CONFIG.vertexSource,      BlurHModule::CONFIG.fragmentSource.data());
shaders.loadFromMemory(BlurVModule::CONFIG.name.data(),      BlurVModule::CONFIG.vertexSource,      BlurVModule::CONFIG.fragmentSource.data());
shaders.loadFromMemory(VignetteModule::CONFIG.name.data(),    VignetteModule::CONFIG.vertexSource,    VignetteModule::CONFIG.fragmentSource.data());
// Add new modules here, alphabetically
```

### Why explicit registration?

With 16+ shaders planned, you might wonder why not auto-register. Three reasons:

1. **Auditability.** Every shader that gets loaded is visible in one place. No hidden side effects from including a header.
2. **Compile time.** Module headers include almost nothing. Auto-registration would require pulling `ShaderManager.hpp` (and transitively raylib) into every module header, defeating the fast-compile goal.
3. **Control.** You can comment out a single `loadFromMemory()` line to disable a shader during debugging without touching the module itself.

---

## Module Convention Rules

Every shader module must follow these rules. They are enforced by convention, not by the compiler, so pay attention.

### 1. No virtual methods

Modules are plain classes with `static constexpr` members. No virtual functions, no inheritance, no abstract base class.

```cpp
// ❌ Never do this
class IShaderModule {
public:
    virtual std::string_view getName() const = 0;
    virtual ~IShaderModule() = default;
};

// ✅ Convention-only approach
class BlurHModule {
public:
    static constexpr std::string_view NAME = "blur_h";
    // ...
};
```

### 2. No raylib includes

Module headers include **only** `ShaderModule.hpp` (which includes `Core/Types.hpp` and `<string_view>`). Never include `raylib.h`, `ShaderManager.hpp`, or any other raylib header in a module file.

```cpp
// ❌ Pulls in raylib — slow compile, unnecessary dependency
#include "raylib.h"

// ✅ Only what is needed for the module's data
#include "Utils/render/Shader/ShaderModule.hpp"
```

### 3. Only Core/Types.hpp and string_view

The only types used in module headers are:

- `i32`, `f32`, `u8` from `Core/Types.hpp`
- `std::string_view` from `<string_view>`
- `ShaderModuleConfig` from `ShaderModule.hpp`

### 4. Explicit registration in App::init()

No factory, no auto-registration, no macro magic. Each module is loaded by a direct `loadFromMemory()` call in `App::init()`.

### 5. All data is constexpr

Every member in a module class is `static constexpr`. This means the data lives in read-only memory and is available at compile time. No runtime allocation.

### 6. Uniform name constants

Every uniform name used by the shader must be exposed as a `static constexpr std::string_view` constant on the module class. This prevents typos and keeps the GLSL source and the C++ consumer in sync.

```cpp
// ❌ Raw string — typo-prone, disconnected from the GLSL
i32 loc = ShaderManager::getLocation(shader, "blurRaduis"); // typo!

// ✅ Module constant — compiler-checked, single source of truth
i32 loc = ShaderManager::getLocation(shader, BlurHModule::UNIFORM_BLUR_RADIUS.data());
```

### 7. Designated initializers for CONFIG

Always use designated initializers when constructing `ShaderModuleConfig`:

```cpp
static constexpr ShaderModuleConfig CONFIG{
    .name           = NAME,
    .fragmentSource = FRAGMENT_SOURCE,
    .vertexSource   = VERTEX_SOURCE,
};
```

This makes the struct self-documenting and resistant to field reorderings.

---

## Types

| Type | Where Defined | Used For |
|------|--------------|----------|
| `i32` | `Core/Types.hpp` | Integer values (pixel coordinates, uniform locations) |
| `f32` | `Core/Types.hpp` | Floating-point values (blur radius, shader parameters) |
| `u8` | `Core/Types.hpp` | Small unsigned values (color channels) |
| `std::string_view` | `<string_view>` | Shader names, uniform names, GLSL source |
| `ShaderModuleConfig` | `ShaderModule.hpp` | Aggregated shader metadata (name, sources) |

---

## Current Modules

| Module | File | Name | Purpose |
|--------|------|------|---------|
| `BlurHModule` | `BlurHModule.hpp` | `"blur_h"` | Horizontal Gaussian blur (9-tap) |
| `BlurVModule` | `BlurVModule.hpp` | `"blur_v"` | Vertical Gaussian blur (9-tap) |