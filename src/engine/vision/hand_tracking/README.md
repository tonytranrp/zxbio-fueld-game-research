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
- Rendering and smoothing remain C++ side; Python only detects hands.

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

## Privacy and failure behavior

- The worker does not open a camera until the game asks for consent and the user
  approves it for the current session.
- Missing Python packages, missing model assets, camera failure, packet timeout,
  or worker exit should leave the game running with tracking offline.
- Preview is intended for Debug/Dev UI only.

## Packet format

- Magic: `BHTK`
- Version: `1`
- Payload: up to 2 hands, handedness, confidence, gesture, 21 image landmarks,
  and 21 world landmarks per hand.
- C++ validates magic, version, packet size, and hand count before accepting a
  snapshot.

## Coding standards

- Keep worker process ownership inside `HandTrackingService`.
- Keep packet structs validated before exposing snapshots.
- Do not put procedural mapping or screen overlay drawing in this folder.
- Feature-guard runtime access with `BIOFUEL_ENABLE_HAND_TRACKING`.
