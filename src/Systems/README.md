# Systems — Game Systems Layer

Each game system lives in its own subfolder under `Systems/`. Every system is a **stateless static class** — no instances, no constructors, no state.

## Folder Convention

```
Systems/
├── Systems.hpp              ← Stub / placeholder for future forward declarations
├── README.md                ← This file
├── Input/                   ← Input polling system
│   ├── InputSystem.hpp
│   └── InputSystem.cpp
├── Render/                  ← (future) Tile/sprite renderer
├── Economy/                 ← (future) Market price simulation
├── Ecology/                 ← (future) Carbon scoring, soil health
├── Season/                  ← (future) Turn advancement, crop growth
├── TechTree/                ← (future) Tech unlock logic
└── Event/                   ← (future) Random event generation
```

**Rule:** One system per folder. Folder name is PascalCase. Namespace matches folder path.

## Namespace Convention

```
Systems/Input/   → namespace biofuel::systems::input { ... }
Systems/Render/  → namespace biofuel::systems::render { ... }
Systems/Economy/ → namespace biofuel::systems::economy { ... }
```

Never dump multiple systems into the same folder. If a folder has 5+ files, you've probably put two systems together — split them.

## Coding Standards

### 1. Stateless Static Class

Every system is a class with **only static methods**. No state, no instance:

```cpp
// Systems/Season/SeasonSystem.hpp
namespace biofuel::systems::season {

class SeasonSystem {
public:
    static void advance();
    [[nodiscard]] static i32 current();
    [[nodiscard]] static bool isWinter();
};

} // namespace biofuel::systems::season
```

Called from `App::update()` or `App::processInput()`:

```cpp
// Core/App.cpp
#include "Systems/Season/SeasonSystem.hpp"

void Application::update(const f32 dt) {
    if (shouldAdvanceTurn) {
        systems::season::SeasonSystem::advance();
    }
    Data::screens().update(dt);
}
```

### 2. One Responsibility Per System

| System | Does Exactly |
|--------|-------------|
| `Input` | Polls Raylib input, fires events |
| `Render` | Draws tiles/sprites/models |
| `Economy` | Updates market prices |
| `Ecology` | Tracks carbon, soil, water |
| `Season` | Advances turns, grows crops |
| `TechTree` | Manages research queue |
| `Event` | Generates random game events |

If a system exceeds ~200 lines, consider whether it has two responsibilities.

### 3. Event-Driven Communication

Systems **never** call each other directly. They fire events through `Data::eventBus()` and other systems/screens listen:

```cpp
// ❌ BAD — direct coupling between systems
systems::economy::EconomySystem::update(marketData);

// ✅ GOOD — fire an event, whoever cares listens
Data::eventBus().trigger(event::season::TurnAdvancedEvent{newSeason, newYear});
```

### 4. Include Discipline

```cpp
// .hpp — minimal includes, only what the interface needs
#pragma once
namespace biofuel::systems::input {
class InputSystem { ... };
}

// .cpp — all implementation includes
#include "InputSystem.hpp"
#include "Data/Data.hpp"
#include "Data/event/input/InputEvents.hpp"
#include "Data/event/mouse/MouseEvents.hpp"
```

Never include `Data.hpp` in a system header. Keep headers lightweight.

### 5. Types

- Enums and constants: use project types (`i32`, `f32`, `u8`)
- Raylib types at the boundary only (`Vector2` for mouse position — convert to project types if passing deeper)
- Never propagate `entt::entity` or `entt::dispatcher` outside the system boundary

### 6. No Dependencies on UI or Screens

Systems don't know about screens, menus, or the UI layer. Systems produce data and fire events; screens consume them. This keeps the dependency graph:

```
App → Systems → Data (events) ← UI (screens)
```

## Adding a New System

1. Create the folder: `Systems/MySystem/`
2. Create `Systems/MySystem/MySystem.hpp` with the class declaration
3. Create `Systems/MySystem/MySystem.cpp` with the implementation
4. Add `Systems/MySystem/README.md` describing what the system does (optional for now)
5. Call it from the appropriate `App::` method
6. Fire events for cross-system communication

```cpp
// Systems/MySystem/MySystem.hpp
#pragma once

namespace biofuel::systems::mysystem {

class MySystem {
public:
    static void init();
    static void update(f32 dt);
    static void shutdown();
};

} // namespace biofuel::systems::mysystem
```

## Templates

Systems don't use templates. They operate on concrete game data types. If you find yourself writing `template<typename T>` in a system, stop — you're probably trying to generalize something that should stay concrete.
