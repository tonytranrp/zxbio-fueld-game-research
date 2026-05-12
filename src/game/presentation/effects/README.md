# game/presentation/effects

Screen-level visual helpers that participate directly in rendering.

## Current contents

```text
game/presentation/effects/
|-- ScreenBackdropController.hpp
|-- ScreenBackdropController.cpp
|-- ScreenBlurEffect.hpp
|-- ScreenBlurEffect.cpp
`-- README.md
```

## ScreenBlurEffect

`ScreenBlurEffect` captures the screen below a popup into a render texture, applies blur shader passes, draws the blurred result, and overlays a configurable tint. It owns its fade state and leases render textures through transient cache behavior.

## ScreenBackdropController

`ScreenBackdropController` owns screen-facing shader backdrop timing, reveal, brightness, and fallback color behavior.

The main-menu New Game / Continue transition no longer owns model-hand effects here; it uses shader, camera, and UI dismiss motion only. Procedural robot-hand work lives in `engine/custom/procedural/` and the Debug-only dev screen consumes that engine module.
