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

When New Game or Continue is activated, the UI dismisses and the main-menu shader/camera
transition (turn right, sweep left, speed up, shift from warm reds to a cosmic palette) plays
once and then holds its final state forever — `m_dimensionShift` clamps at `1.0` and the screen
never transitions away. There is currently no screen after this; the warped backdrop is the
end state while gameplay is being rebuilt around imported 3D models instead of the old voxel
world (see `Research/plans/07-visual-direction-and-3d-models.md`).

## Dependencies

- `Screen` / `ScreenManager` for lifecycle and navigation
- `ScreenBackdropController` for the shader backdrop
- `IdleTrigger` and `IdleScreen` for idle video behavior
- `MenuHelper` for horizontal carousel rendering and input
- `Easing` for intro, dismiss, menu-slide, and camera easing

## Coding standards

- Keep menu options and layout state in typed local structs.
- Put reusable widgets in `game/presentation/widgets/`.
- Put shader module definitions in `engine/graphics/shaders/`.
- Keep the transition shader-only; do not load runtime model resources directly in this screen.
- Use queued screen operations for transitions started during update.
