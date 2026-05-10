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
|   |-- ModelKeyframe.hpp
|   |-- ModelKeyframe.cpp
|   |-- MenuHandKeyframes.cpp
|   `-- PremadeAnimations.hpp
`-- screen/
    |-- ModelControllerOverlay.hpp
    |-- ModelControllerOverlay.cpp
    |-- MenuTransitionHands.hpp
    |-- MenuTransitionHands.cpp
    |-- ScreenBlurEffect.hpp
    |-- ScreenBlurEffect.cpp
    `-- README.md
```

## Design notes

- `Animation<T>` is one of the intentional generic/template subsystems in the repo
- `AnimationManager` owns active animation instances and updates them each frame
- `PremadeAnimations` provides concrete factory helpers for common animation patterns
- `ModelKeyframe` adds a second focused generic subsystem for authored rig animation tracks (`f32`, `Vector3`, `Quaternion`)
- `ScreenBlurEffect` is a screen-facing helper that captures, blurs, and tints the screen below a popup
- `MenuTransitionHands` is a screen-facing transition effect that now consumes typed model instances from `ModelSystem`
- `ModelControllerOverlay` is a dev-only runtime tuning overlay for model/control-point placement; enable it with `BIOFUEL_DEV_MODEL_CONTROLLER=ON`

## Style guidance

- use `f32` for timing and interpolation values
- keep generic behavior inside the existing animation subsystem instead of spreading templates into unrelated folders
- prefer premade animation helpers before adding one-off animation plumbing in screens
- author model clips in typed C++ first; keep the data flow stable so file-authored clips can be added later if needed

## Current users

- the pause popup uses the animation runtime for panel slide-in/slide-out and uses `ScreenBlurEffect` for its blurred backdrop
- the main menu transition uses `MenuTransitionHands` plus `ModelKeyframePlayer` for the rigged hand-and-portal presentation while model lifetime stays in `Systems/Model`
- dev builds can enable `ModelControllerOverlay` to select model control points, drag XYZ gizmo arrows, and copy runtime offsets without rewriting source files
