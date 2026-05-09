# UI — Screen System & Manager

The `UI/` directory contains the screen stack, screen lifecycle, and individual game screens.

## Architecture

```
UI/
├── Screen.hpp              ← Abstract base class for all screens
├── ScreenManager.hpp       ← Singleton screen stack (push/pop/replace)
├── ScreenManager.cpp       ← ScreenManager implementation
├── UI.hpp                  ← Stub for future UI components
└── screens/
    ├── LoadingScreen.hpp   ← Startup screen with progress bar & deferred init
    ├── LoadingScreen.cpp
    ├── MainMenuScreen.hpp  ← Title screen with navigable menu
    ├── MainMenuScreen.cpp
    ├── PausePopupScreen.hpp ← ESC overlay with Resume/Quit + blur backdrop
    └── PausePopupScreen.cpp
```

## Coding Standards

### 1. Screen Lifecycle — Know the Order

ScreenManager calls lifecycle methods in this exact order:

```
push(screen):
  1. previous screen → onPause()
  2. screen → setManager(this)
  3. screen → onEnter()
  4. screen enters TransitionIn

pop():
  1. top screen enters TransitionOut
  2. onExit() called after transition completes
  3. previous screen → onResume()
```

**Never override onExit() just to reset state** — use `onEnter()` for initialization:

```cpp
// ✅ onEnter resets state every time the screen is pushed
void MainMenuScreen::onEnter() {
    m_selected = 0;
    m_cooldown = 0.0f;
}

// ❌ Don't do cleanup in onExit() unless you allocated resources
void MainMenuScreen::onExit() {}  // Just omit if empty
```

### 2. Screen Class Rules

- **`final`** — all concrete screens should be declared `final` unless you intend them to be base classes
- **No raw member exposure** — use accessors, not public fields
- **Use `manager()` to access ScreenManager** from derived screens:

```cpp
// ✅ Push a new screen
if (auto* sm = manager()) {
    sm->push(std::make_unique<PausePopupScreen>());
}

// ✅ Request app quit
if (auto* sm = manager()) {
    sm->requestQuit();
}
```

### 3. MenuHelper for Lists — No Duplicated Navigation Code

Any screen that has a vertical list of selectable items **must use MenuHelper**:

```cpp
#include "Utils/ui/MenuHelper.hpp"

// In onRender():
utils::ui::renderVerticalMenu(
    std::span{s_items},
    m_selected,
    centerX, startY,
    MENU_LAYOUT
);

// In onInput():
if (utils::ui::navigateVerticalMenu(m_selected, count, m_cooldown, dt,
        std::span{s_items}, MENU_LAYOUT)) {
    activateSelected();
}
```

### 4. Types

- All positions/sizes: `i32`
- Time/deltas: `f32`
- Counts/indices: `i32`
- Colors: Raylib `Color` struct with `u8` channels

### 5. constexpr for Constants

All layout numbers, timing values, and static data must be `constexpr`:

```cpp
static constexpr i32 TITLE_SIZE = 48;
static constexpr f32 KEY_REPEAT_DELAY = 0.12f;
static constexpr std::array<MenuItem, 3> s_items = {{ ... }};
```

## Popup Screens with Blur Backdrop

For modal/popup screens that appear over other screens (e.g., pause menu), use `ScreenBlurEffect` to blur and tint the screen behind the popup:

```cpp
#include "AnimationController/screen/ScreenBlurEffect.hpp"

class MyPopupScreen final : public Screen {
private:
    animation::screen::ScreenBlurEffect m_blurEffect;

    void onEnter() override {
        const i32 sw = utils::render::Renderer::screenWidth();
        const i32 sh = utils::render::Renderer::screenHeight();
        m_blurEffect.init(sw, sh);
        m_blurEffect.startBlurIn(BLUR_CONFIG);
    }

    void onRender() override {
        Screen* prev = manager()->screenBelowTop();
        m_blurEffect.render(prev);  // Blurs the screen behind
        // ... draw popup panel on top ...
    }
};
```

See `AnimationController/screen/README.md` for full `ScreenBlurEffect` documentation.

## Development Compile-Time Flag

For testing a specific screen without navigating to it manually, use the CMake dev flag:

```bash
cmake -DBIOFUEL_DEV_STARTUP_PAUSE_POPUP=ON build
cmake --build build --config Release
```

The loading screen still runs (shaders must compile), then transitions to `PausePopupScreen` with `MainMenuScreen` underneath for blur testing.

## Adding a New Screen

1. Create `UI/screens/MyScreen.hpp` and `.cpp`
2. Inherit from `Screen`, mark `final`
3. Override `onEnter()`, `onUpdate()`, `onRender()`, `onInput()`
4. If it has a menu list, use `MenuHelper`
5. Register constants as `static constexpr` members
6. Push it from another screen via `manager()->push(std::make_unique<MyScreen>())`

```cpp
class MyScreen final : public Screen {
public:
    void onEnter() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

private:
    static constexpr std::string_view TITLE = "My Screen";
    i32 m_state = 0;
};
```

## Templates

The UI layer does not use templates. Screens are polymorphic via virtual dispatch (`std::unique_ptr<Screen>`). If you need a reusable widget, put it in `Utils/ui/` as a free-function helper or a concrete utility class.
