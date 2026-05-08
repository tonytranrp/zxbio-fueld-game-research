# Decisions

## 2026-05-08 Planning Session
- ScreenManager: singleton via Data::screens() (matches EventManager pattern)
- Screen lifecycle: full (onEnter/onExit/onPause/onResume/onUpdate/onRender/onInput)
- Screen stacking: push/pop with overlay support (passthrough flags)
- Transitions: fade-to-black, 0.5s default, linear easing
- Game.hpp/cpp: DELETE (ScreenManager replaces its purpose)
- OnRender/OnUpdate: NOT events — direct method calls from ScreenManager
- FPS counter: stays in App::render() (debug info)
- beginFrame/endFrame: stays in App (not in screens)
- Input routing: only top screen receives input by default
- Namespace: biofuel::ui for ScreenManager/Screen, biofuel::ui::screens for concrete screens
- Fade overlay: use existing Renderer::drawRect() with alpha Color
- Transition conflict: ignore new request with spdlog warning (simpler than queuing)
