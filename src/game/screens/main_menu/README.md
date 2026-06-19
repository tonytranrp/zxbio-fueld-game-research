# MainMenu

Title screen with a raymarched shader backdrop, staged intro animation, horizontal carousel menu, and a cascading dismiss that transitions into the shader dimension-shift sequence.

## Current contents

```text
game/screens/main_menu/
|-- MainMenuTypes.hpp
|-- MainMenuScreen.hpp
|-- MainMenuScreen.cpp
|-- MainMenuScreenModule.hpp
`-- README.md
```

## MainMenuScreen

1. `onEnter()` resets state, configures the `ScreenBackdropController`, and waits for crossfade completion.
2. `onUpdate()` advances backdrop time, menu slide, dismiss animation, staged intro, idle detection, and the shader/camera dimension shift.
3. `onRender()` renders the shader backdrop plus title, subtitle, hints, horizontal carousel menu, and footer through typed render elements.
4. `onInput()` navigates the carousel with LEFT/RIGHT and activates with ENTER or mouse click. Pause is routed globally by `PauseController`.

When New Game or Continue is activated, the UI dismisses and the main-menu shader/camera transition holds its final state, then queues `JoinScreen`. `JoinScreen` is responsible for entering the temporary `GamePlayScreen` placeholder.

## Dependencies

- `Screen` / `ScreenManager` for lifecycle and navigation
- `ScreenBackdropController` for the shader backdrop
- `JoinScreen` for the post-transition Join step
- `IdleTrigger` and `IdleScreen` for idle video behavior
- `MenuHelper` for horizontal carousel rendering and input
- `Easing` for intro, dismiss, menu-slide, and camera easing

## Coding standards

- Keep menu options and layout state in typed local structs.
- Put reusable widgets in `game/presentation/widgets/`.
- Put shader module definitions in `engine/graphics/shaders/`.
- Keep the transition shader-only; do not load runtime model resources directly in this screen.
- Use queued screen operations for transitions started during update.
