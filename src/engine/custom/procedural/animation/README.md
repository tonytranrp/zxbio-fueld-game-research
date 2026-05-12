# engine/custom/procedural/animation

Procedural animation helpers for generated rigs live here. These are separate
from generic `engine/animation/` tracks because they know about procedural hand
poses and rig controls.

## Current contents

```text
engine/custom/procedural/animation/
`-- HandAnimation.hpp
```

## How to use it

Use these helpers when a procedural hand needs an engine-owned pose clip, idle
motion, or scripted shape that can be reused outside a single debug screen.

```cpp
ProceduralHandPose pose{};
applyHandIdlePose(pose, elapsedSeconds);
```

The exact helper names should stay focused on procedural rigs, not UI effects.
UI fade, slide, and menu animation belongs in `engine/animation/` or the owning
screen.

## Coding standards

- Keep clips deterministic and data-oriented.
- Take explicit time/progress inputs; do not read global frame time here.
- Avoid screen, input, or camera dependencies.
- Share math types from `engine/custom/procedural/core/`.
