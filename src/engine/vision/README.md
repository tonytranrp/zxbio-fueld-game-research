# engine/vision

Optional computer-vision integrations live here. Vision features must be
runtime-safe and build-optional: normal game builds should not fetch Python,
Asio, MediaPipe, or camera dependencies unless the feature flag for that
integration is enabled.

Current modules:

- `hand_tracking/`: managed Python worker plus C++ service for MediaPipe hand
  landmarks, gestures, preview frames, and procedural-hand retargeting.
