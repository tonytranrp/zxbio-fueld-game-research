# AnimationController/screen

Screen-level visual effects that integrate with the animation system.

## Architecture

```
AnimationController/screen/
├── ScreenBlurEffect.hpp    ← Gaussian blur backdrop for popup/modal screens
├── ScreenBlurEffect.cpp
└── README.md               ← This file
```

## ScreenBlurEffect

A reusable utility for popup/modal screens that need to **blur and tint** the screen behind them.

### Why It Exists

Previously, popup screens used a solid black or tinted rectangle overlay, completely obscuring the screen behind them. `ScreenBlurEffect` captures the screen below, applies a two-pass Gaussian blur shader, and draws the result as a backdrop — giving a polished "native app" frosted-glass look.

### How It Works

```
1. Capture: Render the previous screen into a RenderTexture2D
2. Blur H: Apply horizontal Gaussian blur shader → ping-pong texture
3. Blur V: Apply vertical Gaussian blur shader → screen
4. Tint: Draw semi-transparent overlay on top
```

### Usage

```cpp
#include "AnimationController/screen/ScreenBlurEffect.hpp"

class PausePopupScreen final : public Screen {
private:
    animation::screen::ScreenBlurEffect m_blurEffect;

    static constexpr animation::screen::BlurConfig BLUR_CONFIG = {
        .tintColor = {15, 15, 25, 0},
        .maxTintAlpha = 120,
        .fadeInDuration = 0.3f,
        .fadeOutDuration = 0.3f,
        .blurRadius = 3.0f,
    };

    void onEnter() override {
        const i32 sw = utils::render::Renderer::screenWidth();
        const i32 sh = utils::render::Renderer::screenHeight();
        m_blurEffect.init(sw, sh);
        m_blurEffect.startBlurIn(BLUR_CONFIG);
    }

    void onExit() override {
        m_blurEffect.shutdown();
    }

    void onUpdate(f32 dt) override {
        m_blurEffect.update(dt);
    }

    void onRender() override {
        // Pass the screen below this one in the stack
        Screen* prev = nullptr;
        if (auto* sm = manager()) {
            prev = sm->screenBelowTop();
        }
        m_blurEffect.render(prev);

        // ... draw popup panel on top ...
    }
};
```

### Shader Requirements

`ScreenBlurEffect` requires two fragment shaders compiled via `ShaderManager`:
- `"blur_h"` — horizontal Gaussian blur pass
- `"blur_v"` — vertical Gaussian blur pass

These are compiled at startup by the loading screen from GLSL source embedded in the binary:

```cpp
// In LoadingScreen::buildTasks():
shaders.loadFromMemory(
    BlurHModule::NAME.data(),
    BlurHModule::VERTEX_SOURCE,
    BlurHModule::FRAGMENT_SOURCE.data()
);
shaders.loadFromMemory(
    BlurVModule::NAME.data(),
    BlurVModule::VERTEX_SOURCE,
    BlurVModule::FRAGMENT_SOURCE.data()
);
```

By the time any popup screen uses `ScreenBlurEffect`, the shaders are already compiled and cached. Consumer code just does `shaderMgr.get()` and `shaderMgr.has()` — no compilation at that point.

No external `.fs` files are needed at runtime — the GLSL source lives in `assets/shaders/*.glsl` and is embedded at build time via CMake.

### Co-Owner Pattern

`ScreenBlurEffect` is a "co-owner" of rendering — it manages its own visual state
(capture textures, shaders, fade animation) and renders itself, rather than just
mutating a value that the screen then reads. This makes the animation system a true
participant in the render pipeline.

## Coding Standards

- Namespace: `biofuel::animation::screen`
- Types: `i32` for dimensions, `f32` for time/radius, `u8` for alpha
- `[[nodiscard]]` on all queries
- `noexcept` on all accessors and state transitions
- `constexpr` for default configs
