# Plan: Loading Screen + Shader Pipeline Revamp

## Problem
- App startup has a visible delay before anything appears on screen — shader compilation (`LoadShaderFromMemory`) happens synchronously after window creation with no visual feedback
- GLSL source is embedded as raw C++ strings in headers — no syntax highlighting, no build-time validation, clunky to edit

## Solution
1. **Loading screen** appears immediately after window creation, shows animated progress bar while init runs, holds minimum 3 seconds
2. **Shader sources** move to `.glsl` files under `assets/shaders/`, embedded at build time via CMake `file(READ)` + `configure_file()`, with optional `glslc` validation

---

## Files to Create

| File | Purpose |
|------|---------|
| `src/UI/screens/LoadingScreen.hpp` | Loading screen class declaration |
| `src/UI/screens/LoadingScreen.cpp` | Loading screen implementation — progress bar, status text, task orchestration |
| `src/Core/LoadingTask.hpp` | `LoadingTask` struct + `LoadingTaskQueue` for deferred init progression |
| `assets/shaders/blur_h.glsl` | Horizontal blur GLSL extracted from BlurHModule |
| `assets/shaders/blur_v.glsl` | Vertical blur GLSL extracted from BlurVModule |
| `${CMAKE_BINARY_DIR}/generated/ShaderSources.hpp` | Build-generated header with constexpr GLSL strings |

## Files to Modify

| File | Change |
|------|--------|
| `src/Core/App.hpp` | Add deferred init task registration method |
| `src/Core/App.cpp` | Split init: push LoadingScreen immediately, move shader compile to LoadingScreen |
| `src/CMakeLists.txt` | Shader source embedding + optional glslc validation |
| `src/Utils/render/Shader/BlurHModule.hpp` | Replace inline FRAGMENT_SOURCE with reference to generated `ShaderSources.hpp` |
| `src/Utils/render/Shader/BlurVModule.hpp` | Same as above |
| `src/Utils/render/EmbeddedShaders.hpp` | Update backward-compat aliases to point to generated sources |

---

## Step-by-Step

### Step 1: Extract GLSL to `.glsl` Files

Move the GLSL string from each module into its own file under `assets/shaders/`:

```
assets/shaders/
├── blur_h.glsl    ← content from BlurHModule::FRAGMENT_SOURCE
└── blur_v.glsl    ← content from BlurVModule::FRAGMENT_SOURCE
```

### Step 2: CMake Shader Pipeline

In `src/CMakeLists.txt`, add **before** the `add_executable` call:

```cmake
# --- Shader source embedding ---
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(SHADER_SOURCES_CONTENT "#pragma once\n#include <string_view>\n\nnamespace biofuel::shader_source {\n")

foreach(SHADER_FILE IN ITEMS blur_h blur_v)
    file(READ "${SHADER_DIR}/${SHADER_FILE}.glsl" SHADER_CONTENT)
    string(APPEND SHADER_SOURCES_CONTENT
        "inline constexpr std::string_view ${SHADER_FILE}_source = R\"shader(${SHADER_CONTENT})shader\";\n\n"
    )
endforeach()

string(APPEND SHADER_SOURCES_CONTENT "} // namespace biofuel::shader_source\n")

file(WRITE "${GENERATED_DIR}/ShaderSources.tmp.hpp" "${SHADER_SOURCES_CONTENT}")
configure_file("${GENERATED_DIR}/ShaderSources.tmp.hpp" "${GENERATED_DIR}/ShaderSources.hpp" COPYONLY)
```

Then add the generated directory to include paths (after `add_executable`):
```cmake
target_include_directories(${PROJECT_NAME} PRIVATE ${GENERATED_DIR})
```

**Optional glslc validation** (non-fatal — warns if glslc isn't installed):
```cmake
find_program(GLSLC glslc)
if(GLSLC)
    foreach(SHADER IN ITEMS blur_h blur_v)
        add_custom_command(
            TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${GLSLC} --target-env=opengl -o NUL "${SHADER_DIR}/${SHADER}.glsl"
            COMMENT "Validating shader: ${SHADER}.glsl"
        )
    endforeach()
else()
    message(STATUS "glslc not found — shader validation skipped. Install Vulkan SDK or shaderc for validation.")
endif()
```

### Step 3: Update Shader Modules

**BlurHModule.hpp** — replace the inline `FRAGMENT_SOURCE` with a reference to the generated source:
```cpp
#include "ShaderSources.hpp"  // generated at build time
static constexpr std::string_view FRAGMENT_SOURCE = shader_source::blur_h_source;
```

Same for `BlurVModule.hpp` (references `shader_source::blur_v_source`).

**EmbeddedShaders.hpp** — keep backward-compat aliases (they delegate to module constants). No change needed functionally.

### Step 4: LoadingTask System

`src/Core/LoadingTask.hpp`:
- `struct LoadingTask { std::string name; f32 weight; std::function<void()> work; }`
- `class LoadingTaskQueue` — add(), processNext(), progress(), isDone(), currentName()
- `progress()` = completedWeight / totalWeight, `isDone()` = all tasks processed

### Step 5: LoadingScreen

- `src/UI/screens/LoadingScreen.hpp/cpp`
- Extends `Screen` (no input needed, uses replace() for transition)
- Builds task queue in `onEnter()`, processes one task per frame in `onUpdate()`
- Smooth progress bar animation (lerp display toward actual)
- Animated dots ("Loading." → "Loading.." → "Loading..." every 0.5s)
- After all tasks complete AND 3 seconds elapsed → `manager()->replace(MainMenuScreen)`

### Step 6: App Init Restructure

In `App::init()`:
- Keep: window creation, EventManager/ScreenManager/AnimationManager/ShaderManager init (all instant)
- **Remove**: `loadFromMemory()` calls for blur_h and blur_v
- **Change**: Push `LoadingScreen` instead of `MainMenuScreen`
- The shader compilation moves into `LoadingScreen::onEnter()` via task queue

### Step 7: No Changes to ScreenBlurEffect

`ScreenBlurEffect::init()` doesn't compile shaders — only creates RenderTextures. Its `render()` does map lookups (`has()`, `get()`), not compilation. No changes needed.

---

## Visual Layout

```
┌────────────────────────────────────────────┐
│                                            │
│             F U E L   F A R M              │  amber/gold, 40px
│                                            │
│    ┌──────────────────────────────┐        │
│    │██████████████░░░░░░░░░░░░░░░░│        │  fill: amber, outline: gray
│    └──────────────────────────────┘        │
│        Compiling shaders...                │  light gray, 16px
│              Loading...                    │  dots cycle every 0.5s
│              v0.1.0 — C++20                │  dark gray, 12px
└────────────────────────────────────────────┘
```

---

## Verification Checklist

1. Build succeeds with generated `ShaderSources.hpp`
2. If glslc installed: shaders validate at build time; if not: warning only, build proceeds
3. Window appears instantly with loading screen (<100ms)
4. Progress bar animates smoothly from 0% → 100%
5. Status text changes per task
6. Loading screen visible minimum 3 seconds
7. After tasks complete + 3s: smooth transition to MainMenuScreen via `replace()`
8. ESC from MainMenu → PausePopup with blur works (shaders already compiled)
9. `BIOFUEL_DEV_STARTUP_PAUSE_POPUP` dev flag still functional
