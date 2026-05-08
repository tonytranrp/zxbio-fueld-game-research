# src/ — Game Source Code

## Directory Structure

```
src/
├── main.cpp                  ← Entry point
├── Core/                     ← App, Types, game loop
│   └── README.md
├── Data/                     ← Event bus, event types, central bridge
│   ├── README.md
│   └── event/                ← Event structs organized by domain
│       └── README.md
├── Systems/                  ← Gameplay systems (stateless, static methods)
│   └── README.md
├── UI/                       ← Screen stack, screen base class, all screens
│   ├── README.md
│   └── screens/              ← Individual game screens (MainMenu, PausePopup, ...)
└── Utils/                    ← Reusable utilities
    ├── render/  README.md    ← Raylib draw wrappers
    ├── event/   README.md    ← EventBus wrapper
    ├── ui/      README.md    ← MenuHelper, reusable UI logic
    ├── json/    README.md    ← JSON file I/O
    ├── task/    README.md    ← Parallel task execution
    └── font/    README.md    ← Font loading/caching
```

## Global Coding Standards

These apply to **every file** in this project.

### 1. Types — Always Project Types

```cpp
// ❌ Never use raw primitive types
int x = 0;
float dt = 0.0f;
unsigned char alpha = 255;

// ✅ Always use project types from Core/Types.hpp
i32 x = 0;
f32 dt = 0.0f;
u8 alpha = 255;
```

### 2. [[nodiscard]] on All Value-Returning Functions

```cpp
[[nodiscard]] i32 screenWidth();          // Yes
[[nodiscard]] bool isRunning() const;      // Yes
void update(f32 dt);                       // No return = no [[nodiscard]]
```

### 3. constexpr for Compile-Time Constants

```cpp
static constexpr i32 TILE_SIZE = 32;
static constexpr f32 TICK_RATE = 1.0 / 60.0;
static constexpr std::string_view TITLE = "Fuel Farm";
```

### 4. noexcept on All Accessors

```cpp
[[nodiscard]] i32 getWidth() const noexcept { return m_width; }
```

### 5. std::string_view for String Literals

```cpp
// ✅ Zero-allocation compile-time string
static constexpr std::string_view LABEL = "New Game";

// Convert to std::string only at the API boundary
Renderer::drawText(std::string{LABEL}, x, y, size, color);
```

### 6. Designated Initializers for Clarity

```cpp
// ✅ Self-documenting
Config config{
    .title = "Biofuel Game",
    .width = 1280,
    .height = 720,
    .resizable = true,
};

// ❌ Positional — fragile, easy to swap fields
Config config("Biofuel Game", 1280, 720, true);
```

### 7. Named Constants — No Magic Numbers

```cpp
// ❌
Renderer::drawText("Title", 20, 30, 48, YELLOW);

// ✅
static constexpr i32 TITLE_X = 20;
static constexpr i32 TITLE_SIZE = 48;
Renderer::drawText(std::string{TITLE_STR}, TITLE_X, TITLE_Y, TITLE_SIZE, COLOR_TITLE);
```

### 8. Free Functions Over Static Methods When Possible

```cpp
// ✅ Preferred for stateless operations
namespace utils::ui {
    void renderMenu(...);
}

// ⚠️ Only if you need to group related functions with shared state
class Renderer {
public:
    static void beginFrame(...);
    static void endFrame(...);
};
```

### 9. File Organization

- **`.hpp`** — declarations: class interfaces, function signatures, constexpr data
- **`.cpp`** — definitions: function bodies, static variables, implementation details
- **One class/concern per file pair** — don't put two unrelated classes in one file
- **GLOB_RECURSE picks up everything** — just create the file, CMake finds it

### 10. Namespaces Match Directory Structure

```
src/Core/App.hpp       → namespace biofuel { class Application ... }
src/Systems/           → namespace biofuel::systems { ... }
src/UI/                → namespace biofuel::ui { ... }
src/UI/screens/        → namespace biofuel::ui::screens { ... }
src/Utils/render/      → namespace biofuel::utils::render { ... }
src/Data/              → namespace biofuel { class Data ... }
src/Data/event/input/  → namespace biofuel::event::input { ... }
```

### 11. Include Guards

Always `#pragma once` — never `#ifndef` guards.

### 12. When to Use Templates

Templates are **allowed only** when:
- You're writing a generic container/algorithm that works with 3+ types
- You're wrapping a third-party template library (Taskflow, entt)
- You need `std::span<T>` for array abstraction

Templates are **forbidden** when:
- You're avoiding a `std::string` allocation — use `std::string_view`
- You only have 1-2 concrete instantiations — just write the concrete code
- You're "future-proofing" — YAGNI

Default to concrete types. Templates add compile time, obscure error messages, and make the codebase harder to navigate.
