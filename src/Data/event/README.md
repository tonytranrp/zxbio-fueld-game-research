# Event System

This folder contains the game's event system built on top of **entt** dispatcher.

## Architecture

```
Data/event/
├── EventManager.hpp      ← Central manager (init, shutdown, get bus)
├── input/
│   └── InputEvents.hpp   ← Keyboard input events
├── mouse/
│   └── MouseEvents.hpp   ← Mouse input events
├── screen/
│   └── ScreenEvents.hpp  ← Screen/resize events
└── window/
    └── WindowEvents.hpp  ← Window lifecycle events
```

## Rules

1. **One event type per folder** — each domain gets its own subfolder
2. **Only `.hpp` files** — events are pure data structs, no `.cpp` needed
3. **Hooked via `Data.hpp`** — the main bridge file that exposes events to the rest of the engine
4. **Manager pattern** — `EventManager` owns the bus, handles init/shutdown

## How to add a new event

1. Create a new folder under `Data/event/` (e.g. `Data/event/custom/`)
2. Add `CustomEvents.hpp` with your event struct(s)
3. Include it in `Data.hpp` hook
4. Emit from anywhere: `eventBus.emit<CustomEvent>(args...)`
5. Listen from anywhere: `eventBus.connect<CustomEvent, &Listener::onCustom>(listener)`

## Example

```cpp
// In Data/event/input/InputEvents.hpp
struct KeyPressedEvent {
    int key;
    bool ctrl;
    bool shift;
};

// Emitting
biofuel::Data::eventBus().emit<KeyPressedEvent>(KEY_SPACE, false, false);

// Listening
class PlayerController {
    void onKeyPressed(const KeyPressedEvent& e) { ... }
};

player.connect<input::KeyPressedEvent, &PlayerController::onKeyPressed>(*this);
```
