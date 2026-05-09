# AnimationController

Event-driven animation system for the Fuel Farm game. Provides a flexible, type-safe animation framework built on top of the project's existing event bus (`entt::dispatcher`).

---

## Architecture Overview

```
AnimationController/
├── README.md                          ← This file
├── AnimationManager.hpp/.cpp          ← Singleton that owns and updates all active animations
├── screen/                            ← Screen-level visual effects (co-owners of rendering)
│   ├── ScreenBlurEffect.hpp/.cpp      ← Gaussian blur backdrop for popup screens
│   └── README.md
└── animation/
    ├── Easing.hpp                     ← All easing function definitions
    ├── Animation.hpp                  ← Core Animation<T> template with event callbacks
    ├── Animation.inl                   ← Template implementation (header-only for now)
    └── PremadeAnimations.hpp          ← Factory functions for common animation patterns
```

---

## Core Concepts

### 1. Animation<T> Template

The heart of the system. A strongly-typed animation that interpolates a value of type `T` from a start value to an end value over a given duration, using a configurable easing function.

```cpp
template<typename T>
class Animation {
    // T = the animated value type (float, Color, Vector2, etc.)
    // Must support: lerp via AnimationUtils::lerp()
};
```

**Key features:**
- **Event-driven callbacks** — `onUpdate`, `onComplete`, `onCancel` via `entt::dispatcher`
- **Configurable easing** — any function matching `float(float)` signature
- **Bidirectional** — `reverse()` flips start/end and plays backward
- **Cancellable** — `cancel()` fires `onCancel` and marks for removal
- **Auto-removal** — `isDone()` tells AnimationManager to erase the animation

### 2. Easing Functions

All easing functions take a normalized progress `t ∈ [0, 1]` and return the eased value. The system ships with ~20 standard easings (quad, cubic, quart, quint, sine, expo, circ, elastic, bounce, back).

**Usage:**
```cpp
#include "animation/Easing.hpp"

// Built-in
animation.setEasing(Easing::easeInOutCubic);

// Custom
animation.setEasing([](float t) { return t * t; });
```

### 3. AnimationManager Singleton

Owns a vector of all active animations. Each frame:
1. Calls `update(dt)` on all animations
2. Removes finished/cancelled animations
3. Dispatches completion/cancellation events to the event bus

**Usage:**
```cpp
#include "AnimationController/AnimationManager.hpp"

auto& mgr = AnimationManager::instance();
mgr.init();
mgr.update(deltaTime);
mgr.shutdown();
```

### 4. Premade Animations

Factory functions that create pre-configured animations for common patterns. These are the primary way screens should create animations.

**Available:**
| Function | Description |
|----------|-------------|
| `makeFadeIn()` | Alpha 0→255 over duration |
| `makeFadeOut()` | Alpha 255→0 over duration |
| `makeScaleIn()` | Scale 0→1 with configurable pivot |
| `makeScaleOut()` | Scale 1→0 |
| `makeSlideUp()` | Y offset from offsetY→0 |
| `makeSlideDown()` | Y offset from 0→offsetY |
| `makePulse()` | Scale oscillates between 1 and maxScale |
| `makeShake()` | Position oscillates ±intensity |
| `makeColorShift()` | Color interpolates from start to end |
| `makeFloatLerp()` | Generic float interpolation |

**Usage:**
```cpp
auto anim = PremadeAnimations::makeFadeIn(0.4f, Easing::easeOutQuad);
anim->onComplete += [](Animation<Color>* a) {
    spdlog::info("Fade complete!");
};
AnimationManager::instance().add(std::move(anim));
```

### 5. ScreenBlurEffect (Co-Owner of Rendering)

`ScreenBlurEffect` is a higher-level utility that makes the animation system a **co-owner of screen rendering**. It captures the screen behind a popup, applies a two-pass Gaussian blur shader, and draws the result as a tinted backdrop.

**Why this matters:**
- Screens no longer need to manually track overlay state or draw solid rectangles that obscure the background
- The blur effect is self-contained: it owns capture textures, shaders, fade state, and renders itself
- This is the "co-owner" pattern: AnimationController doesn't just animate values, it participates directly in the render pipeline

**Usage:**
```cpp
#include "AnimationController/screen/ScreenBlurEffect.hpp"

animation::screen::ScreenBlurEffect blur;

// In onEnter():
static constexpr animation::screen::BlurConfig CONFIG = {
    .tintColor = {15, 15, 25, 0},
    .maxTintAlpha = 120,
    .fadeInDuration = 0.3f,
    .fadeOutDuration = 0.3f,
    .blurRadius = 3.0f,
};
blur.init(screenWidth, screenHeight);
blur.startBlurIn(CONFIG);

// In onUpdate(dt):
blur.update(dt);

// In onRender():
blur.render(prevScreen);  // Captures, blurs, and draws backdrop
```

