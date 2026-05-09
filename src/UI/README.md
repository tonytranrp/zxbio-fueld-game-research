# UI

The `UI/` folder owns the screen stack, screen lifecycle, and concrete screens that make up the current user-facing flow.

## Current layout

```text
UI/
|-- Screen.hpp
|-- ScreenManager.hpp
|-- ScreenManager.cpp
`-- screens/
    |-- LoadingScreen.hpp
    |-- LoadingScreen.cpp
    |-- MainMenuScreen.hpp
    |-- MainMenuScreen.cpp
    |-- PausePopupScreen.hpp
    `-- PausePopupScreen.cpp
```

## Screen flow today

- app startup pushes `LoadingScreen`
- loading screen compiles shaders and initializes runtime services
- loading transitions to `MainMenuScreen`
- `PausePopupScreen` can be pushed on top of the menu for blur-backed pause UI

## Screen rules

- concrete screens should be `final`
- initialize or reset per-entry state in `onEnter()`
- use `manager()` for push/pop/replace/quit requests
- use deferred queue operations from inside update code when re-entrancy is a concern
- keep screen-owned animation state local to the screen or a focused helper such as `ScreenBlurEffect`

## Current transition behavior

`ScreenManager` owns screen stack transitions and render-texture-based crossfades. Individual screens can still opt into their own animation flow, such as the pause popup slide animation.
