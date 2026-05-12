# game/presentation/idle

Idle detection as a reusable, screen-agnostic utility.

## Current contents

```text
game/presentation/idle/
|-- IdleTrigger.hpp
`-- README.md
```

## IdleTrigger

`IdleTrigger` is a self-contained idle timer that any screen can own. It does not trigger idle transitions itself — it only tracks inactivity and reports readiness. The owning screen decides what to do with that signal.

## How to use it

```cpp
systems::idle::IdleTrigger m_idleTrigger;
// ...
m_idleTrigger.onInput();               // reset on any input
m_idleTrigger.update(dt);              // accumulate idle time
if (m_idleTrigger.ready()) {           // check threshold
    // push IdleScreen, etc.
}
```

### Key details

- A `noexcept` struct with no heap allocations — safe to place directly in screen state
- Idle threshold defaults to 30.0s, configurable at construction or via `setThreshold()`
- `onInput()` resets the internal accumulator to zero
- `ready()` returns true when `accumulator >= threshold` and stays true until reset
- `reset()` clears the accumulator back to zero without changing the threshold
- Fully isolated from screens, models, events, and rendering

## Dependencies

None. `IdleTrigger` depends only on `engine/core/Types.hpp` for `f32`.

## Coding standards

- Keep stateless and concrete — no virtual methods, no event coupling
- Use `constexpr` for the default threshold
- `noexcept` on all methods
- Readable by any screen without including heavyweight headers
