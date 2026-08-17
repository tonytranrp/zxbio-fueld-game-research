# game/presentation/idle

Idle detection as a reusable, screen-agnostic utility.

## Current contents

```text
game/presentation/idle/
|-- IdleTrigger.hpp
`-- README.md
```

## IdleTrigger

`IdleTrigger` is a self-contained idle timer that any screen can own. It does not decide what "idle" means for the screen — it just tracks inactivity and fires a callback exactly once when the timeout is reached. The owning screen supplies that callback (e.g. push `IdleScreen`) and decides when to re-arm it.

## How to use it

```cpp
biofuel::game::presentation::idle::IdleTrigger m_idleTrigger{5.0f};
// ...
m_idleTrigger.setCallback([this] { /* push IdleScreen, etc. */ });
// In update:   m_idleTrigger.update(dt, /*active=*/true);
// In input:    if (anyInput) m_idleTrigger.onInput();
// In onExit:   m_idleTrigger.reset();
```

### Key details

- A `noexcept`-methods class with no heap allocations of its own (aside from the `std::function` callback's small-buffer storage) — safe to place directly in screen state
- Idle timeout defaults to 5.0s, set at construction or via `setTimeout()`
- `update(dt, active)` accumulates idle time only while `active` is true, and only until the callback has fired once
- Once the timer reaches the timeout, `m_onIdle()` fires **exactly once** and the trigger latches (`hasFired()` becomes `true`) until `reset()` or `onInput()` is called
- `onInput()` resets the accumulator to zero **and** clears the fired latch, so any input re-arms the trigger
- `reset()` does the same as `onInput()` — clears both the accumulator and the fired latch
- Fully isolated from screens, models, events, and rendering

## Dependencies

None. `IdleTrigger` depends only on `engine/core/Types.hpp` for `f32`.

## Coding standards

- Keep stateless and concrete — no virtual methods, no event coupling
- Use `constexpr` for the default threshold
- `noexcept` on all methods
- Readable by any screen without including heavyweight headers
