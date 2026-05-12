# VideoScreen

Fullscreen video playback overlay. Follows the IdleScreen pattern.

## Current contents

```text
game/screens/video/
|-- VideoScreen.hpp
|-- VideoScreen.cpp
`-- README.md
```

## VideoScreen

The screen lifecycle:

1. **`onEnter()`** — Calls `VideoManager::play()` on the pre-loaded video, sets looping, and marks playback started only if `VideoManager::isPlaying()` confirms it.
2. **`onUpdate()`** — Counts down input delay even when playback fails, so fallback screens can still be dismissed.
3. **`onRender()`** — Draws the video frame texture fullscreen via `Renderer::drawFullscreenTexture()`. Falls back to solid black if playback is not active or the frame is unavailable.
4. **`onInput()`** — Space, Enter, Escape, or a mouse-button release pops the screen after the input delay.

### Configuration before push

```cpp
auto screen = std::make_unique<VideoScreen>("cutscene");
screen->setLooping(false); // override the default looping behavior for one-shot clips
screen->setSkipOnAnyInput(true);
screen->setInputDelay(0.5f);
sm->push(std::move(screen));
```

### Preloading

```cpp
// Call from MainMenuScreen::onEnter() or LoadingScreen:
VideoScreen::preloadVideo("cutscene", "assets/video/intro.mp4");
```

## Dependencies

- `Screen` / `ScreenManager` for lifecycle and navigation
- `VideoManager` for playback and frame access
- `Renderer` for `drawFullscreenTexture()` and `drawFullscreen()`

## Coding standards

- All timing constants are `constexpr`
- Constructor takes `string_view` video name
- Uses `VideoManager::hasVideo()` to guard against missing video
- Input delay prevents accidental skip on first frame
- Guard `manager()` before popping so the screen remains safe in isolated tests
