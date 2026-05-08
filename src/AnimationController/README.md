# AnimationController

Event-driven animation system for the Fuel Farm game. Provides a flexible, type-safe animation framework built on top of the project's existing event bus (`entt::dispatcher`).

---

## Architecture Overview

```
AnimationController/
├── README.md                    ← This file
├── AnimationManager.hpp/.cpp    ← Singleton that owns and updates all active animations
└── animation/
    ├── Easing.hpp               ← All easing function definitions
    ├── Animation.hpp            ← Core Animation<T> template with event callbacks
    ├── Animation.inl             ← Template implementation (header-only for now)
    └── PremadeAnimations.hpp    ← Factory functions for common animation patterns
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

**Usage:**
```cpp
auto anim = PremadeAnimations::makeFadeIn(0.4f, Easing::easeOutQuad);
anim->onComplete += [](Animation<Color>* a) {
    spdlog::info("Fade complete!");
};
AnimationManager::instance().add(std::move(anim));
```

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

---

## Coding Standards

- All animation types live in `biofuel::animation` namespace
- `AnimationManager` lives in `biofuel::animation` namespace
- Easing functions are in `biofuel::animation::Easing` namespace
- Use `f32` for all timing values (seconds)
- Use `u8` for alpha/Color channels
- Animations are **moved** into `AnimationManager` (unique_ptr)
- Never call `delete` on animation pointers — let `AnimationManager` handle lifetime
- All callbacks are lambdas or `entt::delegate` — no virtual function overrides needed