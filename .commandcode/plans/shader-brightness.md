# Plan: Smooth MainMenu BG Shader Intro — Shader + Screen Cooperation

## Problem

The mainmenu_bg.glsl voxel raymarcher appears at full brightness instantly during the crossfade, while text elements have a smooth fade-in sequence. The bg shader should also animate in smoothly.

## Solution

Add a `uBrightness` uniform to the GLSL shader that scales `finalColor.rgb`. Animate it 0→1 with `easeOutCubic` over the crossfade duration. Shader alpha stays 1.0 so crossfade compositing works correctly.

## Visual Timeline

```
Crossfade (0.5s)           | Fade-In (~1.0s)      | Normal
───────────────────────────|──────────────────────|────────
Bg shader: brightens 0→1   | Full brightness      | Full
Title: hidden              | Title fades in       | Full
Subtitle: hidden           | +0.2s                |
Hints: hidden              | +0.4s                |
Menu: hidden               | +0.6s                |
```

## Files Changed

| File | Change |
|------|--------|
| `assets/shaders/mainmenu_bg.glsl` | Add `uniform float uBrightness;` + `finalColor = vec4(col * uBrightness, 1.0);` |
| `src/Utils/render/Shader/MainMenuBgModule.hpp` | Add `UNIFORM_UBRIGHTNESS` constant |
| `src/UI/screens/MainMenuScreen.hpp` | Add `m_bgBrightnessLoc` member |
| `src/UI/screens/MainMenuScreen.cpp` | Lookup brightness loc in ensureBgShader; set each frame in onRender; reset in onExit |

## Detailed Changes

### 1. GLSL shader

Add `uniform float uBrightness = 0.0;` after existing uniforms.
Change `finalColor = vec4(col, 1.0);` to `finalColor = vec4(col * uBrightness, 1.0);`.

### 2. MainMenuBgModule.hpp

```cpp
static constexpr std::string_view UNIFORM_UBRIGHTNESS = "uBrightness";
```

### 3. MainMenuScreen.hpp

```cpp
i32 m_bgBrightnessLoc = -1;
```

### 4. MainMenuScreen.cpp

**ensureBgShader()** — add: `m_bgBrightnessLoc = sm.getLocation(m_bgShader, MainMenuBgModule::UNIFORM_UBRIGHTNESS.data());`

**onExit()** — add: `m_bgBrightnessLoc = -1;`

**onRender()** — before `BeginShaderMode`, compute and set brightness:
```cpp
f32 bgBrightness = 1.0f;
if (m_transitionState == TransitionState::TransitionIn) {
    bgBrightness = animation::Easing::easeOutCubic(m_transitionProgress);
}
ShaderManager::setValue(m_bgShader, m_bgBrightnessLoc, &bgBrightness, SHADER_UNIFORM_FLOAT);
```

## Edge Cases

| Case | Handling |
|------|----------|
| Uniform location = -1 (not found) | ShaderManager::setValue skips; shader uses default uBrightness=0 (dark) |
| Transition duration = 0 | easeOutCubic(1.0) = 1.0 immediately |
| Window resize | iResolution updates; brightness unaffected |

## Verification

1. Crossfade begins → bg renders dark (brightness 0)
2. During crossfade → bg brightens with easeOutCubic
3. Crossfade complete → full brightness
4. Title fade starts on bright bg
5. Text elements stagger normally
6. Voxel animation runs continuously from screen entry
