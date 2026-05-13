# engine/vision/hand_tracking

Optional hand-tracking runtime for live camera input.

## Current contents

```text
engine/vision/hand_tracking/
|-- HandTrackingTypes.hpp
|-- HandTrackingService.hpp/.cpp
|-- HandTrackingServiceModule.hpp
`-- README.md
```

The hand-tracking runtime is optional and disabled by default. Enable it with:

```powershell
cmake -S . -B build-handtracking -DBIOFUEL_ENABLE_HAND_TRACKING=ON
cmake --build build-handtracking --config Debug --parallel
```

For the Visual Studio/CMake debug tree used by this workspace:

```powershell
cmake -S . -B out/build/x64-Debug -DBIOFUEL_ENABLE_HAND_TRACKING=ON
cmake --build out/build/x64-Debug --config Debug --parallel
```

## Runtime shape

- C++ launches a managed Python worker lazily, after in-app camera consent.
- Python uses MediaPipe Gesture Recognizer and OpenCV to detect hands.
- Landmark and gesture snapshots are binary UDP packets on port `40241`.
- C++ sends JSON-line control commands over TCP port `40242`.
- Dev preview frames use a TCP length-prefixed MJPEG stream on port `40243`.
- Packet validation, light landmark smoothing, gesture debouncing, and preview
  thread ownership remain C++ side; Python only detects hands.
- Hand identity stabilization remains C++ side. When MediaPipe reports an
  ambiguous left/right classification, the service matches the palm against the
  previous frame and preserves the previous side for short, nearby continuations.
- Startup allows camera/MediaPipe warm-up before reporting failure. The worker
  sends an early no-hands frame as soon as the camera produces pixels, then
  starts full gesture recognition.
- Preview frames are produced from the live camera capture path, not from the
  recognition loop, so preview FPS can stay smooth even when landmark inference
  is heavier than the camera stream.
- The C++ preview receiver decodes MJPEG on the preview thread and exposes
  ready RGBA frames to screens, keeping JPEG decompression off the render path.

When enabled, CMake fetches standalone Asio through CPM and provisions a
build-local Python 3.12 environment with `uv`. The app never relies on system
Python. The Python worker and pinned requirements live in
`tools/python/biofuel_hand_tracking/`.

## How to use it

```cpp
auto& tracking = biofuel::engine::runtime::Runtime::handTracking();
tracking.requestCameraAccess();
tracking.approveCameraAccess();
tracking.start();

if (auto frame = tracking.latestFrame()) {
    // Feed frame into procedural hand retargeting.
}
```

`HandTrackingFrame` carries up to two hands, typed handedness, gesture data, and
21 image/world landmarks per hand.

Preview consumers should prefer `latestPreviewFrameAfter(lastSequence)` so they
only upload new frames. Treat "no previous preview" as an invalid sequence value,
because the warm-up frame can legitimately use sequence `0`.

## Privacy and failure behavior

- The worker does not open a camera until the game asks for consent and the user
  approves it for the current session.
- Missing Python packages, missing model assets, camera failure, packet timeout,
  or worker exit should leave the game running with tracking offline.
- Preview is intended for Debug/Dev UI only.
- On Windows the worker tries DirectShow before the default OpenCV backend, then
  falls back through configured camera indices/backends.

## Packet format

- Magic: `BHTK`
- Version: `1`
- Payload: up to 2 hands, handedness, confidence, gesture, 21 image landmarks,
  and 21 world landmarks per hand.
- C++ validates magic, version, packet size, enum values, score ranges, and hand
  count before accepting a snapshot.

## Coding standards

- Keep worker process ownership inside `HandTrackingService`.
- Preview networking must be stoppable from the main thread; avoid blocking
  reads that cannot observe the service's running flag.
- Keep packet structs validated before exposing snapshots.
- Keep identity recovery conservative: use previous-side stabilization only for
  nearby, low-confidence handedness changes, not for large jumps across the
  camera frame.
- Do not put procedural mapping or screen overlay drawing in this folder.
- Feature-guard runtime access with `BIOFUEL_ENABLE_HAND_TRACKING`.
