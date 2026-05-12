# engine/events/input

Input event payloads live here.

## Current contents

```text
engine/events/input/
|-- InputEvents.hpp
`-- InputEventModule.hpp
```

## How to use it

Use these events when a system needs typed input notifications outside the
current screen's direct `onInput()` handling.

```cpp
Events::publish<typed::input::KeyPressed>({.key = KEY_ENTER});
```

Most screen UI should still read input in `onInput()` through Raylib or local
helpers. Global input events are for cross-system coordination.

## Coding standards

- Payloads should stay small and raw enough to reflect the input source.
- Do not encode gameplay actions here; translate input to actions in game code.
- Register all public event tags in `InputEventModule.hpp`.
