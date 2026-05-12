# MainMenu

Title screen with a raymarched shader backdrop, staged intro animation, horizontal carousel menu, and a cascading dismiss that hands off into the rigged hand / dimension-shift sequence.

## Current contents

```text
game/screens/main_menu/
|-- MainMenuTypes.hpp
|-- MainMenuScreen.hpp
|-- MainMenuScreen.cpp
`-- README.md
```

## Types (`MainMenuTypes.hpp`)

All animation state types live in a separate header to keep the screen class lean:

- **`IntroPhase`** — Enum controlling the intro fade-in sequence (`WaitingForTransition → TitleFade → SubtitleFade → HintsFade → MenuFade → Done`)
- **`TextFade`** — Fade-in state for a single text element (delay, duration, elapsed, eased alpha)
- **`UIDismissState`** — Cascading dismiss animation with staggered element indices (`ELEM_MENU`, `ELEM_TITLE`, `ELEM_HINTS`, `ELEM_FOOTER`) and configurable stagger delay / element duration
- **`MenuSlideState`** — Horizontal carousel slide animation state (direction, elapsed, eased motion)

## MainMenuScreen

The screen lifecycle:

1. **`onEnter()`** — Resets all state, configures the `ScreenBackdropController` with the main menu voxel raymarch shader, starts waiting for crossfade to complete
2. **`onUpdate()`** — Advances backdrop time, menu slide, and dismiss animation; kicks off staged intro when the background reveal crosses the sync threshold
3. **`onRender()`** — Renders shader backdrop, the `MenuTransitionHands` effect when dismissing, then conditionally draws title block, subtitle, hints, horizontal carousel menu, and footer — each respecting intro phase, dismiss progress, and fade multipliers
4. **`onInput()`** — ESC opens pause popup; LEFT/RIGHT navigates the horizontal carousel; ENTER or mouse click activates the selected item

### Dismiss animation

When "New Game" or "Continue" is activated, a staggered cascade dismisses all UI:

| Order | Element | Direction | Easing |
|-------|---------|-----------|--------|
| 1st | Menu carousel | Slides DOWN | `easeInCubic` |
| 2nd | Title + Subtitle | Slides LEFT | `easeInCubic` |
| 3rd | Hints text | Slides LEFT | `easeInCubic` |
| 4th | Footer | Slides DOWN + fades | `easeInCubic` |

Each element staggers by `0.06s`. Each takes `0.40s` to complete. Input is blocked during the dismiss. Elements are skipped entirely once progress ≥ 1.0.

### Idle transition

After 30s of inactivity, `IdleTrigger` fires and the screen pushes `IdleScreen`:

1. `m_idleTransitionActive` guards prevent dimension-shift updates during the push
2. Hands are dismissed via `startIdleDismiss()` — no 3D model, just a quick fade
3. On resume (`onResume()`), dimension shift, camera, and phase state are all reset

Idle music is preloaded in `onEnter()` via `IdleScreen::preloadAssets()`.

## Dependencies

- `Screen` / `ScreenManager` for lifecycle and navigation
- `ScreenBackdropController` for the shader backdrop with reveal/brightness
- `MenuTransitionHands` for the rigged hand transition sequence
- `IdleTrigger` for inactivity detection
- `IdleScreen` for the idle overlay (pushed on trigger, preloaded via `preloadAssets()`)
- `MenuHelper` for horizontal carousel rendering, navigation, and hit-testing
- `Easing` for intro/dismiss/menu-slide easing functions

## Coding standards

- Keep animation state types in `MainMenuTypes.hpp`, not inside the screen class
- Use `constexpr` for all layout constants and color palettes
- Respect intro phase guards in render — don't draw elements before their fade-in stage
- Skip dismissed elements when progress ≥ 1.0 to avoid unnecessary draw calls
- Block input during both intro and dismiss animations
