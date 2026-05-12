# engine/app

The application wrapper owns Raylib window startup, the main loop, fixed-step
update timing, service initialization order, and shutdown.

## Current contents

```text
engine/app/
|-- App.hpp
`-- App.cpp
```

## Responsibilities

- Create and close the Raylib window.
- Push the loading screen as the first screen.
- Poll input, update services, and render the active screen stack.
- Apply the fixed timestep accumulator.
- Keep platform window helpers, such as Windows borderless dragging, isolated
  behind platform guards.

## How to use it

`src/main.cpp` constructs `Application::Config` and calls `run()`.

```cpp
biofuel::engine::app::Application::Config config{
    .title = "Biofuel Game - Fuel Farm",
    .width = 1280,
    .height = 720,
    .targetFps = 60,
    .resizable = true,
};

biofuel::engine::app::Application app(config);
return app.run();
```

## Coding standards

- Keep the app layer thin. New gameplay work belongs in screens, services, or
  systems.
- All runtime services should be reached through `Runtime::services()`.
- Add init/shutdown calls in paired order.
- Optional services must be guarded by their feature macro.
- Do not render game-specific UI here except debug overlays.
