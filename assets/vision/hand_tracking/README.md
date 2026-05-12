# Hand Tracking Assets

`gesture_recognizer.task` is the MediaPipe Gesture Recognizer task bundle used
by the optional Python hand-tracking worker.

- Source: `https://storage.googleapis.com/mediapipe-models/gesture_recognizer/gesture_recognizer/float16/1/gesture_recognizer.task`
- Upstream docs: `https://ai.google.dev/edge/mediapipe/solutions/vision/gesture_recognizer`
- License: MediaPipe is distributed under Apache-2.0. Keep this model bundled
  only if that license remains acceptable for the project.
- Runtime: `tools/python/biofuel_hand_tracking/worker.py`
- Feature gate: `BIOFUEL_ENABLE_HAND_TRACKING`

The worker uses the task only for camera perception. The game renders its own
procedural hands from the returned landmarks and gestures.

Update process:

1. Replace `gesture_recognizer.task` with a model from the official MediaPipe
   model storage or a project-trained Gesture Recognizer export.
2. Run the hand-tracking feature build so the asset is copied to the executable
   output.
3. Smoke test DevHandLab tracking and confirm packet gestures still map to the
   C++ `HandTrackingGesture` enum.
