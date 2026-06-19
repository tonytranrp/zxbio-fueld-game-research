# game/app

Game entry point and bootstrap. This thin folder wires the Biofuel game into the
engine's `Application` runtime: it builds the app config, installs global input
handlers, and pushes the first screen.

## Current contents

```text
game/app/
|-- GameApp.hpp    declares makeApplication()
`-- GameApp.cpp    builds the Application config and startup hook
```

## What it does

`makeApplication()` returns a fully configured `engine::app::Application`:

- **Window config** — title "Biofuel Game - Fuel Farm", 1280×720, resizable,
  uncapped framerate (`targetFps = 0`).
- **Global input** — runs every frame: routes pause via
  `PauseController::handleGlobalInput()` and toggles the debug overlay and its
  Memory/Assets panels with F3 / F4 / F5.
- **Startup hook** — sets the screen transition/stack policy resolvers from the
  game screen catalog, then pushes `LoadingScreen`.

## How it is used

`main` (the engine entry) calls `biofuel::game::app::makeApplication()` and runs
the returned `Application`. Keep this folder a thin seam between `main` and the
engine — game-specific systems and screens are registered here but implemented
elsewhere (`game/screens`, `game/gameplay`, `game/presentation`).
