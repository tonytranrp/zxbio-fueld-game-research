# IdleScreen

Dimmed ambient overlay that plays when the player is inactive on the main menu. Owns its own audio playback and shader dim state so the main menu screen does not carry idle logic.

## Current contents

```text
UI/screens/IdleScreen/
|-- IdleScreen.hpp
|-- IdleScreen.cpp
`-- README.md
```

## IdleScreen

The screen lifecycle:

1. **`onEnter()`** — Sets the main menu shader's `uIdleDim` uniform to 1.0 immediately (fully dimmed, no fade-in fight), starts playing the idle background music, dims the screen to black via the shader's early-out path
2. **`onUpdate()`** — No per-frame work needed; the shader handles the visual and audio plays independently
3. **`onRender()`** — Renders a full-screen black overlay as a fallback for when the shader isn't available (e.g. shader compilation during loading)
4. **`onInput()`** — Any key or mouse click triggers `sm->pop()`, returning to the main menu

### Static preload

```cpp
// Called from MainMenuScreen::onEnter() to pre-warm the idle music asset
IdleScreen::preloadAssets();
```

This loads the idle background music into `AudioManager` so it's ready when the idle screen pushes. MainMenuScreen does not need to know the music file path.

### Shader integration

IdleScreen sets `uIdleDim = 1.0` on the main menu background shader. The shader has an early-out that skips all raymarching when the uniform reaches 1.0, producing a solid black screen with zero GPU cost. No shader recompilation at runtime.

## Dependencies

- `Screen` / `ScreenManager` for lifecycle, push/pop
- `AudioManager` for background music playback
- `ShaderManager` for setting the `uIdleDim` uniform on the main menu shader
- `MenuTransitionHands` — explicitly **not** triggered during idle; uses `startIdleDismiss()` which bypasses the hand model entirely

## Coding standards

- No idle logic lives in MainMenuScreen — detection is in `IdleTrigger`, rendering/audio is here
- Use `constexpr` for the music path and fade durations
- Keep the screen self-contained — it owns its audio state
- Preload assets via the static `preloadAssets()` method so callers don't reach into private paths
