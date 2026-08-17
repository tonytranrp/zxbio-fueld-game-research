# PausePopupScreen

Semi-transparent blurred overlay with a vertical menu. `PauseController` routes the global ESC pause request on eligible screens, then this popup slides in from the right edge and slides out to the left when dismissed.

## Current contents

```text
game/screens/pause_popup/
|-- PausePopupScreen.hpp
|-- PausePopupScreen.cpp
|-- PausePopupScreenModule.hpp
|-- PauseController.hpp
|-- PauseController.cpp
`-- README.md
```

## PausePopupScreen

The screen lifecycle:

1. **`onEnter()`** — Disables `ScreenManager` crossfade (handles all animation internally), initializes `ScreenBlurEffect` with the configured blur parameters, starts blur-in + panel slide-in animation via `AnimationManager`
2. **`onUpdate()`** — Processes deferred pop requests, decrements keyboard cooldown, updates blur effect fade state
3. **`onRender()`** — Renders blurred backdrop via `ScreenBlurEffect` (captures the screen below, applies Gaussian blur + desaturation + vignette + tint), then draws the dark panel, "PAUSED" title, separator, vertical menu, and controls hint — all offset by the current slide animation
4. **`onInput()`** — ESC starts slide-out dismiss; UP/DOWN navigates vertical menu; mouse hit-testing adjusted for panel slide position

### Animation flow

Two-phase animation via `AnimationManager`:

- **Slide in**: Panel slides from right edge (1.0 → 0.0) with `easeOutCubic`. Simultaneously, `ScreenBlurEffect` fades the blur + tint in. Input is blocked until complete.
- **Slide out**: ESC or "Quit to Desktop" triggers blur fade-out + panel continues leftward (0.0 → -1.0) with `easeOutQuad`. On complete, pops the screen (and requests quit if quitting).

### State machine

| Flag | Purpose |
|------|---------|
| `m_animatingIn` | Blocks input during entry slide |
| `m_animatingOut` | Blocks ESC during exit slide |
| `m_quitting` | If true, requests app quit after pop |
| `m_wantsPop` | Deferred pop flag set on slide-out complete |

### Global pause eligibility

`PauseController` is global, but intentionally narrow. It opens pause only for
`MainMenu` and `GamePlay`. It rejects loading, the pause popup itself,
and transient/diagnostic screens (`Idle`, `Video`) because those
screens already own ESC for dismissal or navigation.

## Dependencies

- `Screen` / `ScreenManager` for lifecycle and stack operations
- `ScreenBlurEffect` for capture → blur → composite rendering
- `AnimationManager` + `PremadeAnimations` for slide animations
- `MenuHelper` for vertical menu rendering, navigation, and hit-testing

## Coding standards

- No types live in this screen's header — the class is self-contained
- Use `constexpr` for all layout constants, `BlurConfig`, and timing values
- Deferred pop via `m_wantsPop` flag to avoid re-entrancy during update
- Disable `ScreenManager` crossfade (`setTransitionDuration(0.0f)`) — all animation is screen-owned
