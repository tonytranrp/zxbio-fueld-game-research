# engine/window

Platform-specific window helpers extracted from application code.

## Current contents

```text
engine/window/
|-- DragHandler.hpp
`-- README.md
```

## DragHandler

`DragHandler` encapsulates the Windows `WM_NCHITTEST` workaround that allows dragging a borderless Raylib window by its client area. It is a header-only helper that lives here so `App.cpp` stays lean and portable.

```cpp
systems::window::DragHandler m_dragHandler;
// In the window message callback:
m_dragHandler.UpdateDrag(cmd);
// In the game loop, after updates:
m_dragHandler.EndDrag();
```

### Key details

- `noexcept` throughout — safe to call from any context
- `UpdateDrag(cmd)` captures the initial click position and begins a window move
- `EndDrag()` releases the mouse capture
- Header-only, `#ifdef _WIN32` guarded — compiles to nothing on Linux/macOS
- Uses raw Win32 API calls isolated behind the guard

## Dependencies

None beyond Win32 headers (guarded). No project headers.

## Coding standards

- Keep all platform-specific code behind `#ifdef` guards
- No new/delete or heap allocation — stack-only state
- Comment the Win32 API usage so non-Windows developers understand what it does
