# engine/animation

Generic animation utilities live here. This folder is engine-owned and should
not know about concrete game screens.

## Current contents

```text
engine/animation/
|-- Animation.hpp             value animation template and callbacks
|-- AnimationManager.hpp/.cpp global animation owner
|-- Easing.hpp                easing functions
|-- ModelKeyframe.hpp/.cpp    keyframe tracks for model rigs
|-- PremadeAnimations.hpp     helper factories for common screen effects
|-- AnimationServiceModule.hpp
`-- typed/
```

## How to use it

Use `Animation<T>` for short-lived UI or state interpolation. The manager owns
active animations and advances them once per fixed update through
`Runtime::animation()`.

```cpp
auto slide = std::make_unique<Animation<f32>>(
    "pause.panel_slide",
    1.0f,
    0.0f,
    0.3f,
    Easing::easeOutCubic);

slide->onUpdate([this](auto* anim) {
    m_panelOffset = anim->current();
});

Runtime::animation().add(std::move(slide));
```

Use `ModelKeyframePlayer` when an authored or procedural model needs rig-aware
tracks, scalar channels, and transition blending.

## Coding standards

- Keep animation code value-oriented and reusable.
- Add `AnimationUtils::Lerp<T>` before animating a new value type.
- Give animations stable names from `typed/AnimationTracks.hpp` when possible.
- Do not let callbacks own screen lifetime; they may run after a screen starts
  exiting.
- Model keyframe tracks should use seconds and normalized progress explicitly.

## Adding a new animation helper

Add a factory to `PremadeAnimations.hpp` only when two or more callers need the
same pattern. One-screen animations should stay local to that screen.
