# engine/events/animation

Animation and screen-transition event payloads live here.

## Current contents

```text
engine/events/animation/
|-- AnimationEvents.hpp
`-- AnimationEventModule.hpp
```

## How to use it

Use these events for high-level visual lifecycle coordination, such as screen
transition start/end or overlay fade completion.

```cpp
Events::publish<typed::animation::ScreenTransitionStarted>({
    .screenName = screen.getName(),
    .isEntering = true,
});
```

## Coding standards

- Payloads are plain structs only.
- Keep event names specific to the visual lifecycle.
- Do not send per-frame animation values through global events unless a real
  subscriber needs them.
- Register every public event tag in `AnimationEventModule.hpp`.
