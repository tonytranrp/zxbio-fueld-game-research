# AnimationController/screen

Screen-level visual helpers that participate directly in rendering.

## Current contents

```text
AnimationController/screen/
|-- MenuTransitionHands.hpp
|-- MenuTransitionHands.cpp
|-- ScreenBlurEffect.hpp
|-- ScreenBlurEffect.cpp
`-- README.md
```

## MenuTransitionHands

`MenuTransitionHands` is a screen-facing effect helper for the main-menu transition.

It does not own raw model loading. Instead it:

1. requests a typed model instance from `ModelSystem`,
2. drives portal timing and dimension-shift choreography,
3. aligns the hand presentation camera, and
4. applies effect-specific shader uniforms before drawing.

Asset lifetime, shader pairing, and model registration live below it in `Systems/Model`.

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
- the screen stack so the popup can pass the previous screen into `render()`

Keep this folder focused on screen-facing visual effects, not general animation math or raw asset lifetime.
