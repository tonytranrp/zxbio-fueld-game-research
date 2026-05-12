# engine/window

Platform-specific window helpers extracted from application code live here.

## Current contents

```text
engine/window/
|-- DragHandler.hpp
|-- WindowServiceModule.hpp
`-- README.md
```

## DragHandler

`DragHandler` encapsulates the Windows client-area drag workaround for a
borderless Raylib window. It lives here so `App.cpp` stays lean and portable.

## How to use it

```cpp
#ifdef _WIN32
biofuel::engine::window::DragHandler::install(GetWindowHandle());
biofuel::engine::window::DragHandler::flush();
biofuel::engine::window::DragHandler::uninstall();
#endif
```

## Coding standards

- Keep all platform-specific code behind `#ifdef` guards.
- No heap allocation for simple window state.
- Comment platform API usage so non-Windows developers can follow it.
- Register shared window service access through `WindowServiceModule.hpp`.
