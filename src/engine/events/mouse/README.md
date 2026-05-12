# engine/events/mouse

Mouse event payloads live here.

## Current contents

```text
engine/events/mouse/
|-- MouseEvents.hpp
`-- MouseEventModule.hpp
```

## How to use it

Use mouse events for shared systems that need pointer movement, wheel, or button
state without being coupled to a specific screen.

```cpp
Events::publish<typed::mouse::MouseMoved>({
    .x = GetMouseX(),
    .y = GetMouseY(),
});
```

Screen-local widgets should usually use direct hit-testing instead.

## Coding standards

- Store screen-space coordinates explicitly.
- Keep drag/drop interpretation outside this event folder.
- Register new tags in `MouseEventModule.hpp`.
