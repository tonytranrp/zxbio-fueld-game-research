# engine/custom/procedural/hand

The procedural robot-hand engine module lives here. It provides the reusable
hand rig, materials, mesh rendering, animation hooks, and live hand-tracking
retargeting used by the Debug hand lab.

## Current contents

```text
engine/custom/procedural/hand/
|-- HandTypes.hpp
|-- HandPhysicsInteraction.hpp/.cpp
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
- Quick per-hand, session-local calibration through the pose mapping layer.
- Adaptive calibration refinement during normal tracking so edge/depth range can
  improve after the short guided pass.
- Compile-time landmark sets for palm metrics and stable one-hand-per-side
  selection before rendering.
- Adaptive pose smoothing and short dropout grace so rendered hands stay stable
  when detector handedness is ambiguous or a single recognizer frame drops.
- Palm-relative landmark scaling that preserves finger spread in one-hand and
  two-hand tracking. Adaptive two-hand mode may position both palms in the same
  calibrated stage volume; it must not squeeze fingers into half-screen lanes.
- Shared absolute camera projection for tracked landmarks, so two hands forming
  one shape keep camera-space contacts in the 3D model.
- Palm-span depth mapping for forward/back motion; moving a hand closer or
  farther from the camera should move the mapped palm through stage depth.
- Rapier-backed hand interaction helpers that sync tracked palms/fingertips to
  kinematic colliders, keep touch props in a bounded comfortable range, recover
  escaped props, and expose grab state for engine/gameplay demos.

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

For physics interactions, keep the hand pose tracking authoritative and sync
Rapier kinematic bodies from it:

```cpp
HandPhysicsInteraction3D interaction;
interaction.init(Runtime::physics().world3D());
interaction.update(Runtime::physics().world3D(), &mapped.leftPose, &mapped.rightPose, dt);
```

## Coding standards

- Keep UI, camera orbit controls, and tool workflow in game screens.
- Keep detector IPC in `engine/vision/hand_tracking/`.
- Keep generic calibration and pose math in `engine/custom/procedural/pose/`.
- Let the retargeter own hand selection, calibration phase advancement, and
  conversion from calibrated profiles to `TrackedRobotHandPose`.
- Keep short-lived tracking grace in the retargeter; detector services should
  report raw availability, while rendering pose continuity belongs here.
- Retarget palm placement and local hand shape separately. Stage layout may
  move a hand, but finger geometry should remain palm-relative so close hands
  can actually touch.
- Use the shared spatial calibration for XY landmark projection. Per-hand
  calibration may tune scale/depth, but two hands at the same camera coordinate
  must resolve to the same stage coordinate.
- Treat landmark depth as camera-through local depth, not a reflected mirror
  surface. Fingertips should move through the staged scene on the same side the
  camera sees.
- Keep tracked hand pose driven by vision/retargeting; use Rapier for contacts,
  props, and gameplay interactions around the hand.
- Keep prop recovery and grab assist in the reusable interaction controller so
  debug screens and future gameplay use the same reach behavior.
- Keep finger solving typed by `FingerId` so future per-finger limits can be
  added without changing call sites.
- Prefer typed hand-side, rig, and material structs over loose booleans/strings.
- Do not persist calibration here until a real save/profile system owns it.
