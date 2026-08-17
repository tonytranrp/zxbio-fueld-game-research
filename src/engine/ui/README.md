# engine/ui

Reusable screen stack infrastructure lives here.

## Current contents

```text
engine/ui/
|-- Screen.hpp
|-- ScreenManager.hpp/.cpp
|-- ScreenManagerOverrides.cpp
|-- ScreenManagerRendering.cpp
`-- typed/
```

## How to use it

Concrete screens derive from `Screen` and are owned by `ScreenManager`.

```cpp
class ExampleScreen final : public biofuel::engine::ui::Screen {
public:
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;
};
```

Navigate through the manager or the runtime facade:

```cpp
Runtime::screen().queueReplace<ExampleScreen>();
Runtime::screen().queuePush<ExampleModalScreen>();
```

Use queued operations from `onUpdate()` to avoid re-entrancy while the stack is
being iterated.

Modal screens can block lower-screen updates through their typed stack policy.
`ScreenManager::blocksUnderlyingUpdates()` exposes that state to the application
loop so simulation-like services can freeze while modal UI keeps rendering and
receiving input.

Game-specific global input policy belongs above this layer. A game-level
controller decides which concrete screens can open modal UI; `engine/ui` only
provides stack operations and freeze-state queries.

Architecture follow-up: `blocksUnderlyingUpdates()` currently doubles as the
application freeze bridge for pause-like modal UI. If simulation, media, or
sensor services need independent behavior, introduce an explicit pause/timescale
policy instead of adding more meaning to the UI stack policy.

## Coding standards

- Screens are non-copyable and owned by the manager.
- Use typed screen registration for production screens.
- Cross-screen reusable rendering state belongs in `game/presentation/` or
  engine UI typed render elements.
- Do not put game-specific screen classes in `engine/ui/`.
