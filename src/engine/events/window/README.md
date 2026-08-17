# engine/events/window

Window lifecycle and platform display event payloads live here.

## Current contents

```text
engine/events/window/
|-- WindowEvents.hpp
`-- WindowEventModule.hpp
```

## How to use it

Publish these events when the Raylib window requests close and another system
needs to react.

```cpp
Events::publish<typed::window::CloseRequested>();
```

## Coding standards

- Keep platform-specific API details out of event payloads.
- Use project integer aliases for dimensions.
- Do not put input or screen-stack events here.
- Register new tags in `WindowEventModule.hpp`.