**Shaders are embedded** — no external `.fs` files needed. The GLSL source lives in `Utils/render/EmbeddedShaders.hpp` and is compiled into the binary via `LoadShaderFromMemory`.

---

## Event System Integration

Animations use the project's existing `entt::dispatcher` for all callbacks. This means screens can subscribe to animation events using standard entt patterns:

```cpp
// In a screen's onEnter()
auto& dispatcher = Data::events().dispatcher();
m_animationCompleteToken = dispatcher.sink<AnimationCompleteEvent>()
    .connect<&MainMenuScreen::onAnimationComplete>(this);

// Handler
void MainMenuScreen::onAnimationComplete(const AnimationCompleteEvent& e) {
    if (e.id == m_fadeInId) {
        spdlog::info("Fade-in finished for {}", e.name);
    }
}
```

**Event types:**
- `AnimationUpdateEvent<T>` — fired every frame during animation
- `AnimationCompleteEvent<T>` — fired when animation finishes normally
- `AnimationCancelEvent<T>` — fired when animation is cancelled
- `ScreenTransitionStartedEvent` — fired when a screen starts transitioning
- `ScreenTransitionCompletedEvent` — fired when a screen finishes transitioning
- `ScreenBlurFadeStartedEvent` — fired when blur fade begins
- `ScreenBlurFadeCompletedEvent` — fired when blur fade ends

---

## Type Support

The `Animation<T>` template works with any type `T` that has a corresponding `AnimationUtils::lerp()` specialization. Built-in support for:

| Type | Notes |
|------|-------|
| `float` | Direct linear interpolation |
| `Color` | Per-channel RGBA interpolation |
| `Vector2` | 2D vector lerp |
| `Vector3` | 3D vector lerp |
| `Rectangle` | Position + size interpolation |

**Adding a new type:**
```cpp
template<>
struct AnimationUtils::Lerp<MyType> {
    static MyType call(const MyType& a, const MyType& b, float t) {
        return MyType{
            Lerp<float>::call(a.x, b.x, t),
            Lerp<float>::call(a.y, b.y, t),
        };
    }
};
```

---

## Screen Integration

Screens use the animation system for:
1. **Screen transitions** — fade in/out when pushing/popping screens
2. **UI element animations** — menu items sliding in, panels scaling up
3. **Visual effects** — title pulse, button hover, shake on error
4. **Blur effects** — blurred backdrops behind popup screens (via `ScreenBlurEffect`)

**Pattern for screen transitions:**
```cpp
void MyScreen::onEnter() override {
    // Start with hidden state
    m_alpha = 0.0f;
    m_scale = 0.8f;

    // Create fade + scale in animation
    auto fadeAnim = PremadeAnimations::makeFadeIn(0.35f, Easing::easeOutCubic);
    auto scaleAnim = PremadeAnimations::makeScaleIn(0.35f, Easing::easeOutBack);

    fadeAnim->onUpdate += [this](Animation<f32>* a) {
        m_alpha = a->current();
    };
    scaleAnim->onUpdate += [this](Animation<f32>* a) {
        m_scale = a->current();
    };

    AnimationManager::instance().add(std::move(fadeAnim));
    AnimationManager::instance().add(std::move(scaleAnim));
}
```

**Pattern for blur effects:**
```cpp
void MyPopupScreen::onEnter() override {
    static constexpr animation::screen::BlurConfig CONFIG = {
        .tintColor = {15, 15, 25, 0},
        .maxTintAlpha = 120,
        .fadeInDuration = 0.3f,
        .fadeOutDuration = 0.3f,
        .blurRadius = 3.0f,
    };
    m_blur.init(screenWidth, screenHeight);
    m_blur.startBlurIn(CONFIG);
}

void MyPopupScreen::onUpdate(f32 dt) override {
    m_blur.update(dt);
}

void MyPopupScreen::onRender() override {
    m_blur.render(prevScreen);  // Captures, blurs, and draws backdrop
    // ... render panel on top
}
```

---

## Coding Standards

- All animation types live in `biofuel::animation` namespace
- `AnimationManager` lives in `biofuel::animation` namespace
- Easing functions are in `biofuel::animation::Easing` namespace
- Screen effects live in `biofuel::animation::screen` namespace
- Use `f32` for all timing values (seconds)
- Use `u8` for alpha/Color channels
- Animations are **moved** into `AnimationManager` (unique_ptr)
- Never call `delete` on animation pointers — let `AnimationManager` handle lifetime
- All callbacks are lambdas or `entt::delegate` — no virtual function overrides needed
