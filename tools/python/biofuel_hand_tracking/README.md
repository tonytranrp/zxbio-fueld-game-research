# Biofuel Hand Tracking Worker

Optional managed Python worker for webcam hand tracking. C++ launches this
process only when `BIOFUEL_ENABLE_HAND_TRACKING=ON` and a runtime system asks
for tracking after in-app camera consent.

The worker uses MediaPipe Gesture Recognizer and OpenCV. It sends landmarks and
gestures to C++ as binary UDP snapshots, accepts JSON-line TCP control commands,
and can stream a Dev/Debug-only MJPEG preview over TCP.

Pinned dependencies live in `requirements.txt`; CMake provisions Python 3.12
with uv into the build tree so the app does not depend on the machine Python.

## Standards

- Keep protocol packing in `protocol.py`; worker code should not hand-roll
  binary packet offsets.
- Keep frame timestamps monotonic for MediaPipe `VIDEO` mode.
- Always release OpenCV camera handles in `finally` paths.
- Sanitize landmark and score values before they cross into C++.
