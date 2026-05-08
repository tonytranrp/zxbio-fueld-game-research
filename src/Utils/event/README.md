# Utils/event — EventBus Wrapper

A thin C++ wrapper around `entt::dispatcher` providing a simplified API for emitting and listening to game events.

## Architecture

```
Utils/event/
├── EventBus.hpp   ← EventBus class (header-only, wraps entt::dispatcher)
└── EventBus.cpp   ← Placeholder (all logic is in header)
```

## Coding Standards

### 1. Use Data::eventBus(), Not EventBus Directly

The global event bus lives in `Data/event/EventManager`. Access it through:

```cpp
auto& bus = biofuel::Data::eventBus();
bus.trigger(MyEvent{...});
```

`Utils/event/EventBus` is available if you need a local event bus (e.g., for a self-contained subsystem), but the global bus is preferred for cross-system communication.

### 2. Event Structs — Plain Data, No Logic

Events are defined in `Data/event/`. They must be:
- Simple `struct`s with public members
- No constructors (use aggregate initialization)
- No virtual methods
- No heap allocations

```cpp
// ✅ Good event
struct KeyPressedEvent {
    i32 key;
    bool ctrl;
    bool shift;
    bool alt;
};

// Emit
bus.trigger(KeyPressedEvent{KEY_SPACE, false, false, false});
```

### 3. Trigger vs Enqueue

- `dispatcher.trigger(event)` — immediate, synchronous delivery
- `dispatcher.enqueue(event)` — deferred, delivered on next `update()`

**Use `trigger()` by default.** Use `enqueue()` only when you need to defer events during a sensitive operation (e.g., mid-physics update).

### 4. Connecting to Events

```cpp
// Free function
bus.sink<KeyPressedEvent>().connect<&myHandler>();

// Member function
bus.sink<KeyPressedEvent>().connect<&MyClass::onKeyPressed>(instance);

// Lambda (via entt)
bus.sink<KeyPressedEvent>().connect<&MyClass::onKeyPressed>(*this);
```

**Always disconnect in destructors or onExit():**
```cpp
bus.sink<KeyPressedEvent>().disconnect<&MyClass::onKeyPressed>(*this);
```

### 5. Event Naming

- Event structs: `XxxYyyEvent` (e.g., `KeyPressedEvent`, `ScreenResizedEvent`)
- File names: `XxxEvents.hpp` (e.g., `InputEvents.hpp`, `MouseEvents.hpp`)
- One event category per folder under `Data/event/`

## Templates

The `EventBus` class itself is a template wrapper. When using it:

```cpp
// Connect a free function
bus.connect<MyEvent, &myFreeFunction>();

// Connect a member function
bus.connect<MyEvent, &MyClass::onMyEvent>(listener);

// Emit (constructs event in-place)
bus.emit<MyEvent>(arg1, arg2);
```

Prefer `Data::eventBus()` with direct `trigger()` calls over the template wrapper for new code — the template wrapper exists for backward compatibility.
