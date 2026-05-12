# engine/events/screen

Screen stack, window display, and screen override events live here.

## Current contents

```text
engine/events/screen/
|-- ScreenEvents.hpp
`-- ScreenEventModule.hpp
```

## How to use it

The screen manager consumes transition, layer, and debug-render override events.
Use them when tooling needs to adjust a typed screen without reaching into the
screen manager internals.

```cpp
Events::publish<typed::screen::ScreenLayerOverride>({
    .screenId = ScreenId::MainMenu,
    .layerName = "debug",
    .enabled = false,
});
```

## Coding standards

- Use `ScreenId` instead of screen-name strings when targeting a screen.
- Override events must say whether they are persistent.
- Keep navigation commands on `ScreenManager`; events are for notifications and
  override requests.
- Register new tags in `ScreenEventModule.hpp`.
