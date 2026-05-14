# LoadingScreen

Animated loading bar displayed during engine initialization. It processes a weighted task queue one task per frame and can poll async-safe tasks without blocking the render loop. Raylib resource work remains on the main/render thread.

## Current contents

```text
game/screens/loading/
|-- LoadingScreen.hpp
|-- LoadingScreen.cpp
`-- README.md
```

## LoadingScreen

The screen lifecycle:

1. **`onEnter()`** - Resets loading progress, configures the `ScreenBackdropController` with the loading prelude shader, and rebuilds the dynamic initialization queue from startup systems plus registered preload assets.
2. **`onUpdate()`** - Processes or polls one `LoadingTask` per frame through `TaskManager`, smoothly lerps the display progress toward actual progress, and auto-transitions to `MainMenuScreen` with typed `queueReplace<MainMenuScreen>()` when all tasks complete and the minimum display time has elapsed.
3. **`onRender()`** - Renders the shader backdrop, dark panel, title, progress bar, current task text, skip hint, and footer.
4. **`onInput()`** - Any key or mouse click skips the remaining minimum time once tasks are finished.

## Task queue

The queue is dynamic. It always includes startup systems and shader compilation, then appends one visible preload task per registered startup model asset.

| Phase | Tasks |
|-------|-------|
| Window config | Exit key, minimum size, target FPS |
| Async-safe preflight | Filesystem/directory checks that do not call Raylib resource APIs |
| System init | Event bus, task manager, screen stack, animation system, shader system, model system |
| Shader compilation | Embedded screen shaders used by the current startup flow |
| Model preload | One task per `ModelSystem` registry entry marked `preloadOnStartup` |
| Preloading | Crossfade shader cache and render buffer allocation |

Each task reports its name and weight. Model tasks show the current asset name so the loading UI matches the real runtime asset pipeline.

## Dependencies

- `LoadingTaskQueue` from `engine/core/LoadingTask.hpp`
- `TaskManager` from `engine/tasks/TaskManager.hpp` for async-safe tasks
- `ScreenBackdropController` for the shader backdrop
- `ShaderManager` and embedded screen shader modules
- `ModelSystem` for typed model registration and preload work
- `ScreenManager` for transition to `MainMenuScreen` or the idle-video dev screen

## Coding standards

- No helper types live in this screen header; keep the class self-contained
- Use `constexpr` for timing constants and layout dimensions
- Build tasks in `buildTasks()`, called from `onEnter()`
- `buildTasks()` clears the queue with `Runtime::tasks()` first, so re-entering the screen cancels any stale active async task without cancelling unrelated engine work
- Defer navigation via `queueReplace()` to avoid re-entrancy during update
