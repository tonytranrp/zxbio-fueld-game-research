# Biofuel Hand Tracking Worker

Optional managed Python worker for webcam hand tracking. C++ launches this
process only when `BIOFUEL_ENABLE_HAND_TRACKING=ON` and a runtime system asks
for tracking after in-app camera consent.

The worker uses MediaPipe Gesture Recognizer and OpenCV. It sends landmarks and
gestures to C++ as binary UDP snapshots, accepts JSON-line TCP control commands,
and can stream a Dev/Debug-only MJPEG preview over TCP.

Camera startup intentionally emits one empty-hand frame after the camera starts
producing pixels and before MediaPipe finishes warming up. This keeps the C++
service online and the preview visible during model initialization.

The worker keeps camera capture/preview independent from MediaPipe recognition:
preview streams from the newest camera frame while recognition consumes the
newest available frame at its configured max FPS. This prevents the dev camera
panel from dropping to recognition speed on slower machines. `max_fps:
"camera"` means "do not add an artificial cap; emit each new camera frame."

Handedness thresholds intentionally mark low-margin left/right classifications
as `Unknown`. C++ owns identity recovery because it has the previous accepted
frame and can preserve side only when the palm continues nearby.

Pinned dependencies live in `requirements.txt`; CMake provisions Python 3.12
with uv into the build tree so the app does not depend on the machine Python.

## Standards

- Keep protocol packing in `protocol.py`; worker code should not hand-roll
  binary packet offsets.
- Keep frame timestamps monotonic for MediaPipe `VIDEO` mode.
- Always release OpenCV camera handles in `finally` paths.
- Sanitize landmark and score values before they cross into C++.
- Treat ambiguous handedness as unknown instead of forcing a side; downstream
  C++ stabilization should make the final side decision.
- Try configured camera indices/backends in order; on Windows prefer DirectShow
  before the default OpenCV backend to avoid slow MSMF warm-up.
- Keep preview and recognition FPS policy in `default_config.json`; use
  `"camera"` for adaptive camera-rate behavior, or a number for an explicit cap.
- Clamp preview `max_width` and `jpeg_quality` before passing them into OpenCV;
  bad config should drop a frame, not crash the worker.
