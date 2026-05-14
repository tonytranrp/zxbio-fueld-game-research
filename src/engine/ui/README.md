# engine/ui

Reusable screen stack infrastructure lives here.

## Current contents

```text
engine/ui/
|-- Screen.hpp
|-- ScreenManager.hpp/.cpp
`-- typed/
```

## How to use it

Concrete screens derive from `Screen` and are owned by `ScreenManager`.

```cpp
class GamePlayScreen final : public biofuel::engine::ui::Screen {
public:
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;
};
```

Navigate through the manager or the runtime facade:

```cpp
Runtime::screen().queueReplace<GamePlayScreen>();
Runtime::screen().queuePush<PausePopupScreen>();
```

Use queued operations from `onUpdate()` to avoid re-entrancy while the stack is
being iterated.

Modal screens can block lower-screen updates through their typed stack policy.
`ScreenManager::blocksUnderlyingUpdates()` exposes that state to the application
loop so simulation-like services can freeze while modal UI keeps rendering and
receiving input.

## Coding standards

- Screens are non-copyable and owned by the manager.
- Use typed screen registration for production screens.
- Cross-screen reusable rendering state belongs in `game/presentation/` or
  engine UI typed render elements.
- Do not put game-specific screen classes in `engine/ui/`.
