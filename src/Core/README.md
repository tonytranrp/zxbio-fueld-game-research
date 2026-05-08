# Core — Application Entry Point & Types

The `Core/` directory contains the application bootstrap, game loop, and project-wide type definitions.

## Architecture

```
Core/
├── App.hpp          ← Application class (window, loop, lifecycle)
├── App.cpp          ← Application implementation
└── Types.hpp        ← Project-wide type aliases (i32, f32, u8, etc.)
```

## Coding Standards

### 1. Types — Always Use Project Aliases, Never Raw Primitives

```cpp
// ❌ BAD — raw C++ types
int width = 1280;
float deltaTime = 0.016f;
unsigned char alpha = 255;

// ✅ GOOD — project types from Core/Types.hpp
i32 width = 1280;
f32 deltaTime = 0.016f;
u8 alpha = 255;
```

**Available types:** `i8, i16, i32, i64, u8, u16, u32, u64, f32, f64`

This makes intent explicit and prevents implicit conversion bugs. Use `i32` for indices/counts, `f32` for time/positions, `u8` for 0-255 color channels.

### 2. Application Class — Single Owner of the Window

`Application` owns the Raylib window and the main loop. It:
- Creates the window in `init()`
- Calls `InputSystem::poll()` → `ScreenManager::handleInput()` each frame
- Delegates update/render to ScreenManager
- Closes the window in `shutdown()`

Never create a second `Application` instance. Never call `InitWindow()` or `CloseWindow()` outside of `App.cpp`.

### 3. Config — Named Initialization

```cpp
// ✅ Use designated initializers for clarity
Application::Config config{
    .title = "Biofuel Game - Fuel Farm",
    .width = 1280,
    .height = 720,
    .targetFps = 60,
    .resizable = true,
};
```

### 4. Quit Mechanism

Screens should **never** call `CloseWindow()` directly. Instead, call:
```cpp
manager()->requestQuit();
```

The Application loop checks `quitRequested()` and exits cleanly through `shutdown()`.

### 5. Const Correctness

- All getters must be `[[nodiscard]]` and `const noexcept` where possible
- Config members are `const` after initialization
- `dt` parameters are `const f32`

## Templates

This layer is template-free by design. The Application is a concrete class, not a template. If you need generic behavior, put it in `Utils/` — keep the Core layer simple and concrete.
