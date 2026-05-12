# engine/custom/procedural/physics

Procedural pose physics and camera-to-stage mapping live here. This folder owns
math that turns tracked or generated input into stage-space poses.

## Current contents

```text
engine/custom/procedural/physics/
|-- ProceduralPosePhysics.hpp
`-- TrackedPoseMapping.hpp
```

## How to use it

`TrackedPoseMapping.hpp` defines the strongly typed hand-tracking calibration
and mapping model:

- `MirrorPolicy`
- `StageLayoutPolicy`
- `CameraFrameSpace`
- `StageVolume`
- `CalibrationWizardState`
- `CalibrationSessionProfile`
- pose smoothing, visibility fitting, and separation helpers

Screens should consume mapped state and display calibration UI. They should not
own calibration math.

```cpp
using physics::CalibrationWizardStep;

if (wizard.step == CalibrationWizardStep::Near) {
    DrawText(calibrationPrompt(wizard.step).data(), x, y, size, color);
}
```

## Coding standards

- Use explicit coordinate-space type names: camera frame, display landmark,
  stage volume, pose bounds.
- Keep calibration state session-local unless a persistence layer is added.
- Clamp unsafe data at the boundary, but keep useful unclamped helpers for range
  mapping where intentional.
- Do not include game screens or UI widgets here.
