# UI

The `UI/` folder owns the screen stack, screen lifecycle, and concrete screens that make up the current user-facing flow.

## Current layout

```text
UI/
|-- Screen.hpp
|-- ScreenFwd.hpp
|-- ScreenManager.hpp
|-- ScreenManager.cpp
`-- screens/
    |-- IdleScreen/
    |   |-- IdleScreen.hpp
    |   |-- IdleScreen.cpp
    |   `-- README.md
    |-- LoadingScreen/
    |   |-- LoadingScreen.hpp
    |   |-- LoadingScreen.cpp
    |   `-- README.md
    |-- MainMenu/
    |   |-- MainMenuTypes.hpp
    |   |-- MainMenuScreen.hpp
    |   |-- MainMenuScreen.cpp
    |   `-- README.md
    |-- PausePopupScreen/
    |   |-- PausePopupScreen.hpp
    |   |-- PausePopupScreen.cpp
    |   `-- README.md
    `-- VideoScreen/
        |-- VideoScreen.hpp
        |-- VideoScreen.cpp
        `-- README.md
```

## Screen flow

- App startup pushes `LoadingScreen`
- Loading screen compiles shaders and initializes runtime services
- Loading transitions to `MainMenuScreen`
- `PausePopupScreen` can be pushed on top of the menu for blur-backed pause UI
- "New Game" / "Continue" on the main menu triggers a cascading dismiss animation

## Screen rules

- Concrete screens must be `final`
- Each screen lives in its own subdirectory under `screens/` with its own `README.md`
- If a screen has helper types (enums, structs), extract them into a `*Types.hpp` file within that subdirectory
- Initialize or reset per-entry state in `onEnter()`
- Use `manager()` for push/pop/replace/quit requests
- Use deferred queue operations (`queuePush`, `queueReplace`) from inside update code when re-entrancy is a concern
- Keep screen-owned animation state local to the screen or a focused helper such as `ScreenBlurEffect`
- Screens and screen helpers should request typed model instances from `ModelSystem`; they should not call raw Raylib model load/unload APIs directly

## Current transition behavior

`ScreenManager` owns screen stack transitions and render-texture-based crossfades. Individual screens can still opt into their own animation flow, such as the pause popup slide animation and the main menu dismiss cascade.

## Adding a new screen

1. Create `src/UI/screens/YourScreen/` directory
2. If you have helper types, create `YourScreenTypes.hpp` first
3. Write `YourScreen.hpp` and `YourScreen.cpp` — inherit from `Screen`, mark `final`
4. Override `getName()` with your screen's identifier
5. Add a `README.md` documenting the screen's flow, types, dependencies, and standards
6. Add your maker function to `ScreenFwd.hpp` and define it in your `.cpp`
7. Add `.../UI/screens/YourScreen` to the include directories in `src/CMakeLists.txt`

**Screen factory (`ScreenFwd.hpp`)** provides forward-declared maker functions (`makeMainMenu()`, `makeLoading()`, `makeIdle()`, `makePausePopup()`, `makeVideoScreen()`) so screens can create each other without `#include`-ing full screen headers.
