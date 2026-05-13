# engine/custom/procedural/pose

Procedural pose mapping and camera-to-stage calibration live here. This folder owns
math that turns tracked or generated input into stage-space poses.

## Current contents

```text
engine/custom/procedural/pose/
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
- `CalibrationHandPhase`
- `CalibrationWizardState`
- `CalibrationSessionProfile`
- quick per-hand calibration steps, synthesized edge/corner camera warp helpers,
  pose smoothing, visibility fitting, and separation helpers

`StageLayoutPolicy::Adaptive` keeps both hands in the shared calibrated stage
space and relies on soft separation only when palms collide. Use
`StageLayoutPolicy::FixedLanes` when a tool deliberately wants hard left/right
lanes.

Screens should consume mapped state and display calibration UI. They should not
own calibration math.

```cpp
using pose::CalibrationHandPhase;
using pose::CalibrationWizardStep;

if (wizard.activeHand == CalibrationHandPhase::Right
    && wizard.step == CalibrationWizardStep::Bottom) {
    DrawText(calibrationPrompt(wizard.activeHand, wizard.step).data(), x, y, size, color);
}
```

## Coding standards

- Use explicit coordinate-space type names: camera frame, display landmark,
  stage volume, pose bounds.
- Keep calibration state session-local unless a persistence layer is added.
- Clamp unsafe data at the boundary, but keep useful unclamped helpers for range
  mapping where intentional.
- Keep calibration sequences and target tolerances here, not in debug screens.
- Prefer short guided calibration plus engine-side adaptive refinement over long
  one-time calibration surveys.
- Do not use adaptive layout to shrink a hand into a lane; finger and palm
  geometry should be preserved by the hand retargeter.
- Preserve camera-space relationships between hands. Separate per-hand
  calibration should not make a shared gesture split apart in stage space.
- Do not include game screens or UI widgets here.
