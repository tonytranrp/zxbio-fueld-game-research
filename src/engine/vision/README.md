# engine/vision

Optional computer-vision integrations live here. Vision features must be
runtime-safe and build-optional: normal game builds should not fetch Python,
Asio, MediaPipe, or camera dependencies unless the feature flag for that
integration is enabled.

## Current folders

```text
engine/vision/
`-- hand_tracking/
```

## Current module

`hand_tracking/` owns the managed Python worker bridge plus the C++ service for
MediaPipe hand landmarks, gestures, and preview frames. Procedural retargeting
uses those snapshots from `engine/custom/procedural/hand/` and
`engine/custom/procedural/physics/`.

## Coding standards

- Vision services must fail offline without crashing the game.
- Camera access requires explicit in-app session consent.
- Raw detector data should be converted into typed snapshots at the boundary.
- Keep rendering and procedural mapping outside the detector service.
