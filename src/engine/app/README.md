# engine/app

The application wrapper owns Raylib window startup, the main loop, fixed-step
update timing, service initialization order, and shutdown.

## Current contents

```text
engine/app/
|-- App.hpp
|-- App.cpp
|-- AppLifecycle.hpp
`-- AppLifecycle.cpp
```

## Responsibilities

- Create and close the Raylib window.
- Poll input, update services, and render the active screen stack (the game
  layer's `startup` callback is what actually pushes the first screen — see
  `game/app/GameApp.cpp`).
- Keep media streaming services, such as audio and video, pumping even when a
  modal/overlay screen freezes gameplay-facing updates below it.
- Apply the fixed timestep accumulator.
- Keep platform window helpers, such as Windows borderless dragging, isolated
  behind platform guards.

## How to use it

`src/main.cpp` constructs `Application::Config` and calls `run()`.

```cpp
biofuel::engine::app::Application::Config config{
    .title = "Example Game",
    .width = 1280,
    .height = 720,
    .targetFps = 60,
    .resizable = true,
    .vsync = true,
};

biofuel::engine::app::Application app(config);
return app.run();
```

`vsync` sets `FLAG_VSYNC_HINT` before the window opens, capping render to the
display's real refresh rate. `render()` has no pacing of its own — it draws
once per spin of the main loop regardless of whether a fixed-step update ran
that iteration — so without vsync (or a low `targetFps`) it redraws unchanged
frames as fast as the loop can spin, burning GPU/power for no visual benefit.

## Coding standards

- Keep the app layer thin. New gameplay work belongs in screens, services, or
  systems.
- All runtime services should be reached through `Runtime::services()`.
- Add init/shutdown calls in paired order.
- Optional services must be guarded by their feature macro.
- Do not render game-specific UI here except debug overlays.
