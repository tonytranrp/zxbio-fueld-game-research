# AnimationController/screen

Screen-level visual helpers that participate directly in rendering.

## Current contents

```text
AnimationController/screen/
|-- ModelControllerOverlay.hpp
|-- ModelControllerOverlay.cpp
|-- MenuTransitionHands.hpp
|-- MenuTransitionHands.cpp
|-- ScreenBlurEffect.hpp
|-- ScreenBlurEffect.cpp
`-- README.md
```

## MenuTransitionHands

`MenuTransitionHands` is a screen-facing effect helper for the main-menu transition (New Game / Continue).

**Phase system:**
```
Idle → (start) → Playing → (anim done, 2.52s) → Complete (holds final frame)
```

- `Playing` — action animation running, full 3D render
- `Complete` — animation finished, renders the final frame indefinitely until `reset()` or `unload()` is called by the owning screen's lifecycle
- No auto-deactivation — hands stay visible for the full transition duration (dismiss + dimension shift + camera sweep)

It does not own raw model loading. Instead it:

1. requests typed model instances from `ModelSystem`,
2. drives portal timing and dimension-shift choreography,
3. consumes authored keyframed rig playback from the model layer,
4. aligns the hand presentation camera, and
5. applies effect-specific shader uniforms before drawing.

Asset lifetime, shader pairing, and model registration live below it in `Systems/Model`.

When `BIOFUEL_DEV_MODEL_CONTROLLER=ON`, `MenuTransitionHands` also exposes runtime-only control targets for the root pose, camera position/target, and `left.hand` / `right.hand` clusters. These targets are edited by `ModelControllerOverlay`; the edits are for visual tuning and reset on launch.

## ModelControllerOverlay

`ModelControllerOverlay` is a development-only screen helper for tuning model placement.

It:

1. receives editable control targets from screen effects or model consumers,
2. projects them with `GetWorldToScreen()`,
3. draws visible points plus red/green/blue XYZ gizmo arrows,
4. applies runtime offsets while dragging, and
5. copies the selected offset in C++ initializer form with `C`.

For the menu hands, left-half mouse input owns the left hand-cluster control and right-half mouse input owns the right hand-cluster control. Each hand control applies a weighted forearm-chain offset instead of dragging individual finger roots, so controller edits are useful for framing without tearing the rig apart. The overlay does not load assets and does not write source files.

## ScreenBlurEffect

`ScreenBlurEffect` is the current screen helper in this folder. It:

1. captures the screen below the popup into a render texture,
2. applies horizontal and vertical blur shader passes,
3. draws the blurred result back to the screen, and
4. overlays a configurable tint.

It resizes its render textures with the window and owns its own fade state, so popup screens do not need to duplicate that logic.

## Dependencies

- `Renderer` for screen-size helpers and texture drawing
- `ShaderManager` plus blur shader modules for uniform lookup and shader access
- `ModelSystem` for typed model instances and runtime-only bone offsets used by dev controller targets
- the screen stack so the popup can pass the previous screen into `render()`

Keep this folder focused on screen-facing visual effects, not general animation math or raw asset lifetime.
