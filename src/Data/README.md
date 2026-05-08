# Data — Central Data Bridge

The `Data/` directory is the **central bridge** between all engine subsystems. It exposes singletons and event types through a single header.

## Architecture

```
Data/
├── Data.hpp              ← Central bridge: events(), screens(), eventBus()
├── Data.cpp              ← (none needed — all inline)
└── event/
    ├── README.md         ← Event system architecture
    ├── EventManager.hpp  ← owns entt::dispatcher, manages lifecycle
    ├── EventManager.cpp
    ├── input/
    │   └── InputEvents.hpp   ← KeyPressed, KeyReleased, KeyRepeat
    ├── mouse/
    │   └── MouseEvents.hpp   ← MouseMoved, MousePressed, MouseReleased, MouseScrolled
    ├── screen/
    │   └── ScreenEvents.hpp  ← ScreenResized, FullscreenToggled
    └── window/
        └── WindowEvents.hpp  ← WindowFocused, WindowMinimized, WindowCloseRequested
```

## Coding Standards

### 1. Data.hpp is the Single Include

Any file that needs events, screens, or the event bus includes **only** `Data.hpp`:

```cpp
#include "Data/Data.hpp"  // Gives you eventBus(), events(), screens()
```

Never include individual event headers or `EventManager.hpp` directly — go through `Data.hpp`.

### 2. Event Structs — Aggregate Initialization Only

```cpp
// ✅ Aggregate init with designated initializers
bus.trigger(KeyPressedEvent{.key = KEY_SPACE, .ctrl = false, .shift = false, .alt = false});

// ✅ Compact aggregate
bus.trigger(KeyPressedEvent{KEY_SPACE, false, false, false});

// ❌ Don't add constructors to events
struct KeyPressedEvent {
    KeyPressedEvent(int k);  // NO — use aggregate init
};
```

### 3. One Event Category Per Folder

- `input/` — keyboard/gamepad
- `mouse/` — mouse movement/clicks/scroll
- `screen/` — resolution/fullscreen changes
- `window/` — focus/minimize/close

Each folder contains exactly one `.hpp` file. No `.cpp` needed — events are pure data.

### 4. EventManager Lifecycle

`EventManager::init()` creates the `entt::dispatcher`. `EventManager::shutdown()` destroys it. These are called by `App::init()` and `App::shutdown()` respectively.

### 5. Trigger vs Emit Naming

Use `trigger()` — it's the entt API and there's no need to wrap it:

```cpp
// ✅ Direct entt API
Data::eventBus().trigger(MyEvent{...});

// ❌ Unnecessary wrapper
Data::eventBus().emit<MyEvent>(...);
```

## Types

- Event structs: use project types (`i32`, `f32`, `u8`, `bool`)
- Raylib types at the boundary: `Vector2` for mouse position, `int` for key codes (Raylib types)
- Don't include `<raylib.h>` in event headers — forward-declare only if needed

## Templates

Event structs are **never** templates. The dispatcher (`entt::dispatcher`) is template-heavy internally, but our wrapper layer is concrete. This keeps compile times low and error messages readable.

## Adding a New Event Type

1. Create `Data/event/my_category/MyEvents.hpp`
2. Define event struct(s) with public members
3. Include in `Data.hpp`
4. Fire from systems: `Data::eventBus().trigger(MyEvent{...})`
5. Listen from screens: connect in `onEnter()`, disconnect in `onExit()`

```cpp
// Data/event/gameplay/GameplayEvents.hpp
namespace biofuel::event::gameplay {

struct CropHarvestedEvent {
    i32 tileX;
    i32 tileY;
    i32 yield;
};

} // namespace biofuel::event::gameplay
```
