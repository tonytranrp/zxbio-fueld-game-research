# AnimationController

This folder contains the generic animation runtime plus screen-level visual effects that sit on top of it.

## Current layout

```text
AnimationController/
|-- AnimationManager.hpp
|-- AnimationManager.cpp
|-- animation/
|   |-- Animation.hpp
|   |-- Easing.hpp
|   `-- PremadeAnimations.hpp
`-- screen/
    |-- ScreenBlurEffect.hpp
    |-- ScreenBlurEffect.cpp
    `-- README.md
```

## Design notes

- `Animation<T>` is one of the intentional generic/template subsystems in the repo
- `AnimationManager` owns active animation instances and updates them each frame
- `PremadeAnimations` provides concrete factory helpers for common animation patterns
- `ScreenBlurEffect` is a screen-facing helper that captures, blurs, and tints the screen below a popup

## Style guidance

- use `f32` for timing and interpolation values
- keep generic behavior inside the existing animation subsystem instead of spreading templates into unrelated folders
- prefer premade animation helpers before adding one-off animation plumbing in screens

## Current users

The pause popup uses the animation runtime for panel slide-in/slide-out and uses `ScreenBlurEffect` for its blurred backdrop.
