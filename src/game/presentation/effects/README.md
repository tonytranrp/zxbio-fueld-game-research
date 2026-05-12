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

## How to add an effect

Add an effect here when multiple game screens need the same presentation helper
and the helper is still Fuel Farm specific.

```cpp
ScreenBackdropController backdrop;
backdrop.configure(...);
backdrop.update(dt);
backdrop.render();
```

## Coding standards

- Effects may own render textures and timing state, but not screen navigation.
- Use engine graphics helpers for render surfaces and shader access.
- Keep shader module definitions in `engine/graphics/shaders/`.
- Move reusable non-game-specific effects into `engine/graphics/` or
  `engine/ui/`.
