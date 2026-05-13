# engine/custom/procedural/hand

The procedural robot-hand engine module lives here. It provides the reusable
hand rig, materials, mesh rendering, animation hooks, and live hand-tracking
retargeting used by the Debug hand lab.

## Current contents

```text
engine/custom/procedural/hand/
|-- HandTypes.hpp
|-- ProceduralHand.hpp
|-- RobotHandPreset.hpp/.cpp
|-- RobotHandMaterials.hpp
|-- RobotHandModule.hpp
|-- RobotHandRenderer.hpp
|-- TrackedRobotHand.hpp
`-- HandTrackingRetarget.hpp
```

## Responsibilities

- Typed left/right robot-hand specs.
- JSON-overridable rig dimensions and material tuning.
- Procedural texture and generated mesh consumption.
- FABRIK IK and joint-limit integration.
- Renderer-facing hand pose and material state.
- Camera-hand landmark retargeting into `TrackedRobotHandPose`.
- Per-hand, session-local calibration through the physics mapping layer.
- Compile-time landmark sets for palm metrics and stable one-hand-per-side
  selection before rendering.

## How to use it

Debug or gameplay code should configure the module, feed it mapped/tracked
poses, and render the resulting hands. It should not duplicate hand-generation
math.

```cpp
HandTrackingRetargeter mapper;
mapper.beginSession(width, height, MirrorPolicy::Selfie, StageLayoutPolicy::Adaptive);
mapper.startCalibration();

MappedTrackedHands mapped = mapper.map(frame, dt);
trackedLeft.apply(mapped.leftPose);
renderer.draw(trackedLeft, renderState);
```

## Coding standards

- Keep UI, camera orbit controls, and tool workflow in game screens.
- Keep detector IPC in `engine/vision/hand_tracking/`.
- Keep generic calibration and pose math in `engine/custom/procedural/pose/`.
- Let the retargeter own hand selection, calibration phase advancement, and
  conversion from calibrated profiles to `TrackedRobotHandPose`.
- Keep finger solving typed by `FingerId` so future per-finger limits can be
  added without changing call sites.
- Prefer typed hand-side, rig, and material structs over loose booleans/strings.
- Do not persist calibration here until a real save/profile system owns it.
